#ifndef PROTEUS_BOAT_WAKE_H
#define PROTEUS_BOAT_WAKE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Isolated W2 field. Never copy an owner by value; clone creates an independent snapshot.
 * Units: metres, seconds, density kg/m^3, energy joules. No BoatWater integration yet. */
typedef struct BoatWake BoatWake;
typedef struct {
    size_t width, height;
    double spacing, dt, density;
    unsigned order; /* Chebyshev square-root order; controls long-wave approximation. */
    const unsigned char *land; /* Optional row-major mask, copied at creation. */
    const double *absorption; /* Optional eta damping rates in 1/s, copied. */
    unsigned spatial_order; /* 0/2: cardinal graph; 4/8: powers of water-only axis graphs. */
    bool reference_operator; /* Diagnostic scalar oracle; copied with snapshots. */
} BoatWakeConfig;
typedef struct {
    double injected, positive_work, negative_work;
    double absorbed, retired, numerical_residual, retained;
    uint64_t tick;
} BoatWakeLedger;

BoatWake *boat_wake_create(const BoatWakeConfig *config);
BoatWake *boat_wake_clone(const BoatWake *field);
void boat_wake_destroy(BoatWake *field);
/* Retires all remaining energy explicitly; preserves cumulative ledger, restarts tick. */
void boat_wake_reset(BoatWake *field);
bool boat_wake_step(BoatWake *field);
/* One fixed step with a prescribed interval impulse split symmetrically around it.
 * Reuses A impulse for both half kicks; callers supply midpoint quadrature of the
 * interval's total source. Each half's signed work is accounted separately.
 * Invalid impulses are rejected before mutation, as with boat_wake_kick(). */
bool boat_wake_step_with_impulse(BoatWake *field, const double *potential_impulse);
/* Add prescribed potential impulse (m^2/s). Signed Hamiltonian change is source work.
 * Sum simultaneous emitters into one array to pay for one operator. The ledger then
 * classifies the net event's signed work, including interference, not each emitter. */
bool boat_wake_kick(BoatWake *field, const double *potential_impulse);
bool boat_wake_sample(const BoatWake *field, size_t x, size_t z, double *eta, double *psi);
/* Audits the reconstructed Hamiltonian; residual is measured from independent work/sinks. */
BoatWakeLedger boat_wake_ledger(BoatWake *field);
/* Exact requested allocation bytes, including owner; allocator overhead is excluded. */
size_t boat_wake_storage_bytes(const BoatWake *field);
/* Discrete |D| symbol on unobstructed water, useful for independent dispersion checks. */
double boat_wake_symbol(unsigned order, double spacing, double kx, double kz);
double boat_wake_operator_symbol(
    unsigned order, unsigned spatial_order, double spacing, double kx, double kz);

#endif
