#ifndef PROTEUS_WATERFRONT_SCENE_H
#define PROTEUS_WATERFRONT_SCENE_H

#include "renderer3d.h"

// Renderer-independent authored data and time evaluation for the waterfront.
// The returned mesh refers to scene-owned, static generated geometry.
#define WATERFRONT_DEMO_DURATION 240.0f
#define WATERFRONT_GULL_COUNT 5
#define WATERFRONT_GULL_PARTS 3
#define WATERFRONT_CAR_COUNT 20
#define WATERFRONT_MAX_LAMPS 32
#define WATERFRONT_WAKE_COUNT 8

typedef struct {
    int id; // Stable first-headlight ID; the second headlight is id + 1.
    int street;
    Vector3 position, forward, right;
    Transform3D body, cabin;
    Color color;
    Vector3 lenses[2][2]; // rear/front, then left/right; shared with light selection
    Vector3 headlight_direction;
    Vector3 pool_position;
} WaterfrontCarState;

typedef struct {
    int id;
    int street;
    Vector3 position, pool_direction;
    Transform3D pole;
    Vector3 lens;
} WaterfrontLampState;

typedef struct {
    float half_width, z_start, z_center;
} WaterfrontWakeState;

typedef struct {
    Transform3D hull, cabin, window;
    WaterfrontWakeState wake[WATERFRONT_WAKE_COUNT];
} WaterfrontBoatState;

// Owned snapshot: no simulation state or pointers into mutable actor storage.
// Evaluate once and share it across colour, reflection, shadow, and lighting passes.
typedef struct {
    View3D camera;
    float time;
    Transform3D bridge;
    Color bridge_color;
    WaterfrontCarState cars[WATERFRONT_CAR_COUNT];
    WaterfrontLampState lamps[WATERFRONT_MAX_LAMPS];
    int lamp_count;
    WaterfrontBoatState boat;
    int gull_count;
    Transform3D gulls[WATERFRONT_GULL_COUNT][WATERFRONT_GULL_PARTS];
} WaterfrontSceneFrame;

typedef struct {
    const Vector3 *points;
    int point_count;
} WaterfrontStreet;

// Four-minute soundtrack route. Deterministic flock and chase camera; holds at the ending.
void waterfront_demo_evaluate(float time, WaterfrontSceneFrame *frame);
const Mesh3D *waterfront_gull_mesh(int part);

float waterfront_scene_duration(bool short_route);
void waterfront_scene_evaluate(float time, bool short_route, WaterfrontSceneFrame *frame);
const Mesh3D *waterfront_scene_city_mesh(void);
WaterfrontStreet waterfront_scene_street(int index);
Vector3 waterfront_scene_street_position(
    WaterfrontStreet street, float distance, Vector3 *direction);

// Borrowed static panorama, shared by CPU sky rays and the GPU sky shader. Do not unload.
// U is atan2(direction.x, direction.z) / (2*pi) + 0.5; V is (1-direction.y) / 2.
Image waterfront_sky_image(void);
void waterfront_sky_draw(Color *pixels, int width, int height, View3D camera);

#endif
