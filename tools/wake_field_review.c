/* W2.1b: independently reconstruct modal packets; compare bounded C domains. */
#include "boat_wake.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PI 3.14159265358979323846
#define G 9.81
#define REQUIRE(x)                                                                                 \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x);                                   \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
#define MW 48
#define MH 32

static double graph_symbol(unsigned spatial, double dx, double kx, double kz)
{
    /* Independent axis polynomial, not the mirrored-stencil coefficients. */
    double result = 0;
    for (unsigned axis = 0; axis < 2; ++axis) {
        double s = 4 * pow(sin(0.5 * dx * (axis ? kz : kx)), 2);
        double corrected = s;
        if (spatial >= 4)
            corrected += s * s / 12;
        if (spatial >= 8)
            corrected += s * s * s / 90 + s * s * s * s / 560;
        result += corrected / (dx * dx);
    }
    return sqrt(result);
}

static void manufactured(void)
{
    for (unsigned spatial = 2; spatial <= 8; spatial *= 2) {
        for (unsigned mode = 0; mode < 3; ++mode) {
            /* A one-cell-wide wet segment also exercises multiple mirror bounces. */
            size_t w = mode == 2 ? 3 : 20, h = 16, n = w * h;
            double *kick = calloc(n, sizeof(double));
            unsigned char *land = calloc(n, 1);
            REQUIRE(kick && land);
            double kx = mode == 2 ? 0 : (mode == 1 ? 7 : 4) * PI / (w * .75);
            double kz = mode == 1 ? 0 : 3 * PI / (h * .75);
            for (size_t z = 0; z < h; ++z)
                for (size_t x = 0; x < w; ++x) {
                    size_t i = z * w + x;
                    land[i] = mode == 2 && x != 1;
                    kick[i] = land[i] ? 0 : cos(kx * (x + .5) * .75) * cos(kz * (z + .5) * .75);
                }
            BoatWakeConfig c = { w, h, .75, 1.0 / 120, 1000, 128, land, NULL, spatial };
            BoatWake *f = boat_wake_create(&c);
            REQUIRE(f && boat_wake_kick(f, kick));
            double symbol = boat_wake_operator_symbol(128, spatial, .75, kx, kz);
            REQUIRE(fabs(symbol / graph_symbol(spatial, .75, kx, kz) - 1) < .005);
            /* One kick-drift-kick step has eta=dt A psi_initial exactly. */
            REQUIRE(boat_wake_step(f));
            for (size_t z = 0; z < h; ++z)
                for (size_t x = 0; x < w; ++x) {
                    double eta, psi;
                    REQUIRE(boat_wake_sample(f, x, z, &eta, &psi));
                    REQUIRE(fabs(eta - c.dt * symbol * kick[z * w + x]) < 2e-13);
                    REQUIRE(
                        fabs(psi - (1 - .5 * c.dt * c.dt * G * symbol) * kick[z * w + x]) < 2e-13);
                }
            BoatWake *copy = boat_wake_clone(f);
            REQUIRE(copy);
            REQUIRE(boat_wake_step(f) && boat_wake_step(copy));
            REQUIRE(boat_wake_ledger(f).retained == boat_wake_ledger(copy).retained);
            for (unsigned t = 2; t < 240; ++t)
                REQUIRE(boat_wake_step(f));
            double theta = 2 * asin(.5 * c.dt * sqrt(G * symbol));
            double height_scale = c.dt * symbol * sin(240 * theta) / sin(theta);
            for (size_t z = 0; z < h; ++z)
                for (size_t x = 0; x < w; ++x) {
                    double eta, psi;
                    REQUIRE(boat_wake_sample(f, x, z, &eta, &psi));
                    REQUIRE(fabs(eta - height_scale * kick[z * w + x]) < 1e-11);
                    REQUIRE(fabs(psi - cos(240 * theta) * kick[z * w + x]) < 1e-11);
                }
            boat_wake_destroy(copy);
            boat_wake_destroy(f);
            free(kick);
            free(land);
        }
    }
}

