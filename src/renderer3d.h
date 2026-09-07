#ifndef PROTEUS_RENDERER3D_H
#define PROTEUS_RENDERER3D_H

#include "raylib.h"

#include <stdbool.h>
#include <stddef.h>

// World and model coordinates are Y-up. The camera looks toward target, with +Z forward
// in view space. Mesh triangles wind counterclockwise when viewed from outside.
typedef struct {
    unsigned int vertices[3];
    Color color;
} Triangle3D;

typedef struct {
    const Vector3 *vertices;
    const Triangle3D *triangles;
    size_t triangle_count;
} Mesh3D;

typedef struct {
    Vector3 position;
    Vector3 rotation; // Radians: local X, then Y, then Z.
    Vector3 scale; // Positive components; zero or mirrored scales are unsupported.
} Transform3D;

typedef struct {
    Vector3 position;
    Vector3 target;
    Vector3 up; // Must not be parallel to the viewing direction.
    float vertical_fov; // Radians, strictly between zero and PI.
    float near_plane;
    float far_plane;
} View3D;

// Zero-initialize before first use. Owns only its depth buffer; pixels belong to the caller.
typedef struct {
    Color *pixels;
    float *depth;
    size_t capacity;
    int width;
    int height;
    View3D camera;
    Vector3 right;
    Vector3 up;
    Vector3 forward;
    Vector3 light_direction; // World-space direction toward the light, normalized by begin.
    float focal_length;
} Renderer3D;

// Starts a frame and clears depth, preserving the caller's background pixels.
// Returns false on invalid camera/target size or allocation failure; do not draw that frame.
bool renderer3d_begin(Renderer3D *renderer, Color *pixels, int width, int height, View3D camera,
    Vector3 light_direction);
void renderer3d_draw_mesh(Renderer3D *renderer, const Mesh3D *mesh, Transform3D transform);
void renderer3d_destroy(Renderer3D *renderer);

// Shared unit box centered at the origin, spanning [-0.5, 0.5] on each axis.
void renderer3d_draw_box(Renderer3D *renderer, Transform3D transform, Color color);

#endif
