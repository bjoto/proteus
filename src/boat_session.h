#ifndef PROTEUS_BOAT_SESSION_H
#define PROTEUS_BOAT_SESSION_H

#include "boat.h"

// Minimal single-player owner: one world, one boat and the only live clock. Course, replay
// and cosmetic state still live on Boat until S2/S3. Assignment clones the whole session.
typedef struct {
    BoatWorld world;
    Boat boat;
    uint64_t tick; // Authoritative; boat.time is a derived view cache that S3 removes.
} BoatSession;

// Tick zero, the boat at the definition's spawn and WATER as the world's initial state.
void boat_session_init(
    BoatSession *session, const BoatWorldDefinition *definition, BoatWater water);
void boat_session_set_tick(BoatSession *session, uint64_t tick);
double boat_session_time(const BoatSession *session);
// Put the boat back at the definition's spawn; the clock and world water/history continue.
void boat_session_respawn(BoatSession *session);
// Advance the clock once, then step the boat at the new tick. False at the clock limit.
bool boat_session_step(BoatSession *session, float wheel_change, float throttle_change);

#endif
