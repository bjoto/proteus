#include "boat_gpu.h"
#include "rlgl.h"

#ifndef PROTEUS_ASSET_DIR
#define PROTEUS_ASSET_DIR "assets"
#endif

void boat_gpu_destroy(BoatGpu *gpu)
{
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
    if (!waterfront_gpu_scene_init(&gpu->scene, PROTEUS_ASSET_DIR "/shaders/waterfront_city.fs"))
        goto failed;
    gpu->target = LoadRenderTexture(320, 200);
    if (!IsRenderTextureValid(gpu->target))
        goto failed;
    SetTextureFilter(gpu->target.texture, TEXTURE_FILTER_POINT);
    if (aa) {
        gpu->supersampled = LoadRenderTexture(1280, 800);
        if (!IsRenderTextureValid(gpu->supersampled))
            goto failed;
        gpu->downsample = LoadShader(NULL, PROTEUS_ASSET_DIR "/shaders/gpu_downsample.fs");
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
    WaterfrontSceneFrame frame;
    boat_scene_frame(boat, camera, &frame);
    RenderTexture2D target = gpu->supersampled.id ? gpu->supersampled : gpu->target;
    BeginTextureMode(target);
    waterfront_gpu_scene_draw(&gpu->scene, &frame, target.texture.width, target.texture.height);
    EndTextureMode();
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
