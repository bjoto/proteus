#ifndef PROTEUS_BOAT_H
#define PROTEUS_BOAT_H

#include "boat_clock.h"
#include "boat_course.h"
#include "boat_wake_player.h"
#include "boat_world.h"
#include "renderer3d.h"
#include "waterfront_scene.h"

#define BOAT_STEP (1.0f / BOAT_TICK_HZ)
#define BOAT_LIMIT_X (BOAT_BASIN_HALF_X - 2.5f)
#define BOAT_LIMIT_Z (BOAT_BASIN_HALF_Z - 2.5f)
#define BOAT_WATER_PARTICLES 384
#define BOAT_CAMERA_DEFAULT ((BoatCamera) { .distance = 20, .height = 14 })

typedef struct {
    float distance; // Horizontal distance behind the boat, 8..48 world units.
    float height; // Height above the water, 4..36 world units.
    bool fixed_fov; // Disable the modest speed lens for a constant 55-degree vertical view.
} BoatCamera;

typedef struct {
    Vector2 position, velocity;
    float height, lift;
    float age, lifetime, size;
    bool spray, wash;
    float variation; // Stable birth variation; independent of the front-spray sequence.
} BoatWaterParticle;

typedef enum { BOAT_WATERBORNE, BOAT_AIRBORNE, BOAT_LANDING, BOAT_CAPSIZED } BoatMotionState;

typedef struct {
    BoatFlag flag; // Visual cloth only; never contributes forces to the hull.
    Vector2 position; // Water-plane X/Z coordinates.
    BoatCourse course;
    // View cache of the last stepped world time (the session tick), or continuous recorded
    // time for a replay view. Physics never reads it; S3 replaces it with a view state.
    double time;
    bool replay_view; // A sampled view never enters boat_step.
    Vector2 velocity;
    float heading; // Zero faces north (-Z); positive turns right.
    float throttle; // Signed thrust command, -1 (reverse) through 0 (idle) to 1 (forward).
    // Actual signed propulsion after deterministic engine response, -1..1.
    float engine_response;
    // 0 is displacement running; 1 is fully planing. Horizontal handling approximation.
    float planing;
    // Physical heave relative to the rest waterline, plus bow-up pitch and local Z roll.
    float height, vertical_velocity;
    float pitch, pitch_velocity, roll, roll_velocity;
    BoatMotionState motion_state;
    float dry_time, landing_time, inverted_contact_time;
    unsigned landings; // Whole-hull flights followed by renewed water support.
    float turn_roll_load; // Filtered gain-weighted lateral acceleration, not a pose angle.
    float wave_spray; // Smoothed 0..1 bow-entry load, used only for wave encounter particles.
    float wheel; // Spring-centred steering setting, -1 (full port) to 1 (full starboard).
    float engine_angle; // Radians of thrust deflection; positive pushes stern to port.
    float yaw_velocity; // Heading change in radians/second, integrated from stern torque.
    float camera_heading; // Smoothed follow direction, updated on fixed simulation ticks.
    BoatWaterParticle water[BOAT_WATER_PARTICLES];
    unsigned int next_particle, emission, wake_emission;
    float wake_distance, spray_time, propeller_time;
    float physical_wake_time;
    Vector2 physical_wake_start;
} Boat;

// Basin spawn at time zero; boat_reset_location() uses a definition's spawn instead.
void boat_reset(Boat *boat);
void boat_reset_location(Boat *boat, const BoatWorldDefinition *definition);
void boat_reset_course(Boat *boat);
void boat_update_flag(Boat *boat, double time);
void boat_player_pose(const Boat *boat, WaterfrontPlayerState *player);
void boat_scene_frame(
    const Boat *boat, const BoatWorld *world, BoatCamera camera, WaterfrontSceneFrame *frame);
// Advance one fixed tick ending at world time NOW; the caller owns the clock (BoatSession).
// Only WORLD's water history is written. Inputs are adjustment rates in -1..1; zero centres
// the wheel and holds throttle. Opposite steering unwinds the wheel three times faster.
void boat_step(Boat *boat, BoatWorld *world, double now, float wheel_change, float throttle_change);
// Mutual planar impulse/separation for two dynamic hulls with vertical overlap.
// Positive normalized masses also scale yaw inertia; no allocation or route ownership.
bool boat_collide_pair(Boat *a, float mass_a, Boat *b, float mass_b);
BoatWakeStern boat_wake_stern(const Boat *boat, const BoatWorld *world, double time);
// Views draw BOAT at its cached time in WORLD; a ghost contributes only its pose.
bool boat_draw(Renderer3D *renderer, Color *pixels, int width, int height, const Boat *boat,
    const BoatWorld *world, BoatCamera camera_settings);

bool boat_draw_with_ghost(Renderer3D *renderer, Color *pixels, int width, int height,
    const Boat *boat, const BoatWorld *world, BoatCamera camera, const Boat *ghost, float opacity);

#endif
