#ifndef PROTEUS_BOAT_WATER_H
#define PROTEUS_BOAT_WATER_H

#include "boat_wake.h"
#include "boat_wake_objects.h"
#include "renderer3d.h"

typedef struct {
    // Zero initialization selects perfectly flat, still water. Direction is simulation X/Z.
    float amplitude, wavelength, angular_speed;
    Vector2 direction; // Unit vector along wave travel.
} BoatWave;

#define BOAT_WAKE_SEGMENTS 64
#define BOAT_WAKE_LIFETIME 20.0f

typedef struct {
    Vector2 position, direction; // Segment centre and unit travel direction in simulation X/Z.
    float length, emitted, amplitude, inverse_speed;
} BoatWakeSegment;

typedef struct {
    float amplitude, wavelength, angular_speed;
    const BoatWakeObjects *objects; // Borrowed app-owned arcade world; copies do not own it.
    Vector2 direction;
    Vector2 current; // Uniform flow, or a compact channel when current_width > 0.
    float current_center, current_width; // Simulation X centre and half-width.
    float wake_amplitude; // Opt-in bounded scripted source, independent of ambient waves.
    Vector2 wake_offset; // Translate the authored source and its wave field together.
    bool player_wake_enabled;
    int player_wake_count, player_wake_next;
    BoatWakeSegment player_wake[BOAT_WAKE_SEGMENTS]; // Owned history: copies never alias a boat.
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
BoatWater boat_water_test(bool waves, bool current, bool wake);
bool boat_water_displaced(BoatWater water);
// Append a travelled segment. Clear/zero the water settings to discard its entire history.
void boat_water_emit_wake(
    BoatWater *water, Vector2 start, Vector2 end, float emitted, float duration, float water_speed);
// Pack live D3 segments into renderer/world-space vec4 arrays for production and probes.
int boat_water_wake_uniforms(BoatWater water, float time, float *origins, float *parameters);
Vector2 boat_water_current(BoatWater water, Vector2 position);
// One source travels X=-24..24 at Z=-8 plus wake_offset over the first 12 seconds.
bool boat_water_wake_source(BoatWater water, float time, Vector2 *position);
BoatWaterSample boat_water_query(BoatWater water, Vector2 position, float time);
BoatWaterSample boat_water_query_field(
    BoatWater water, const BoatWake *field, Vector2 position, float time);
// Dissipative normal resistance in simulation coordinates. Positive support is the local upward
// spring force; it caps exit damping so the combined support never pulls the hull underwater.
Vector3 boat_water_resistance(
    BoatWaterSample water, Vector3 point_velocity, float damping, float support);
// Borrowed, tessellated surface; renderer X is reflected from simulation X. NULL on allocation
// failure. CPU reference draws the colors and full displaced surface. Each call updates storage.
const Mesh3D *boat_water_surface(BoatWater water, float time, bool eriksberg);
const Mesh3D *boat_water_surface_field(BoatWater water, const BoatWake *field, float time);
// GPU-only rest topology, clipped to the original shoreline. World X/Z, not simulation X.
// Cached on a fixed 1.5 m grid; shifts by 12 m outside the visible displacement radius.
#define BOAT_WATER_PATCH_FULL 48.0f
#define BOAT_WATER_PATCH_RADIUS 80.0f
const Mesh3D *boat_water_patch(Vector2 world_center, bool eriksberg);
void boat_water_destroy(void);

#endif
