#ifndef PROTEUS_BOAT_GPU_H
#define PROTEUS_BOAT_GPU_H
#include "boat.h"
#include "renderer_gpu.h"

typedef struct {
    WaterfrontGpuScene scene;
    RenderTexture2D target, supersampled;
    Shader downsample;
} BoatGpu;

bool boat_gpu_init(BoatGpu *gpu, bool aa);
void boat_gpu_draw(BoatGpu *gpu, const Boat *boat, BoatCamera camera);
void boat_gpu_destroy(BoatGpu *gpu);
#endif
