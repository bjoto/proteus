#ifndef PROTEUS_WATERFRONT_SCENE_H
#define PROTEUS_WATERFRONT_SCENE_H

#include "renderer3d.h"

// Renderer-independent authored data and time evaluation for the waterfront.
// The returned mesh refers to scene-owned, static generated geometry.
typedef struct {
    View3D camera;
    float time;
    Transform3D bridge;
    Color bridge_color;
    float boat_z;
} WaterfrontSceneFrame;

typedef struct {
    const Vector3 *points;
    int point_count;
} WaterfrontStreet;

float waterfront_scene_duration(bool short_route);
void waterfront_scene_evaluate(float time, bool short_route, WaterfrontSceneFrame *frame);
const Mesh3D *waterfront_scene_city_mesh(void);
WaterfrontStreet waterfront_scene_street(int index);
Vector3 waterfront_scene_street_position(
    WaterfrontStreet street, float distance, Vector3 *direction);

#endif
