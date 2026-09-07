#ifndef PROTEUS_SCENES_H
#define PROTEUS_SCENES_H

#include "raylib.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    int width;
    int height;
} VirtualSize;

typedef enum {
    SCENE_STARSCROLL,
    SCENE_PLASMA,
    SCENE_FIRE,
    SCENE_CUBE,
} SceneId;

typedef struct {
    SceneId id;
    float time;
    float progress;
} SceneFrame;

void scenes_initialize(void);
void scenes_shutdown(void);
const char *scene_name(SceneId id);
float scene_duration(SceneId id);
float scene_start_time(SceneId id);
float scene_timeline_duration(void);
bool scene_parse(const char *name, SceneId *id);
SceneFrame scene_at(float time);
void scene_draw(Color *pixels, VirtualSize size, SceneFrame scene, float timeline_time, float beat);
size_t scenes_noise_table_bytes(void);

#endif
