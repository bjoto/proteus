#include "boat_wake.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define WAKE_G 9.81

struct BoatWake {
    BoatWakeConfig config;
    size_t count;
    unsigned char *land;
    double *storage, *eta, *psi, *rate, *op, *a, *b, *c;
    double maximum;
    BoatWakeLedger ledger;
};

/* Water-only graph Laplacian L, with no periodic edges. Approximate sqrt(L) by
 * sum_j 4 sqrt(a)/(pi(4j^2-1)) [I-T_j(I-2L/a)], a=8/dx^2.
 * Each term is positive semidefinite and kills constants. Polynomial paths only
 * follow water edges: disconnected components cannot communicate through land. */
static void apply(BoatWake *f, const double *input, double *output)
{
    size_t w = f->config.width, h = f->config.height;
    memcpy(f->a, input, f->count * sizeof(double));
    memcpy(f->c, input, f->count * sizeof(double));
    memset(output, 0, f->count * sizeof(double));
    double scale = 4 * sqrt(8.0) / (3.14159265358979323846 * f->config.spacing);
    for (unsigned j = 1; j <= f->config.order; ++j) {
        for (size_t z = 0; z < h; ++z) {
            for (size_t x = 0; x < w; ++x) {
                size_t i = z * w + x;
                double v = f->a[i];
                if (f->land[i]) {
                    f->b[i] = 0;
                    continue;
                }
                double delta = 0;
                if (x && !f->land[i - 1])
                    delta += f->a[i - 1] - v;
                if (x + 1 < w && !f->land[i + 1])
                    delta += f->a[i + 1] - v;
                if (z && !f->land[i - w])
                    delta += f->a[i - w] - v;
                if (z + 1 < h && !f->land[i + w])
                    delta += f->a[i + w] - v;
                double bv = v + delta / 4;
                f->b[i] = j == 1 ? bv : 2 * bv - f->c[i];
                output[i] += scale / (4.0 * j * j - 1) * (input[i] - f->b[i]);
            }
        }
        double *swap = f->c;
        f->c = f->a;
        f->a = f->b;
        f->b = swap;
    }
}

static double energy(BoatWake *f)
{
    apply(f, f->psi, f->op);
    double total = 0;
    for (size_t i = 0; i < f->count; ++i)
        total += WAKE_G * f->eta[i] * f->eta[i] + f->psi[i] * f->op[i];
    return 0.5 * f->config.density * f->config.spacing * f->config.spacing * total;
}

BoatWake *boat_wake_create(const BoatWakeConfig *c)
{
    if (!c || c->width < 2 || c->height < 2 || c->width > 512 || c->height > 512
        || !isfinite(c->spacing) || c->spacing <= 0 || !isfinite(c->density) || c->density <= 0
        || !isfinite(c->dt) || c->dt <= 0 || !c->order || c->order > 1024
        || c->dt * sqrt(WAKE_G * sqrt(8.0) / c->spacing) >= 1)
        return NULL;
    double energy_scale = 0.5 * c->density * c->spacing * c->spacing;
    if (!isfinite(energy_scale) || energy_scale <= 0 || !isfinite(sqrt(8.0) / c->spacing))
        return NULL;
    BoatWake *f = calloc(1, sizeof(*f));
    if (!f)
        return NULL;
    f->config = *c;
    f->count = c->width * c->height;
    /* Conservative headroom for source sums, Hamiltonian products and time stepping. */
    f->maximum = 0.001
        * sqrt(fmin(DBL_MAX / 1e6,
            DBL_MAX / f->count / energy_scale / fmax(WAKE_G, 2 * sqrt(8.0) / c->spacing)));
    f->land = calloc(f->count, 1);
    f->storage = calloc(7 * f->count, sizeof(double));
    if (!f->land || !f->storage) {
        boat_wake_destroy(f);
        return NULL;
    }
    f->eta = f->storage;
    f->psi = f->eta + f->count;
    f->rate = f->psi + f->count;
    f->op = f->rate + f->count;
    f->a = f->op + f->count;
    f->b = f->a + f->count;
    f->c = f->b + f->count;
    if (c->land)
        memcpy(f->land, c->land, f->count);
    for (size_t i = 0; i < f->count; ++i) {
        double rate = c->absorption ? c->absorption[i] : 0;
        if (!isfinite(rate) || rate < 0) {
            boat_wake_destroy(f);
            return NULL;
        }
        f->rate[i] = rate;
    }
    f->config.land = f->land;
    f->config.absorption = f->rate;
    return f;
}

