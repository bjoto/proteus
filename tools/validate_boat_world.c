/* W3 basin slice: explicit worlds and sessions stay independent. W4 extends this file with
 * map/cache lifecycle checks. */
#include "boat_map.h"
#include "boat_session.h"
#include "raymath.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test);                             \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

enum { TICKS = 2400, SESSIONS = 3 };

// Powered loop through the session's own wake; the same inputs for every world.
static void drive(BoatSession *session, int tick)
{
    const Boat *boat = &session->boat;
    float throttle = tick < 1440 ? 1 : 0;
    float wheel = tick >= 420 && tick < 1440 ? 1 : 0;
    boat_session_step(session, Clamp((wheel - boat->wheel) / BOAT_STEP, -1, 1),
        Clamp((throttle - boat->throttle) / (.5f * BOAT_STEP), -1, 1));
}

static void start(BoatSession *session, int index)
{
    const BoatWorldDefinition *basin = boat_location(BOAT_LOCATION_BASIN);
    BoatWater calm = { .player_wake_enabled = true };
    BoatWater waves = boat_world_test_water(basin, true, true, false);
    waves.player_wake_enabled = true;
    BoatWater mapped = boat_water_gentle();
    if (index == 0)
        boat_session_init(session, basin, calm);
    else if (index == 1)
        boat_session_init(session, basin, waves);
    else
        boat_session_init(session, boat_location(BOAT_LOCATION_ERIKSBERG), mapped);
}

static bool same(const BoatSession *a, const BoatSession *b)
{
    return a->tick == b->tick && a->world.definition == b->world.definition
        && memcmp(&a->boat, &b->boat, sizeof(a->boat)) == 0
        && memcmp(&a->world.water, &b->world.water, sizeof(a->world.water)) == 0;
}

// Solo references, then the same sessions stepped interleaved in one process.
static int interleaved(void)
{
    static BoatSession solo[SESSIONS], live[SESSIONS];
    for (int i = 0; i < SESSIONS; ++i) {
        start(&solo[i], i);
        for (int tick = 0; tick < TICKS; ++tick)
            drive(&solo[i], tick);
        start(&live[i], i);
    }
    for (int tick = 0; tick < TICKS; ++tick)
        for (int i = SESSIONS - 1; i >= 0; --i)
            drive(&live[i], tick);
    for (int i = 0; i < SESSIONS; ++i)
        CHECK(same(&solo[i], &live[i]));
    // The worlds really differ: otherwise equality would prove nothing about isolation.
    CHECK(live[0].world.water.player_wake_count > 0 && live[1].world.water.player_wake_count > 0);
    CHECK(Vector2Distance(live[0].boat.position, live[1].boat.position) > 1);
    CHECK(live[2].world.definition->mapped && !live[0].world.definition->mapped);
    printf("interleaved=ok sessions=basin-calm-d3,basin-waves-current-d3,eriksberg-waves "
           "ticks=%d d3=%d,%d\n",
        TICKS, live[0].world.water.player_wake_count, live[1].world.water.player_wake_count);
    return 0;
}

// Assignment clones a session; the clone's wake history and clock never alias the original.
static int clones(void)
{
    static BoatSession original, clone, reference;
    start(&original, 1);
    for (int tick = 0; tick < 1200; ++tick)
        drive(&original, tick);
    clone = original;
    reference = original;
    for (int tick = 1200; tick < 1800; ++tick) {
        drive(&original, tick);
        boat_session_step(&clone, -1, -1);
    }
    for (int tick = 1200; tick < 1800; ++tick)
        drive(&reference, tick);
    CHECK(same(&original, &reference));
    CHECK(!same(&original, &clone) && clone.tick == original.tick);
    CHECK(original.world.water.player_wake_count != clone.world.water.player_wake_count
        || Vector2Distance(original.boat.position, clone.boat.position) > 1);
    puts("clone=ok");
    return 0;
}

// Views and respawn read or reset only what they own.
static int views(void)
{
    static BoatSession session, before;
    start(&session, 1);
    for (int tick = 0; tick < 900; ++tick)
        drive(&session, tick);
    before = session;
    static WaterfrontSceneFrame frame;
    Renderer3D renderer = { 0 };
    Color *pixels = malloc(320 * 200 * sizeof(*pixels));
    CHECK(pixels);
    boat_scene_frame(&session.boat, &session.world, BOAT_CAMERA_DEFAULT, &frame);
    CHECK(
        boat_draw(&renderer, pixels, 320, 200, &session.boat, &session.world, BOAT_CAMERA_DEFAULT));
    CHECK(same(&session, &before));
    CHECK(frame.time == boat_session_time(&session));
    boat_session_respawn(&session);
    CHECK(session.tick == before.tick && session.boat.time == before.boat.time);
    CHECK(memcmp(&session.world.water, &before.world.water, sizeof(session.world.water)) == 0);
    CHECK(Vector2Equals(session.boat.position, session.world.definition->spawn));
    renderer3d_destroy(&renderer);
    free(pixels);
    puts("views=ok draw/frame unchanged respawn keeps clock+water");
    return 0;
}

int main(void)
{
    SetTraceLogLevel(LOG_WARNING);
    CHECK(boat_location(BOAT_LOCATION_COUNT) == NULL && !boat_location_find("harbour"));
    for (int i = 0; i < BOAT_LOCATION_COUNT; ++i)
        CHECK(boat_location_find(boat_location(i)->name) == boat_location(i));
    CHECK(interleaved() == 0);
    CHECK(clones() == 0);
    CHECK(views() == 0);
    boat_water_destroy();
    boat_map_destroy();
    puts("boat_world=ok");
    return 0;
}
