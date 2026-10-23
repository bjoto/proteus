#ifndef BOAT_FLEET_BOTS_H
#define BOAT_FLEET_BOTS_H
#include "boat_fleet.h"
#include "raymath.h"

// Scripted fleet drivers for validators and boat_server. Every slot laps the crossing rival's
// four basin waypoints, odd slots in the opposite direction, so trails cross and boats meet
// head-on. Two seconds after a capsize a bot asks the fleet for a reset.
typedef struct {
    unsigned waypoint[BOAT_FLEET_SLOTS];
    unsigned capsized[BOAT_FLEET_SLOTS]; // Ticks spent capsized.
    unsigned resets; // Reset requests made.
} BoatFleetBots;

static inline BoatInput boat_fleet_bot_input(
    BoatFleetBots *bots, const BoatFleet *fleet, unsigned slot)
{
    static const Vector2 route[] = { { 60, -12 }, { 0, -55 }, { -60, -12 }, { 0, 45 } };
    const Boat *boat = &fleet->state.slots[slot].boat;
    unsigned lap = bots->waypoint[slot] % 4;
    Vector2 delta = Vector2Subtract(route[slot % 2 ? 3 - lap : lap], boat->position);
    if (Vector2Length(delta) < 16)
        ++bots->waypoint[slot];
    float target = atan2f(delta.x, -delta.y);
    float error = remainderf(target - boat->heading, 2 * PI);
    float wheel = Clamp(2 * error - boat->yaw_velocity, -1, 1);
    float throttle = .65f + .1f * (float)slot;
    return (BoatInput) { Clamp((wheel - boat->wheel) / BOAT_STEP, -1, 1),
        Clamp((throttle - boat->throttle) / (.5f * BOAT_STEP), -1, 1) };
}

// Inputs for every slot, then recovery requests for boats capsized for two seconds.
static inline void boat_fleet_bots_drive(
    BoatFleetBots *bots, BoatFleet *fleet, BoatInput inputs[BOAT_FLEET_SLOTS])
{
    for (unsigned slot = 0; slot < BOAT_FLEET_SLOTS; ++slot) {
        const BoatFleetSlot *s = &fleet->state.slots[slot];
        inputs[slot] = s->active ? boat_fleet_bot_input(bots, fleet, slot) : (BoatInput) { 0 };
        if (!s->active || s->boat.motion_state != BOAT_CAPSIZED) {
            bots->capsized[slot] = 0;
        } else if (++bots->capsized[slot] == 2 * BOAT_TICK_HZ) {
            boat_fleet_request_spawn(fleet, slot);
            bots->waypoint[slot] = 0;
            ++bots->resets;
        }
    }
}

#endif
