/* W3 basin slice: explicit worlds and sessions stay independent. S2 basin slice: presentation
 * follows physics without feeding it, at any display rate. W4 extends this file with map/cache
 * lifecycle checks. */
#include "boat_map.h"
#include "boat_study_state.h"
#include "boat_view.h"
#include "raymath.h"

#include <math.h>
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

static bool same_physics(const BoatSession *a, const BoatSession *b)
{
    return a->tick == b->tick && a->world.definition == b->world.definition
        && memcmp(&a->boat, &b->boat, sizeof(a->boat)) == 0
        && memcmp(&a->world.water, &b->world.water, sizeof(a->world.water)) == 0;
}

static bool same(const BoatSession *a, const BoatSession *b)
{
    return same_physics(a, b)
        && memcmp(&a->presentation, &b->presentation, sizeof(a->presentation)) == 0;
}

static const BoatPresentation rest;

static int live_particles(const BoatPresentation *presentation)
{
    int count = 0;
    for (int i = 0; i < BOAT_WATER_PARTICLES; ++i)
        count += presentation->water[i].lifetime > 0;
    return count;
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

// Views and respawn read or reset only what they own; drawing twice gives the same frame.
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
    Color *again = malloc(320 * 200 * sizeof(*again));
    CHECK(pixels && again);
    boat_scene_frame(
        &session.boat, &session.presentation, &session.world, BOAT_CAMERA_DEFAULT, &frame);
    CHECK(boat_draw(&renderer, pixels, 320, 200, &session.boat, &session.presentation,
        &session.world, BOAT_CAMERA_DEFAULT));
    boat_scene_frame(
        &session.boat, &session.presentation, &session.world, BOAT_CAMERA_DEFAULT, &frame);
    CHECK(boat_draw(&renderer, again, 320, 200, &session.boat, &session.presentation,
        &session.world, BOAT_CAMERA_DEFAULT));
    CHECK(same(&session, &before));
    CHECK(memcmp(pixels, again, 320 * 200 * sizeof(*pixels)) == 0);
    CHECK(frame.time == boat_session_time(&session));
    CHECK(frame.player.particle_count == live_particles(&session.presentation));
    boat_session_respawn(&session);
    CHECK(session.tick == before.tick && session.boat.time == before.boat.time);
    CHECK(memcmp(&session.world.water, &before.world.water, sizeof(session.world.water)) == 0);
    CHECK(Vector2Equals(session.boat.position, session.world.definition->spawn));
    CHECK(memcmp(&session.presentation, &rest, sizeof(rest)) == 0);
    renderer3d_destroy(&renderer);
    free(pixels);
    free(again);
    puts("views=ok draw/frame twice unchanged respawn keeps clock+water, resets presentation");
    return 0;
}

// Disabling presentation leaves physics, D3 history and arcade wakes identical; the disabled
// presentation never advances.
static int presentation_toggle(void)
{
    static BoatSession on, off;
    for (int i = 0; i < SESSIONS; ++i) {
        start(&on, i);
        start(&off, i);
        off.presentation_enabled = false;
        int peak = 0;
        for (int tick = 0; tick < TICKS; ++tick) {
            drive(&on, tick);
            drive(&off, tick);
            CHECK(same_physics(&on, &off));
            int live = live_particles(&on.presentation);
            peak = live > peak ? live : peak;
        }
        CHECK(peak > 0 && on.presentation.flag.initialized);
        CHECK(memcmp(&off.presentation, &rest, sizeof(rest)) == 0);
    }
    // The default arcade wake loop with the scripted crossing boat and its own cloth.
    const BoatWorldDefinition *basin = boat_location(BOAT_LOCATION_BASIN);
    static ObjectStudy objects_on, objects_off;
    boat_session_init(&on, basin, boat_water_test(false, false, false));
    boat_session_init(&off, basin, boat_water_test(false, false, false));
    off.presentation_enabled = false;
    object_study_reset(&objects_on, &on, true, BOAT_OBJECT_LIFE_TICKS, 1.8f);
    object_study_reset(&objects_off, &off, true, BOAT_OBJECT_LIFE_TICKS, 1.8f);
    for (int tick = 0; tick < 3840; ++tick) {
        demo_step(&on, tick, NULL, NULL, &objects_on);
        demo_step(&off, tick, NULL, NULL, &objects_off);
        CHECK(boat_study_state_hash(BOAT_STUDY_HASH_SEED, &on.boat, &objects_on)
            == boat_study_state_hash(BOAT_STUDY_HASH_SEED, &off.boat, &objects_off));
        CHECK(memcmp(&objects_on.other, &objects_off.other, sizeof(Boat)) == 0);
    }
    CHECK(objects_on.wakes.emitted > 0 && objects_on.other_presentation.flag.initialized);
    CHECK(memcmp(&off.presentation, &rest, sizeof(rest)) == 0);
    CHECK(memcmp(&objects_off.other_presentation, &rest, sizeof(rest)) == 0);
    printf("presentation_toggle=ok sessions=%d+crossing ticks=%d,3840 births=%llu\n", SESSIONS,
        TICKS, (unsigned long long)objects_on.wakes.emitted);
    return 0;
}

// Fixed ticks indexed by tick number give the same session at 30, 60 and 144 Hz display rates;
// frames between ticks only read. Wall-clock keyboard sampling is outside this check.
static int render_rates(void)
{
    static BoatSession reference, shown;
    static WaterfrontSceneFrame frame;
    start(&reference, 1);
    for (int tick = 0; tick < TICKS; ++tick)
        drive(&reference, tick);
    Renderer3D renderer = { 0 };
    Color *pixels = malloc(320 * 200 * sizeof(*pixels));
    CHECK(pixels);
    const int rates[] = { 30, 60, 144 };
    for (size_t r = 0; r < sizeof(rates) / sizeof(*rates); ++r) {
        start(&shown, 1);
        double accumulator = 0;
        int frames = 0;
        while (shown.tick < TICKS) {
            accumulator += fmin(1.0 / rates[r], .1);
            while (accumulator >= BOAT_TICK_SECONDS && shown.tick < TICKS) {
                drive(&shown, (int)shown.tick);
                accumulator -= BOAT_TICK_SECONDS;
            }
            boat_scene_frame(
                &shown.boat, &shown.presentation, &shown.world, BOAT_CAMERA_DEFAULT, &frame);
            if (frames++ % rates[r] == 0)
                CHECK(boat_draw(&renderer, pixels, 320, 200, &shown.boat, &shown.presentation,
                    &shown.world, BOAT_CAMERA_DEFAULT));
        }
        CHECK(same(&shown, &reference));
        printf("render_rate=%dHz frames=%d ok\n", rates[r], frames);
    }
    renderer3d_destroy(&renderer);
    free(pixels);
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
    CHECK(presentation_toggle() == 0);
    CHECK(render_rates() == 0);
    boat_water_destroy();
    boat_map_destroy();
    puts("boat_world=ok");
    return 0;
}