static void irregular_geometry(void)
{
    enum { W = 13, H = 11, N = W * H };
    unsigned char land[N];
    double p[N], q[N];
    for (size_t z = 0; z < H; ++z)
        for (size_t x = 0; x < W; ++x) {
            size_t i = z * W + x;
            land[i] = (x == 6 && z != 4) || (x >= 9 && z >= 7) || (x == 2 && z < 3);
            p[i] = land[i] ? 0 : sin(.31 * i) + cos(.73 * i);
            q[i] = land[i] ? 0 : cos(.41 * i) - sin(.57 * i);
        }
    BoatWakeConfig c = { W, H, .75, .025, 1000, 32, land, NULL, 8 };
    BoatWake *a = boat_wake_create(&c), *b = boat_wake_create(&c);
    REQUIRE(a && b && boat_wake_kick(a, p) && boat_wake_kick(b, q));
    REQUIRE(boat_wake_step(a) && boat_wake_step(b));
    double cross_a = 0, cross_b = 0, positive = 0, mean = 0;
    for (size_t z = 0; z < H; ++z)
        for (size_t x = 0; x < W; ++x) {
            size_t i = z * W + x;
            double ap, aq, psi;
            REQUIRE(boat_wake_sample(a, x, z, &ap, &psi));
            REQUIRE(boat_wake_sample(b, x, z, &aq, &psi));
            ap /= c.dt;
            aq /= c.dt;
            cross_a += p[i] * aq;
            cross_b += q[i] * ap;
            positive += p[i] * ap;
            mean += ap;
            if (land[i])
                REQUIRE(ap == 0 && aq == 0);
        }
    REQUIRE(fabs(cross_a - cross_b) < 1e-11 * fmax(1, fabs(cross_a)));
    REQUIRE(positive > 0 && fabs(mean) < 1e-11);
    boat_wake_destroy(a);
    boat_wake_destroy(b);
    for (size_t i = 0; i < N; ++i)
        q[i] = land[i] ? 0 : 1;
    a = boat_wake_create(&c);
    REQUIRE(a && boat_wake_kick(a, q) && boat_wake_step(a));
    REQUIRE(boat_wake_ledger(a).retained == 0);
    boat_wake_destroy(a);
    double errors[2] = { 0 };
    for (unsigned r = 0; r < 2; ++r) {
        c.dt = .025 / (r + 1);
        a = boat_wake_create(&c);
        REQUIRE(a && boat_wake_kick(a, p));
        double initial = boat_wake_ledger(a).retained;
        for (unsigned t = 0; t < 400 * (r + 1); ++t) {
            REQUIRE(boat_wake_step(a));
            errors[r] = fmax(errors[r], fabs(boat_wake_ledger(a).retained / initial - 1));
        }
        boat_wake_destroy(a);
    }
    REQUIRE(errors[0] < .01 && errors[1] < .3 * errors[0]);
}

typedef struct {
    double kx, kz, coefficient;
} Mode;
static Mode modes[49 * 33];
static size_t mode_count;

static void packet_modes(void)
{
    /* Exact Neumann cosine coefficients of reflected Gaussian-carrier images.
     * psi is a 6 m bidirectional carrier at (-18,0), sigma_x/z=5/6 m.
     * The same continuous coefficients are used at every grid/order. */
    for (unsigned z = 0; z <= 32; ++z)
        for (unsigned x = 0; x <= 48; ++x) {
            double kx = PI * x / 72, kz = PI * z / 48, carrier = 2 * PI / 6;
            double cx = sqrt(2 * PI) * 5 / 72 * (x ? 2 : 1) * .5
                * (exp(-12.5 * pow(kx - carrier, 2)) + exp(-12.5 * pow(kx + carrier, 2)))
                * cos(kx * 18);
            double cz = sqrt(2 * PI) * 6 / 48 * (z ? 2 : 1) * exp(-18 * kz * kz) * cos(kz * 24);
            modes[mode_count++] = (Mode) { kx, kz, cx * cz };
        }
}

