#ifndef PROTEUS_RENDERER_GPU_H
#define PROTEUS_RENDERER_GPU_H

#include "waterfront_scene.h"

#include "raylib.h"

#include <stdbool.h>

// GPU-owned, static waterfront resources for the diagnostic fixture.  They must be
// created and destroyed while a raylib/OpenGL context is alive.
typedef struct {
    Mesh city;
    Mesh windows;
    Mesh crane;
    Mesh bridge;
    Mesh cube;
    Mesh plane;
    Shader shader;
    bool ready;
} WaterfrontGpuScene;

bool waterfront_gpu_scene_init(WaterfrontGpuScene *scene, const char *fragment_shader_path);
void waterfront_gpu_scene_draw(
    const WaterfrontGpuScene *scene, WaterfrontSceneFrame frame, int width, int height);
void waterfront_gpu_scene_destroy(WaterfrontGpuScene *scene);

#endif
