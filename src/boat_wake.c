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
    double bound, weights[4];
    unsigned radius;
    uint32_t *neighbors;
    BoatWakeLedger ledger;
};

/* Water-only graph Laplacian L, with no periodic edges. Approximate sqrt(L) by
 * sum_j 4 sqrt(a)/(pi(4j^2-1)) [I-T_j(I-2L/a)], a bounds the chosen stencil.
 * Close the omitted tail with a normalized Fejer kernel: add C(1-F_p(B)),
 * C=2 sqrt(a)/(pi(2p+1)). Its extra positive weights vanish at the constant mode.
 * Each term is positive semidefinite and kills constants. Polynomial paths only
 * follow water edges: disconnected components cannot communicate through land. */
static void apply(BoatWake *f, const double *input, double *output)
{
    size_t w = f->config.width, h = f->config.height;
    memcpy(f->a, input, f->count * sizeof(double));
    memcpy(f->c, input, f->count * sizeof(double));
    memset(output, 0, f->count * sizeof(double));
    double scale = 4 * sqrt(f->bound) / (3.14159265358979323846 * f->config.spacing);
    double tail = scale / (2 * (2.0 * f->config.order + 1));
    for (unsigned j = 1; j <= f->config.order; ++j) {
        double fejer
            = 2.0 * (f->config.order + 1 - j) / ((f->config.order + 1.0) * (f->config.order + 1.0));
        double weight = scale / (4.0 * j * j - 1) + tail * fejer;
        for (size_t z = 0; z < h; ++z) {
            for (size_t x = 0; x < w; ++x) {
                size_t i = z * w + x;
                double v = f->a[i];
                if (f->land[i]) {
                    f->b[i] = 0;
                    continue;
                }
                double lap = 0;
                if (f->radius == 1) {
                    if (x && !f->land[i - 1])
                        lap += v - f->a[i - 1];
                    if (x + 1 < w && !f->land[i + 1])
                        lap += v - f->a[i + 1];
                    if (z && !f->land[i - w])
                        lap += v - f->a[i - w];
                    if (z + 1 < h && !f->land[i + w])
                        lap += v - f->a[i + w];
                } else {
                    for (unsigned r = 0; r < f->radius; ++r) {
                        const uint32_t *neighbors = f->neighbors + (i * f->radius + r) * 4;
                        double sum = 0;
                        for (unsigned d = 0; d < 4; ++d)
                            sum += v - f->a[neighbors[d]];
                        lap += f->weights[r] * sum;
                    }
                }
                double bv = v - 2 * lap / f->bound;
                f->b[i] = j == 1 ? bv : 2 * bv - f->c[i];
                output[i] += weight * (input[i] - f->b[i]);
            }
        }
        double *swap = f->c;
        f->c = f->a;
        f->a = f->b;
        f->b = swap;
    }
}

static unsigned spatial_weights(unsigned order, double weights[4])
{
    if (order == 8) {
        weights[0] = 8.0 / 5;
        weights[1] = -1.0 / 5;
        weights[2] = 8.0 / 315;
        weights[3] = -1.0 / 560;
        return 4;
    }
    if (order == 4) {
        weights[0] = 4.0 / 3;
        weights[1] = -1.0 / 12;
        return 2;
    }
    weights[0] = 1;
    return 1;
}

/* Mirror within each contiguous water segment, never across its land endpoints.
 * These stencils equal positive powers of that segment's Neumann graph Laplacian. */
static void build_neighbors(BoatWake *f)
{
    for (unsigned axis = 0; axis < 2; ++axis) {
        size_t length = axis ? f->config.height : f->config.width;
        size_t lines = axis ? f->config.width : f->config.height;
        size_t stride = axis ? f->config.width : 1;
        for (size_t line = 0; line < lines; ++line) {
            size_t base = axis ? line : line * f->config.width;
            size_t start = 0;
            while (start < length) {
                if (f->land[base + start * stride]) {
                    ++start;
                    continue;
                }
                size_t end = start + 1;
                while (end < length && !f->land[base + end * stride])
                    ++end;
                int count = (int)(end - start);
                for (size_t pos = start; pos < end; ++pos) {
                    size_t i = base + pos * stride;
                    for (unsigned r = 0; r < f->radius; ++r) {
                        for (unsigned sign = 0; sign < 2; ++sign) {
                            int offset = (int)(pos - start) + (sign ? 1 : -1) * (int)(r + 1);
                            offset = ((offset % (2 * count)) + 2 * count) % (2 * count);
                            if (offset >= count)
                                offset = 2 * count - 1 - offset;
                            f->neighbors[(i * f->radius + r) * 4 + 2 * axis + sign]
                                = (uint32_t)(base + (start + (size_t)offset) * stride);
                        }
                    }
                }
                start = end;
            }
        }
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
        || (c->spatial_order && c->spatial_order != 2 && c->spatial_order != 4
            && c->spatial_order != 8))
        return NULL;
    double weights[4] = { 0 };
    unsigned radius = spatial_weights(c->spatial_order, weights);
    double bound = 0;
    for (unsigned r = 0; r < radius; r += 2)
        bound += 8 * weights[r];
    if (c->dt * sqrt(WAKE_G * sqrt(bound) / c->spacing) >= 1)
        return NULL;
    double energy_scale = 0.5 * c->density * c->spacing * c->spacing;
    if (!isfinite(energy_scale) || energy_scale <= 0 || !isfinite(sqrt(8.0) / c->spacing))
        return NULL;
    BoatWake *f = calloc(1, sizeof(*f));
    if (!f)
        return NULL;
    f->config = *c;
    f->count = c->width * c->height;
    f->radius = radius;
    f->bound = bound;
    memcpy(f->weights, weights, sizeof(weights));
    /* Conservative headroom for source sums, Hamiltonian products and time stepping. */
    f->maximum = 0.001
        * sqrt(fmin(DBL_MAX / 1e6,
            DBL_MAX / f->count / energy_scale / fmax(WAKE_G, 2 * sqrt(bound) / c->spacing)));
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
    if (radius > 1) {
        f->neighbors = calloc(f->count * radius * 4, sizeof(*f->neighbors));
        if (!f->neighbors) {
            boat_wake_destroy(f);
            return NULL;
        }
        build_neighbors(f);
    }
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
        free(f->neighbors);
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
    return boat_wake_operator_symbol(order, 2, spacing, kx, kz);
}

double boat_wake_operator_symbol(
    unsigned order, unsigned spatial_order, double spacing, double kx, double kz)
{
    double weights[4] = { 0 }, bound = 0, lap = 0;
    unsigned radius = spatial_weights(spatial_order, weights);
    for (unsigned r = 0; r < radius; ++r) {
        lap += 2 * weights[r] * (2 - cos((r + 1) * kx * spacing) - cos((r + 1) * kz * spacing));
        if (!(r % 2))
            bound += 8 * weights[r];
    }
    double b = 1 - 2 * lap / bound;
    double previous = 1, current = b, sum = 0;
    double tail = 1 / (2 * (2.0 * order + 1));
    for (unsigned j = 1; j <= order; ++j) {
        double fejer = 2.0 * (order + 1 - j) / ((order + 1.0) * (order + 1.0));
        sum += (1 - current) * (1 / (4.0 * j * j - 1) + tail * fejer);
        double next = 2 * b * current - previous;
        previous = current;
        current = next;
    }
    return 4 * sqrt(bound) / (3.14159265358979323846 * spacing) * sum;
}