static void modal_field(double *out, double dx, unsigned order, unsigned spatial, double dt,
    double time, bool continuum)
{
    memset(out, 0, MW * MH * sizeof(double));
    for (size_t j = 0; j < mode_count; ++j) {
        Mode m = modes[j];
        double k = hypot(m.kx, m.kz);
        if (!k)
            continue;
        double a = continuum ? k : boat_wake_operator_symbol(order, spatial, dx, m.kx, m.kz);
        double amplitude;
        if (continuum)
            amplitude = a / sqrt(G * a) * sin(sqrt(G * a) * time);
        else {
            double theta = 2 * asin(.5 * dt * sqrt(G * a));
            amplitude = dt * a * sin(round(time / dt) * theta) / sin(theta);
        }
        amplitude *= m.coefficient;
        for (size_t z = 0; z < MH; ++z) {
            double row = amplitude * cos(m.kz * ((z + .5) * 48 / MH));
            for (size_t x = 0; x < MW; ++x)
                out[z * MW + x] += row * cos(m.kx * ((x + .5) * 72 / MW));
        }
    }
}

static double relative_l2(const double *a, const double *b)
{
    double error = 0, reference = 0;
    for (size_t i = 0; i < MW * MH; ++i) {
        error += pow(a[i] - b[i], 2);
        reference += b[i] * b[i];
    }
    return sqrt(error / reference);
}

static void modal_review(void)
{
    packet_modes();
    double reference[MW * MH], candidate[MW * MH], fine[MW * MH];
    puts("  \"modal_packet\": [");
    bool first = true;
    double times[] = { 16, 76 };
    for (unsigned t = 0; t < 2; ++t) {
        modal_field(reference, 0, 0, 0, 0, times[t], true);
        modal_field(fine, .375, 1024, 8, 1.0 / 120, times[t], false);
        double spacings[] = { 1.5, .75, .375, .375 };
        unsigned orders[] = { 128, 256, 512, 1024 };
        for (unsigned i = 0; i < 4; ++i) {
            modal_field(candidate, spacings[i], orders[i], 8, 1.0 / 120, times[t], false);
            double error = relative_l2(candidate, reference), grid = relative_l2(candidate, fine);
            printf("%s    {\"time\": %.0f, \"spacing\": %.3f, \"order\": %u, "
                   "\"continuum_eta_l2\": %.9g, \"fine_eta_l2\": %.9g}",
                first ? "" : ",\n", times[t], spacings[i], orders[i], error, grid);
            first = false;
            if (i == 1)
                REQUIRE(error < .01 && grid < .01);
        }
    }
    puts("\n  ],");
}

static void costs(void)
{
    puts("  \"candidate_costs\": [");
    for (unsigned size = 96; size <= 192; size *= 2) {
        BoatWakeConfig c = { size, size * 2 / 3, .75, 1.0 / 120, 1000, 256, NULL, NULL, 8 };
        BoatWake *f = boat_wake_create(&c);
        REQUIRE(f);
        clock_t begin = clock();
        for (unsigned t = 0; t < 20; ++t)
            REQUIRE(boat_wake_step(f));
        printf("    {\"width_m\": %.0f, \"height_m\": %.0f, \"ms_per_step\": %.6f, "
               "\"owned_payload_bytes\": %zu}%s\n",
            c.width * c.spacing, c.height * c.spacing,
            1000.0 * (clock() - begin) / CLOCKS_PER_SEC / 20, c.width * c.height * 121,
            size == 192 ? "" : ",");
        boat_wake_destroy(f);
    }
    puts("  ]");
}

typedef struct {
    BoatWake *field;
    size_t w, h, shift_x, shift_z;
    unsigned kind;
    double initial, normalization, early, late, returning;
} Domain;

