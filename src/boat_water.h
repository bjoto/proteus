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

BoatWater boat_water_gentle(void);
BoatWaterSample boat_water_query(BoatWater water, Vector2 position, float time);
// Dissipative normal resistance in simulation coordinates. Positive support is the local upward
// spring force; it caps exit damping so the combined support never pulls the hull underwater.
Vector3 boat_water_resistance(
    BoatWaterSample water, Vector3 point_velocity, float damping, float support);
// Borrowed, tessellated surface; renderer X is reflected from simulation X. NULL on allocation
// failure. CPU draws the colors; GPU uploads topology once and evaluates the shared waves in
// its water shader. Each call updates borrowed storage.
const Mesh3D *boat_water_surface(BoatWater water, float time, bool eriksberg);
void boat_water_destroy(void);

#endif
