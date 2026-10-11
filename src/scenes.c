#include "scenes.h"
#include "renderer3d.h"
#include "waterfront.h"

#include <math.h>
#include <string.h>

static Renderer3D cube_renderer;

const char *scene_name(SceneId id) { return id == SCENE_CUBE ? "cube" : "unknown"; }
float scene_duration(SceneId id) { return id == SCENE_CUBE ? WATERFRONT_DEMO_DURATION : 0; }
float scene_timeline_duration(void) { return WATERFRONT_DEMO_DURATION; }

bool scene_parse(const char *name, SceneId *id)
{
    if (strcmp(name, "cube") != 0)
        return false;
    *id = SCENE_CUBE;
    return true;
}

SceneFrame scene_at(float time)
{
    float local = fmaxf(0, fminf(time, WATERFRONT_DEMO_DURATION));
    return (SceneFrame) { SCENE_CUBE, local, local / WATERFRONT_DEMO_DURATION };
}

void scene_draw(Color *pixels, VirtualSize size, SceneFrame scene)
{
    WaterfrontSceneFrame frame;
    waterfront_demo_evaluate(scene.time, &frame);
    waterfront_draw_frame(&cube_renderer, pixels, size.width, size.height, &frame);
}

void scenes_shutdown(void) { renderer3d_destroy(&cube_renderer); }
