#ifndef PROTEUS_BOAT_BANKING_FIXTURE_H
#define PROTEUS_BOAT_BANKING_FIXTURE_H

#include "boat.h"

// Shared free-drive inputs for numerical measurements and presentation captures.
static inline void banking_fixture_reset(Boat *boat, int scenario)
{
    boat_reset(boat);
    boat->position.y = scenario == 5 ? -40 : 40;
    if (scenario == 4 || scenario == 6)
        boat->velocity.y = -12;
}

static inline int banking_fixture_ticks(int scenario)
{
    return scenario == 0 ? 720 : scenario == 5 ? 600 : 960;
}

static inline float banking_fixture_input(Boat *boat, int scenario, int tick)
{
    boat->throttle = scenario == 5 ? -1
        : scenario == 4            ? 0
        : scenario == 6            ? (tick < 240 ? -1 : 0)
                                   : 1;
    return scenario == 1 ? -1
        : scenario == 2  ? 1
        : scenario == 3  ? (tick < 480 ? 1 : -1)
        : scenario == 4  ? .5f
                         : 0;
}
#endif