BoatWake *boat_wake_clone(const BoatWake *f)
{
    if (!f)
        return NULL;
    BoatWake *copy = boat_wake_create(&f->config);
    if (copy) {
        memcpy(copy->eta, f->eta, 2 * f->count * sizeof(double));
        copy->ledger = f->ledger;
    }
    return copy;
}

void boat_wake_destroy(BoatWake *f)
{
    if (f) {
        free(f->storage);
        free(f->land);
        free(f);
    }
}

void boat_wake_reset(BoatWake *f)
{
    if (!f)
        return;
    f->ledger.retired += energy(f);
    memset(f->eta, 0, 2 * f->count * sizeof(double));
    f->ledger.tick = 0;
}

bool boat_wake_kick(BoatWake *f, const double *impulse)
{
    if (!f || !impulse)
        return false;
    for (size_t i = 0; i < f->count; ++i)
        if (!isfinite(impulse[i]) || fabs(impulse[i]) > f->maximum
            || fabs(f->psi[i] + impulse[i]) > f->maximum || (f->land[i] && impulse[i] != 0))
            return false;
    double before = energy(f);
    for (size_t i = 0; i < f->count; ++i)
        f->psi[i] += impulse[i];
    double work = energy(f) - before;
    f->ledger.injected += work;
    if (work >= 0)
        f->ledger.positive_work += work;
    else
        f->ledger.negative_work += work;
    return true;
}

bool boat_wake_step(BoatWake *f)
{
    if (!f || f->ledger.tick == UINT64_MAX)
        return false;
    double dt = f->config.dt, before = energy(f);
    /* Kick-drift-kick: fixed integer tick, no absolute float phase. */
    for (size_t i = 0; i < f->count; ++i)
        f->psi[i] -= 0.5 * dt * WAKE_G * f->eta[i];
    apply(f, f->psi, f->op);
    for (size_t i = 0; i < f->count; ++i) {
        f->eta[i] += dt * f->op[i];
        f->psi[i] -= 0.5 * dt * WAKE_G * f->eta[i];
    }
    f->ledger.numerical_residual += energy(f) - before;
    /* Damping eta alone is a provably nonnegative Hamiltonian sink, even near land.
     * A spatial mask on psi could increase its nonlocal kinetic energy. */
    double lost = 0;
    for (size_t i = 0; i < f->count; ++i) {
        double old = f->eta[i];
        f->eta[i] *= exp(-f->rate[i] * dt);
        lost += old * old * -expm1(-2 * f->rate[i] * dt);
    }
    f->ledger.absorbed
        += 0.5 * f->config.density * WAKE_G * f->config.spacing * f->config.spacing * lost;
    ++f->ledger.tick;
    return true;
}

bool boat_wake_sample(const BoatWake *f, size_t x, size_t z, double *eta, double *psi)
{
    if (!f || x >= f->config.width || z >= f->config.height || !eta || !psi)
        return false;
    *eta = f->eta[z * f->config.width + x];
    *psi = f->psi[z * f->config.width + x];
    return true;
}

BoatWakeLedger boat_wake_ledger(BoatWake *f)
{
    if (!f)
        return (BoatWakeLedger) { 0 };
    BoatWakeLedger result = f->ledger;
    result.retained = energy(f);
    return result;
}

double boat_wake_symbol(unsigned order, double spacing, double kx, double kz)
{
    double b = 1 - (4 - 2 * cos(kx * spacing) - 2 * cos(kz * spacing)) / 4;
    double previous = 1, current = b, sum = 0;
    for (unsigned j = 1; j <= order; ++j) {
        sum += (1 - current) / (4.0 * j * j - 1);
        double next = 2 * b * current - previous;
        previous = current;
        current = next;
    }
    return 4 * sqrt(8.0) / (3.14159265358979323846 * spacing) * sum;
}
