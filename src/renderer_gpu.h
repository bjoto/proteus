#ifndef PROTEUS_RENDERER_GPU_H
#define PROTEUS_RENDERER_GPU_H

#include "waterfront_scene.h"

#include "raylib.h"

#include <stdbool.h>

// GPU-owned, static waterfront resources for the diagnostic fixture.  They must be
// created and destroyed while a raylib/OpenGL context is alive.
typedef struct {
    Mesh city;
    Mesh facades;
    Mesh roofs;
    Mesh quay;
    Mesh windows;
    Mesh crane;
    Mesh bridge;
    Mesh cube;
    Mesh plane;
    Mesh light_patch;
    Shader shader;
    Material city_material;
    Material facade_material;
    Material roof_material;
    Material quay_material;
    Material life_material;
    Texture2D facade_texture;
    Texture2D roof_texture;
    Texture2D quay_texture;
    bool ready;
} WaterfrontGpuScene;

bool waterfront_gpu_scene_init(WaterfrontGpuScene *scene, const char *fragment_shader_path);
void waterfront_gpu_scene_draw(
    const WaterfrontGpuScene *scene, WaterfrontSceneFrame frame, int width, int height);
void waterfront_gpu_scene_destroy(WaterfrontGpuScene *scene);

#endif
