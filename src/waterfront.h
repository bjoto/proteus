#ifndef PROTEUS_WATERFRONT_H
#define PROTEUS_WATERFRONT_H

#include "renderer3d.h"
#include "waterfront_scene.h"

// Standalone storyboard: full Route A is four patterns, shortened cut is two.
float waterfront_duration(bool short_route);
View3D waterfront_camera(float time, bool short_route);
bool waterfront_draw(
    Renderer3D *renderer, Color *pixels, int width, int height, float time, bool short_route);
bool waterfront_draw_frame(
    Renderer3D *renderer, Color *pixels, int width, int height, const WaterfrontSceneFrame *frame);
// Static geographic scenery without the demo's boat or timeline camera.
bool waterfront_draw_boating(
    Renderer3D *renderer, Color *pixels, int width, int height, View3D camera, bool omit_water);
// Samples storyboard and flock routes against the city with a two-metre camera radius.
// This validates the proxy geometry only, not real-world clearance.
bool waterfront_check_routes(void);

void waterfront_draw_harbor(Renderer3D *renderer, const WaterfrontHarborFrame *frame);

#endif
