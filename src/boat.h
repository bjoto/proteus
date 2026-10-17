#ifndef PROTEUS_BOAT_H
#define PROTEUS_BOAT_H

#include "boat_course.h"
#include "boat_water.h"
#include "renderer3d.h"
#include "waterfront_scene.h"

#define BOAT_STEP (1.0f / 120.0f)
#define BOAT_BASIN_HALF_X 120.0f
#define BOAT_BASIN_HALF_Z 80.0f
#define BOAT_LIMIT_X (BOAT_BASIN_HALF_X - 2.5f)
#define BOAT_LIMIT_Z (BOAT_BASIN_HALF_Z - 2.5f)
#define BOAT_WATER_PARTICLES 384
#define BOAT_CAMERA_DEFAULT ((BoatCamera) { 20, 14 })

typedef struct {
    float distance; // Horizontal distance behind the boat, 8..48 world units.
    float height; // Height above the water, 4..36 world units.
} BoatCamera;

typedef struct {
    Vector2 position, velocity;
    float height, lift;
    float age, lifetime, size;
    bool spray;
} BoatWaterParticle;

typedef struct {
    Vector2 position; // Water-plane X/Z coordinates.
    bool eriksberg;
    BoatWater water_settings;
    BoatCourse course;
    float time;
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
    float wave_spray; // Smoothed 0..1 bow-entry load, used only for wave encounter particles.
    float wheel; // Spring-centred steering setting, -1 (full port) to 1 (full starboard).
    float engine_angle; // Radians of thrust deflection; positive pushes stern to port.
    float yaw_velocity; // Heading change in radians/second, integrated from stern torque.
    float camera_heading; // Smoothed follow direction, updated on fixed simulation ticks.
    BoatWaterParticle water[BOAT_WATER_PARTICLES];
    unsigned int next_particle, emission;
    float wake_distance, spray_time, propeller_time;
} Boat;

void boat_reset(Boat *boat);
void boat_reset_location(Boat *boat, bool eriksberg);
void boat_reset_course(Boat *boat);
void boat_player_pose(const Boat *boat, WaterfrontPlayerState *player);
void boat_scene_frame(const Boat *boat, BoatCamera camera, WaterfrontSceneFrame *frame);
// Advance one fixed tick. Inputs are adjustment rates in -1..1; zero centres the wheel and holds
// throttle.
void boat_step(Boat *boat, float wheel_change, float throttle_change);
bool boat_draw(Renderer3D *renderer, Color *pixels, int width, int height, const Boat *boat,
    BoatCamera camera_settings);

bool boat_draw_with_ghost(Renderer3D *renderer, Color *pixels, int width, int height,
    const Boat *boat, BoatCamera camera, const Boat *ghost, float opacity);

#endif
