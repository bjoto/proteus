#include "boat_gpu.h"
#include "boat_assets.h"
#include "raymath.h"
#include "rlgl.h"

#include <math.h>
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
    if (!boat_asset_path(shader_path, sizeof(shader_path), "shaders/waterfront_city.fs")) {
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
        if (!boat_asset_path(shader_path, sizeof(shader_path), "shaders/gpu_downsample.fs"))
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
    if (boat_water_displaced(boat->water_settings)) {
        Vector2 center
            = { roundf(-boat->position.x / 12) * 12, roundf(boat->position.y / 12) * 12 };
        Mesh *mesh = &gpu->wave_mesh;
        if (!mesh->vaoId || !Vector2Equals(center, gpu->wave_center)
            || boat->eriksberg != gpu->wave_eriksberg) {
            const Mesh3D *surface = boat_water_patch(
                (Vector2) { -boat->position.x, boat->position.y }, boat->eriksberg);
            if (!surface)
                goto restore_water;
            int count = (int)surface->triangle_count * 3;
            if (count > gpu->wave_capacity || !mesh->vaoId) {
                if (mesh->vaoId)
                    UnloadMesh(*mesh);
                *mesh = (Mesh) { 0 };
                gpu->wave_capacity = 3072;
                while (gpu->wave_capacity < count)
                    gpu->wave_capacity *= 2;
                mesh->vertexCount = gpu->wave_capacity;
                mesh->triangleCount = gpu->wave_capacity / 3;
                mesh->vertices = MemAlloc((unsigned int)gpu->wave_capacity * 3 * sizeof(float));
                if (!mesh->vertices) {
                    UnloadMesh(*mesh);
                    *mesh = (Mesh) { 0 };
                    goto restore_water;
                }
                memset(mesh->vertices, 0, (size_t)gpu->wave_capacity * 3 * sizeof(float));
                UploadMesh(mesh, true);
            }
            for (int i = 0; i < count; ++i) {
                Vector3 v = surface->vertices[surface->triangles[i / 3].vertices[i % 3]];
                memcpy(&mesh->vertices[i * 3], &v, sizeof(v));
            }
            UpdateMeshBuffer(*mesh, 0, mesh->vertices, count * 3 * (int)sizeof(float), 0);
            mesh->vertexCount = count;
            mesh->triangleCount = count / 3;
            gpu->wave_center = center;
            gpu->wave_eriksberg = boat->eriksberg;
            gpu->wave_peak_triangles
                = count / 3 > gpu->wave_peak_triangles ? count / 3 : gpu->wave_peak_triangles;
            ++gpu->wave_rebuilds;
        }
        gpu->scene.near_water = *mesh;
    }
    BeginTextureMode(target);
    waterfront_gpu_scene_draw(&gpu->scene, &frame, target.texture.width, target.texture.height);
    EndTextureMode();
restore_water:
    gpu->scene.water = original_water;
    gpu->scene.near_water = (Mesh) { 0 };
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
