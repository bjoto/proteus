#ifndef PROTEUS_RENDERER_GPU_H
#define PROTEUS_RENDERER_GPU_H

#include "waterfront_scene.h"

#include "raylib.h"

#include <stdbool.h>

#define WATERFRONT_GPU_MAX_LOCAL_LIGHTS 32
#define WATERFRONT_GPU_SHADOW_MAP_SIZE 1024
#define WATERFRONT_GPU_REFLECTION_WIDTH 160
#define WATERFRONT_GPU_REFLECTION_HEIGHT 120

// Ground batch stores one constant material ID per triangle in texcoord.x.
// Keep these values in sync with waterfront_city.fs.
typedef enum {
    WATERFRONT_GROUND_ASPHALT = 1,
    WATERFRONT_GROUND_PAVING,
    WATERFRONT_GROUND_CONCRETE,
    WATERFRONT_GROUND_PLANTING,
    WATERFRONT_GROUND_COUNT
} WaterfrontGroundMaterial;

// Facade UVs are metres along the wall / above quay level. Vertex RGB is the
// building palette; alpha stores this opaque material ID, not transparency.
typedef enum {
    WATERFRONT_FACADE_BRICK = 1,
    WATERFRONT_FACADE_PLASTER,
    WATERFRONT_FACADE_PANEL,
    WATERFRONT_FACADE_COUNT
} WaterfrontFacadeMaterial;

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
    Mesh quay; // All ground materials, including authored pavement tops.
    Mesh water;
    Mesh near_water; // Borrowed boat wave patch; owned by BoatGpu, never unloaded here.
    Mesh windows;
    Mesh corner;
    Mesh corner_emissive;
    Mesh crane;
    Mesh bridge;
    Mesh cube;
    Mesh player_hull, player_cabin, player_engine;
    Mesh plane;
    Mesh gull[WATERFRONT_GULL_PARTS];
    Shader shader;
    Shader shadow_shader;
    Shader water_shader;
    Shader sky_shader;
    Texture2D sky_texture;
    RenderTexture2D shadow_map;
    RenderTexture2D reflection;
    Matrix light_view_projection;
    Matrix reflection_view_projection;
    int shadow_map_size;
    Material city_material;
    Material roof_material;
    Material life_material;
    Material shadow_material;
    Material water_material;
    Texture2D roof_texture;
    WaterfrontGpuLight local_lights[WATERFRONT_GPU_MAX_LOCAL_LIGHTS];
    int local_light_ids[WATERFRONT_GPU_MAX_LOCAL_LIGHTS];
    int local_light_count;
    bool lighting_enabled;
    bool shadows_enabled;
    bool atmosphere_enabled;
    bool reflections_enabled;
    bool ready;
} WaterfrontGpuScene;

bool waterfront_gpu_scene_init(WaterfrontGpuScene *scene, const char *fragment_shader_path);
void waterfront_gpu_scene_set_lighting_enabled(WaterfrontGpuScene *scene, bool enabled);
void waterfront_gpu_scene_set_shadows_enabled(WaterfrontGpuScene *scene, bool enabled);
void waterfront_gpu_scene_set_atmosphere_enabled(WaterfrontGpuScene *scene, bool enabled);
void waterfront_gpu_scene_set_reflections_enabled(WaterfrontGpuScene *scene, bool enabled);
bool waterfront_gpu_scene_set_shadow_map_size(WaterfrontGpuScene *scene, int size);
Texture2D waterfront_gpu_scene_shadow_map(const WaterfrontGpuScene *scene);
void waterfront_gpu_scene_draw(
    WaterfrontGpuScene *scene, const WaterfrontSceneFrame *frame, int width, int height);
void waterfront_gpu_scene_destroy(WaterfrontGpuScene *scene);

#endif
