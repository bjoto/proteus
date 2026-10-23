#include "boat_session.h"

void boat_session_init(BoatSession *session, const BoatWorldDefinition *definition, BoatWater water)
{
    boat_world_init(&session->world, definition, water);
    boat_reset_location(&session->boat, definition);
    boat_presentation_reset(&session->presentation);
    session->presentation_enabled = true;
    session->tick = 0;
}

void boat_session_set_tick(BoatSession *session, uint64_t tick)
{
    session->tick = tick;
    session->boat.time = boat_tick_seconds(tick);
}

double boat_session_time(const BoatSession *session) { return boat_tick_seconds(session->tick); }

void boat_session_respawn(BoatSession *session)
{
    boat_reset_location(&session->boat, session->world.definition);
    boat_presentation_reset(&session->presentation);
    session->boat.time = boat_session_time(session);
}

bool boat_session_step(BoatSession *session, float wheel_change, float throttle_change)
{
    if (session->tick == UINT64_MAX)
        return false;
    ++session->tick;
    double now = boat_session_time(session);
    boat_step(&session->boat, &session->world, now, wheel_change, throttle_change);
    if (session->presentation_enabled)
        boat_presentation_step(&session->presentation, &session->boat, &session->world, now);
    return true;
}
