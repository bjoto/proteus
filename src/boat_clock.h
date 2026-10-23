#ifndef PROTEUS_BOAT_CLOCK_H
#define PROTEUS_BOAT_CLOCK_H

#include <stdint.h>

// The float integrator step remains BOAT_STEP. Live time is derived from this integer clock.
#define BOAT_TICK_HZ 120
#define BOAT_TICK_SECONDS (1.0 / BOAT_TICK_HZ)

static inline double boat_tick_seconds(uint64_t tick) { return (double)tick / BOAT_TICK_HZ; }

#endif
