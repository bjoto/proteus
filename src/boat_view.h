#ifndef PROTEUS_BOAT_VIEW_H
#define PROTEUS_BOAT_VIEW_H

#include "boat_presentation.h"
#include "renderer3d.h"
#include "waterfront_scene.h"

#define BOAT_CAMERA_DEFAULT ((BoatCamera) { .distance = 20, .height = 14 })

typedef struct {
    float distance; // Horizontal distance behind the boat, 8..48 world units.
    float height; // Height above the water, 4..36 world units.
    bool fixed_fov; // Disable the modest speed lens for a constant 55-degree vertical view.
} BoatCamera;

// Views draw BOAT at its cached time in WORLD with its PRESENTATION (particles, cloth and
// camera follow). Drawing reads only; a ghost contributes its pose and optional cloth.

// FLAG may be NULL or uninitialized; the cloth then starts at rest on the hoist.
void boat_player_pose(const Boat *boat, const BoatFlag *flag, WaterfrontPlayerState *player);
void boat_scene_frame(const Boat *boat, const BoatPresentation *presentation,
    const BoatWorld *world, BoatCamera camera, WaterfrontSceneFrame *frame);
bool boat_draw(Renderer3D *renderer, Color *pixels, int width, int height, const Boat *boat,
    const BoatPresentation *presentation, const BoatWorld *world, BoatCamera camera_settings);
bool boat_draw_with_ghost(Renderer3D *renderer, Color *pixels, int width, int height,
    const Boat *boat, const BoatPresentation *presentation, const BoatWorld *world,
    BoatCamera camera, const Boat *ghost, const BoatFlag *ghost_flag, float opacity);

#endif
