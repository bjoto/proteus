#include "boat_gpu.h"
#include "raymath.h"
#include "rlgl.h"

#include <stdio.h>
#include <string.h>

void boat_gpu_destroy(BoatGpu *gpu)
{
    if (gpu->basin_water.vaoId)
        UnloadMesh(gpu->basin_water);
    if (gpu->wave_mesh.vaoId)
        UnloadMesh(gpu->wave_mesh);
    waterfront_gpu_scene_destroy(&gpu->scene);
    if (gpu->downsample.id)
        UnloadShader(gpu->downsample);
    if (gpu->supersampled.id)
        UnloadRenderTexture(gpu->supersampled);
    if (gpu->target.id)
        UnloadRenderTexture(gpu->target);
    *gpu = (BoatGpu) { 0 };
}

bool boat_gpu_init(BoatGpu *gpu, bool aa)
{
    char shader_path[1024];
    int length = snprintf(shader_path, sizeof(shader_path), "%sassets/shaders/waterfront_city.fs",
        GetApplicationDirectory());
    if (length < 0 || (size_t)length >= sizeof(shader_path)) {
        TraceLog(LOG_ERROR, "Boat shader path is too long");
        goto failed;
    }
    if (!waterfront_gpu_scene_init(&gpu->scene, shader_path))
        goto failed;
    gpu->target = LoadRenderTexture(320, 200);
    if (!IsRenderTextureValid(gpu->target))
        goto failed;
    SetTextureFilter(gpu->target.texture, TEXTURE_FILTER_POINT);
    if (aa) {
        gpu->supersampled = LoadRenderTexture(1280, 800);
        if (!IsRenderTextureValid(gpu->supersampled))
            goto failed;
        length = snprintf(shader_path, sizeof(shader_path), "%sassets/shaders/gpu_downsample.fs",
            GetApplicationDirectory());
        if (length < 0 || (size_t)length >= sizeof(shader_path))
            goto failed;
        gpu->downsample = LoadShader(NULL, shader_path);
        if (!IsShaderValid(gpu->downsample) || gpu->downsample.id == rlGetShaderIdDefault())
            goto failed;
    }
    return true;
failed:
    boat_gpu_destroy(gpu);
    return false;
}

void boat_gpu_draw(BoatGpu *gpu, const Boat *boat, BoatCamera camera)
{
    boat_gpu_draw_with_ghost(gpu, boat, camera, NULL, 0);
}

void boat_gpu_draw_with_ghost(
    BoatGpu *gpu, const Boat *boat, BoatCamera camera, const Boat *ghost, float opacity)
{
    WaterfrontSceneFrame frame;
    boat_scene_frame(boat, camera, &frame);
    if (ghost && opacity > 0) {
        WaterfrontPlayerState pose = { 0 };
        boat_player_pose(ghost, &pose);
        frame.ghost_opacity = opacity;
        frame.ghost_hull = pose.hull;
        frame.ghost_cabin = pose.cabin;
        frame.ghost_engine = pose.engine;
    }
    RenderTexture2D target = gpu->supersampled.id ? gpu->supersampled : gpu->target;
    Mesh original_water = gpu->scene.water;
    if (!boat->eriksberg) {
        if (!gpu->basin_water.vaoId)
            gpu->basin_water = GenMeshPlane(2 * BOAT_BASIN_HALF_X, 2 * BOAT_BASIN_HALF_Z, 1, 1);
        gpu->scene.water = gpu->basin_water;
    }
    if (boat->water_settings.amplitude != 0) {
        Mesh *mesh = &gpu->wave_mesh;
        if (!mesh->vaoId) {
            // Upload topology once. The water vertex shader evaluates the same explicit waves
            // as boat_water_query; no per-frame mesh transfers or CPU wave tessellation.
            const Mesh3D *surface = boat_water_surface((BoatWater) { 0 }, 0, boat->eriksberg);
            if (!surface)
                return;
            mesh->vertexCount = (int)surface->triangle_count * 3;
            mesh->triangleCount = (int)surface->triangle_count;
            mesh->vertices = MemAlloc((unsigned int)mesh->vertexCount * 3 * sizeof(float));
            mesh->normals = MemAlloc((unsigned int)mesh->vertexCount * 3 * sizeof(float));
            mesh->colors = MemAlloc((unsigned int)mesh->vertexCount * 4);
            if (!mesh->vertices || !mesh->normals || !mesh->colors) {
                UnloadMesh(*mesh);
                *mesh = (Mesh) { 0 };
                return;
            }
            for (size_t i = 0; i < surface->triangle_count; ++i) {
                Triangle3D t = surface->triangles[i];
                for (int j = 0; j < 3; ++j) {
                    Vector3 v = surface->vertices[t.vertices[j]];
                    Vector3 normal = { 0, 1, 0 };
                    Color color = { 45, 79, 104, 255 };
                    size_t index = i * 3 + (size_t)j;
                    memcpy(&mesh->vertices[index * 3], &v, sizeof(v));
                    memcpy(&mesh->normals[index * 3], &normal, sizeof(normal));
                    memcpy(&mesh->colors[index * 4], &color, sizeof(color));
                }
            }
            UploadMesh(mesh, false);
        }
        gpu->scene.water = *mesh;
    }
    BeginTextureMode(target);
    waterfront_gpu_scene_draw(&gpu->scene, &frame, target.texture.width, target.texture.height);
    EndTextureMode();
    gpu->scene.water = original_water;
    if (gpu->supersampled.id) {
        BeginTextureMode(gpu->target);
        ClearBackground(BLACK);
        BeginShaderMode(gpu->downsample);
        DrawTexturePro(target.texture, (Rectangle) { 0, 0, 1280, -800 },
            (Rectangle) { 0, 0, 320, 200 }, (Vector2) { 0 }, 0, WHITE);
        EndShaderMode();
        EndTextureMode();
    }
}