static Domain domain(unsigned scale, unsigned kind, unsigned spatial, unsigned order)
{
    Domain d = { 0 };
    d.w = MW * scale;
    d.h = MH * scale;
    d.shift_x = (d.w - MW) / 2;
    d.shift_z = (d.h - MH) / 2;
    d.kind = kind;
    size_t n = d.w * d.h;
    double *rates = calloc(n, sizeof(double)), *kick = calloc(n, sizeof(double));
    unsigned char *land = calloc(n, 1);
    REQUIRE(rates && kick && land);
    for (size_t z = 0; z < d.h; ++z)
        for (size_t x = 0; x < d.w; ++x) {
            size_t i = z * d.w + x;
            double wx = ((double)x + .5) * 1.5 - d.w * .75, wz = ((double)z + .5) * 1.5 - d.h * .75;
            /* Finite irregular island, narrow waist, and a full one-cell barrier control. */
            double half_width = fabs(wz) < 4.5 ? .8 : 2.3;
            land[i] = kind == 2 ? fabs(wx - .75) < .1
                                : kind == 1 && fabs(wx) < half_width && fabs(wz) < 13.5;
            double edge = fmin(fmin(x, d.w - 1 - x), fmin(z, d.h - 1 - z)) * 1.5;
            rates[i] = edge < 10.5 ? 2 * pow((10.5 - edge) / 10.5, 2) : 0;
            double sx = wx + 18, radius = hypot(sx, wz);
            double envelope = 1;
            if (radius > 7) {
                double u = fmin(1, (radius - 7) / 2);
                envelope = 1 - u * u * u * (10 + u * (-15 + 6 * u));
            }
            kick[i] = land[i] ? 0 : sx * exp(-(sx * sx + wz * wz) / 18) * envelope;
            d.normalization += kick[i] * kick[i] * (2 * PI / 6) / G;
        }
    for (size_t z = 0; z < d.h; ++z)
        for (size_t x = 0; x < d.w; ++x) {
            if (land[z * d.w + x])
                continue;
            for (int dz = -2; dz <= 2; ++dz)
                for (int dx = -2; dx <= 2; ++dx) {
                    int nx = (int)x + dx, nz = (int)z + dz;
                    if (nx >= 0 && nz >= 0 && nx < (int)d.w && nz < (int)d.h
                        && land[(size_t)nz * d.w + (size_t)nx])
                        rates[z * d.w + x] = fmax(rates[z * d.w + x], 1);
                }
        }
    BoatWakeConfig c = { d.w, d.h, 1.5, .05, 1000, order, land, rates, spatial };
    d.field = boat_wake_create(&c);
    REQUIRE(d.field && boat_wake_kick(d.field, kick));
    d.initial = boat_wake_ledger(d.field).retained;
    free(rates);
    free(kick);
    free(land);
    return d;
}

static double sample(Domain *d, size_t x, size_t z)
{
    double eta, psi;
    REQUIRE(boat_wake_sample(d->field, x + d->shift_x, z + d->shift_z, &eta, &psi));
    return eta;
}

static double difference(Domain *a, Domain *b)
{
    double delta = 0;
    /* Fixed common central region: x +/-12m, z +/-12m. */
    for (size_t z = 8; z < 24; ++z)
        for (size_t x = 16; x < 32; ++x)
            delta += pow(sample(a, x, z) - sample(b, x, z), 2);
    return sqrt(delta / a->normalization);
}

static void image_row(FILE *out, Domain *domains)
{
    if (!out)
        return;
    for (size_t z = 0; z < MH; ++z)
        for (unsigned d = 0; d < 6; ++d)
            for (size_t x = 0; x < MW; ++x) {
                double eta = sample(&domains[d], x, z), wx = ((double)x + .5) * 1.5 - 36,
                       wz = ((double)z + .5) * 1.5 - 24;
                double v = fmax(-1, fmin(1, eta / .03));
                unsigned char rgb[3] = { (unsigned char)(128 + 127 * v),
                    (unsigned char)(128 - 100 * fabs(v)), (unsigned char)(128 - 127 * v) };
                bool land = domains[d].kind == 2 ? fabs(wx - .75) < .1
                                                 : domains[d].kind == 1
                        && fabs(wx) < (fabs(wz) < 4.5 ? .8 : 2.3) && fabs(wz) < 13.5;
                if (land)
                    rgb[0] = rgb[1] = rgb[2] = 20;
                fwrite(rgb, 1, 3, out);
            }
}

