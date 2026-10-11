#ifndef PROTEUS_BOAT_H
#define PROTEUS_BOAT_H

#include "renderer3d.h"

#define BOAT_STEP (1.0f / 120.0f)
#define BOAT_LIMIT_X 27.5f
#define BOAT_LIMIT_Z 17.5f
#define BOAT_WATER_PARTICLES 384

typedef struct {
    Vector2 position, velocity;
    float height, lift;
    float age, lifetime, size;
    bool spray;
} BoatWaterParticle;

typedef struct {
    Vector2 position; // Water-plane X/Z coordinates.
    Vector2 velocity;
    float heading; // Zero faces north (-Z); positive turns right.
    float throttle; // Held engine setting, 0 (idle) to 1 (full power).
    float wheel; // Held steering setting, -1 (full port) to 1 (full starboard).
    float engine_angle; // Radians of thrust deflection; positive pushes stern to port.
    float yaw_velocity; // Heading change in radians/second, integrated from stern torque.
    BoatWaterParticle water[BOAT_WATER_PARTICLES];
    unsigned int next_particle, emission;
    float wake_distance, spray_time, propeller_time;
} Boat;

void boat_reset(Boat *boat);
// Advance one fixed tick. Inputs are adjustment rates in -1..1; zero holds each setting.
void boat_step(Boat *boat, float wheel_change, float throttle_change);
bool boat_draw(Renderer3D *renderer, Color *pixels, int width, int height, const Boat *boat);

#endif
