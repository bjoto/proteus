#ifndef PROTEUS_BOAT_SESSION_H
#define PROTEUS_BOAT_SESSION_H

#include "boat_presentation.h"

// Minimal single-player owner: one world, one boat with its presentation and the only live
// clock. Course and replay state still live on Boat until S3. Assignment clones the session.
typedef struct {
    BoatWorld world;
    Boat boat;
    // Cosmetic follower stepped after each physics tick; physics never reads it. Disable it
    // for simulation-only runs; the physical trace is identical either way.
    BoatPresentation presentation;
    bool presentation_enabled;
    uint64_t tick; // Authoritative; boat.time is a derived view cache that S3 removes.
} BoatSession;

// Tick zero, the boat at the definition's spawn, a reset enabled presentation and WATER as
// the world's initial state.
void boat_session_init(
    BoatSession *session, const BoatWorldDefinition *definition, BoatWater water);
void boat_session_set_tick(BoatSession *session, uint64_t tick);
double boat_session_time(const BoatSession *session);
// Put the boat back at the definition's spawn and reset its presentation; the clock and world
// water/history continue.
void boat_session_respawn(BoatSession *session);
// Advance the clock once, step the boat at the new tick, then its presentation when enabled.
// False at the clock limit.
bool boat_session_step(BoatSession *session, float wheel_change, float throttle_change);

#endif