static bool domain_review(const char *path, unsigned base_scale)
{
    Domain domains[6];
    for (unsigned d = 0; d < 6; ++d)
        domains[d] = domain(base_scale * (d < 3 ? 1 : 2), d % 3, 8, 64);
    FILE *out = path ? fopen(path, "wb") : NULL;
    if (path)
        REQUIRE(out);
    if (out)
        fprintf(out, "P6\n%d %d\n255\n", 6 * MW, 5 * MH);
    double differences[3][2] = { { 0 } };
    clock_t begin = clock();
    for (unsigned t = 1; t <= 1600; ++t) {
        for (unsigned d = 0; d < 6; ++d) {
            REQUIRE(boat_wake_step(domains[d].field));
            double intensity = 0;
            for (size_t z = 12; z <= 20; ++z)
                for (size_t x = 30; x <= 36; ++x)
                    intensity += pow(sample(&domains[d], x, z), 2);
            if (d % 3 == 2)
                REQUIRE(intensity == 0);
            double *peak = t <= 240 ? &domains[d].early : &domains[d].late;
            *peak = fmax(*peak, intensity);
            if (t >= 800)
                domains[d].returning = fmax(domains[d].returning, intensity);
        }
        if (t == 240 || t == 800 || t == 1600) {
            unsigned row = t == 240 ? 0 : t == 800 ? 1 : 2;
            for (unsigned kind = 0; kind < 2; ++kind)
                differences[row][kind] = difference(&domains[kind], &domains[kind + 3]);
        }
        if (t == 80 || t == 160 || t == 240 || t == 800 || t == 1600)
            image_row(out, domains);
    }
    if (out)
        fclose(out);
    printf("  \"domain_cpu_seconds\": %.6f,\n", (double)(clock() - begin) / CLOCKS_PER_SEC);
    puts("  \"domains\": [");
    for (unsigned d = 0; d < 6; ++d) {
        Domain *v = &domains[d];
        BoatWakeLedger l = boat_wake_ledger(v->field);
        double closure = l.retained + l.absorbed + l.retired - l.injected - l.numerical_residual;
        REQUIRE(fabs(closure) < 1e-9 * v->initial);
        printf("    {\"width_m\": %.0f, \"height_m\": %.0f, \"kind\": %u, "
               "\"early_shadow\": %.9g, \"late_shadow\": %.9g, \"return_shadow\": %.9g, "
               "\"retained_fraction\": %.9g, \"absorbed_fraction\": %.9g, \"residual_fraction\": "
               "%.9g}%s\n",
            v->w * 1.5, v->h * 1.5, v->kind, v->early, v->late, v->returning,
            l.retained / v->initial, l.absorbed / v->initial, l.numerical_residual / v->initial,
            d == 5 ? "" : ",");
    }
    puts("  ],\n  \"domain_roi_difference\": [");
    bool passed = true;
    for (unsigned t = 0; t < 3; ++t) {
        printf("    {\"time\": %u, \"open\": %.9g, \"island\": %.9g}%s\n",
            t == 0       ? 12
                : t == 1 ? 40
                         : 80,
            differences[t][0], differences[t][1], t == 2 ? "" : ",");
        if (differences[t][0] >= .01 || differences[t][1] >= .01)
            passed = false;
    }
    puts("  ]");
    for (unsigned d = 0; d < 6; ++d) {
        if (d % 3 == 1)
            REQUIRE(domains[d].early < domains[d - 1].early * .01
                && domains[d].late < domains[d - 1].late * .01);
        /* Larger reference may legitimately still contain outgoing energy at t=80. */
        if (d < 3 && boat_wake_ledger(domains[d].field).retained >= domains[d].initial * .01)
            passed = false;
        boat_wake_destroy(domains[d].field);
    }
    return passed;
}

int main(int argc, char **argv)
{
    manufactured();
    irregular_geometry();
    if (argc > 1 && strcmp(argv[1], "--check") == 0) {
        modal_review();
        /* Thin full barrier must remain exact even with radius-four stencils. */
        Domain wall = domain(1, 2, 8, 32);
        for (unsigned t = 0; t < 20; ++t)
            REQUIRE(boat_wake_step(wall.field));
        for (size_t z = 0; z < MH; ++z)
            for (size_t x = 25; x < MW; ++x)
                REQUIRE(sample(&wall, x, z) == 0);
        boat_wake_destroy(wall.field);
        puts("higher-order manufactured modes, clone and thin barrier passed");
        return 0;
    }
    puts("{");
    modal_review();
    costs();
    if (argc > 1 && strcmp(argv[1], "--accuracy") == 0) {
        puts("}");
        return 0;
    }
    puts(",");
    bool small = argc > 1 && strcmp(argv[1], "--small") == 0;
    bool passed = domain_review(argc > 1 && !small ? argv[1] : NULL, small ? 1 : 2);
    printf(",\n  \"domain_gates_passed\": %s\n", passed ? "true" : "false");
    puts("}");
    return passed ? 0 : 1;
}
