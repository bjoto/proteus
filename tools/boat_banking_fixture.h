#ifndef PROTEUS_BOAT_BANKING_FIXTURE_H
#define PROTEUS_BOAT_BANKING_FIXTURE_H

#include "boat.h"
#include "raymath.h"

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
// Hold a fast turn for eight seconds, then release steering while keeping power.
// Translate the complete presentation history once on uniform calm water so the
// three-second exit remains clear of basin walls. No velocity/attitude is prescribed.
#define BANKING_EXIT_HOLD_TICKS 960
#define BANKING_EXIT_TICKS (BANKING_EXIT_HOLD_TICKS + 360)
static inline void banking_exit_reset(Boat *boat)
{
    boat_reset(boat);
    boat->position = (Vector2) { 0 };
    boat->velocity.y = -12;
    boat->throttle = 1;
}

static inline void banking_exit_step(Boat *boat, float wheel, int tick)
{
    if (tick == BANKING_EXIT_HOLD_TICKS) {
        Vector2 translation = Vector2Negate(boat->position);
        boat->position = (Vector2) { 0 };
        for (int i = 0; i < BOAT_WATER_PARTICLES; ++i)
            boat->water[i].position = Vector2Add(boat->water[i].position, translation);
    }
    float input
        = tick < BANKING_EXIT_HOLD_TICKS ? Clamp((wheel - boat->wheel) / BOAT_STEP, -1, 1) : 0;
    boat_step(boat, input, 0);
}
#endif
