#ifndef PROTEUS_BOAT_WATER_H
#define PROTEUS_BOAT_WATER_H

#include "renderer3d.h"

typedef struct {
    // Zero initialization selects perfectly flat, still water. Direction is simulation X/Z.
    float amplitude, wavelength, angular_speed;
    Vector2 direction; // Unit vector along wave travel.
} BoatWave;

typedef struct {
    float amplitude, wavelength, angular_speed;
    Vector2 direction;
    BoatWave crossing[2]; // Smaller components; all parameters are shared with the GPU.
} BoatWater;

typedef struct {
    float height;
    Vector3 normal, velocity; // Y-up, simulation X/Z; velocity is local surface motion.
} BoatWaterSample;

// Kinematic four-sample rigid pose in renderer/world X/Z (X reflected from simulation).
// Shared by moored and travelling scenery; the player retains its dynamic six-support model.
Transform3D boat_water_pose(
    BoatWater water, float time, Vector3 position, float heading, float length, float beam);
BoatWater boat_water_gentle(void);
BoatWaterSample boat_water_query(BoatWater water, Vector2 position, float time);
// Dissipative normal resistance in simulation coordinates. Positive support is the local upward
// spring force; it caps exit damping so the combined support never pulls the hull underwater.
Vector3 boat_water_resistance(
    BoatWaterSample water, Vector3 point_velocity, float damping, float support);
// Borrowed, tessellated surface; renderer X is reflected from simulation X. NULL on allocation
// failure. CPU reference draws the colors and full displaced surface. Each call updates storage.
const Mesh3D *boat_water_surface(BoatWater water, float time, bool eriksberg);
// GPU-only rest topology, clipped to the original shoreline. World X/Z, not simulation X.
// Cached on a fixed 1.5 m grid; shifts by 12 m outside the visible displacement radius.
#define BOAT_WATER_PATCH_FULL 48.0f
#define BOAT_WATER_PATCH_RADIUS 80.0f
const Mesh3D *boat_water_patch(Vector2 world_center, bool eriksberg);
void boat_water_destroy(void);

#endif
