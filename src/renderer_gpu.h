#ifndef PROTEUS_RENDERER_GPU_H
#define PROTEUS_RENDERER_GPU_H

#include "waterfront_scene.h"

#include "raylib.h"

#include <stdbool.h>

#define WATERFRONT_GPU_MAX_LOCAL_LIGHTS 32
#define WATERFRONT_GPU_SHADOW_MAP_SIZE 1024

typedef struct {
    Vector3 position;
    Vector3 direction;
    Color color;
    float radius;
    float intensity;
    float inner_cone_cos;
    float outer_cone_cos;
} WaterfrontGpuLight;

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
    Shader shader;
    Shader shadow_shader;
    RenderTexture2D shadow_map;
    Matrix light_view_projection;
    int shadow_map_size;
    Material city_material;
    Material facade_material;
    Material roof_material;
    Material quay_material;
    Material life_material;
    Material shadow_material;
    Texture2D facade_texture;
    Texture2D roof_texture;
    Texture2D quay_texture;
    WaterfrontGpuLight local_lights[WATERFRONT_GPU_MAX_LOCAL_LIGHTS];
    int local_light_ids[WATERFRONT_GPU_MAX_LOCAL_LIGHTS];
    int local_light_count;
    bool lighting_enabled;
    bool shadows_enabled;
    bool ready;
} WaterfrontGpuScene;

bool waterfront_gpu_scene_init(WaterfrontGpuScene *scene, const char *fragment_shader_path);
void waterfront_gpu_scene_set_lighting_enabled(WaterfrontGpuScene *scene, bool enabled);
void waterfront_gpu_scene_set_shadows_enabled(WaterfrontGpuScene *scene, bool enabled);
bool waterfront_gpu_scene_set_shadow_map_size(WaterfrontGpuScene *scene, int size);
Texture2D waterfront_gpu_scene_shadow_map(const WaterfrontGpuScene *scene);
void waterfront_gpu_scene_draw(
    WaterfrontGpuScene *scene, WaterfrontSceneFrame frame, int width, int height);
void waterfront_gpu_scene_destroy(WaterfrontGpuScene *scene);

#endif
