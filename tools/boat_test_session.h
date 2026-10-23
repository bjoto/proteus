#ifndef BOAT_TEST_SESSION_H
#define BOAT_TEST_SESSION_H
#include "boat_session.h"

// Test fixture: BOAT, as authored, at tick zero in its own calm basin world.
static inline BoatSession basin_session(Boat boat)
{
    BoatSession session;
    boat_session_init(&session, boat_location(BOAT_LOCATION_BASIN), (BoatWater) { 0 });
    session.boat = boat;
    return session;
}

#endif
