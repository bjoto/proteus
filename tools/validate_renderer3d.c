#include "renderer3d.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "Renderer check failed at line %d: %s\n", __LINE__, #condition);       \
            renderer3d_destroy(&renderer);                                                         \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void)
{
    Renderer3D renderer = { 0 };
    Color pixels[80 * 60] = { 0 }, reference[80 * 60];
    View3D camera = { .position = { 0, 0, 0 },
        .target = { 0, 0, 1 },
        .up = { 0, 1, 0 },
        .vertical_fov = 1.2f,
        .near_plane = 0.1f,
        .far_plane = 20 };
    Vector3 light = { 0, 0, -1 };
    Transform3D boxes[] = {
        { .position = { -0.3f, 0, 3 }, .rotation = { 0.2f, 0.6f, 0 }, .scale = { 2, 1, 2 } },
        { .position = { 0.3f, 0, 3 }, .rotation = { -0.3f, -0.5f, 0 }, .scale = { 1, 2, 2 } },
    };
    Color colors[] = { RED, BLUE };
    // Intersecting objects must produce the same image in either submission order.
    for (int pass = 0; pass < 2; ++pass) {
        memset(pixels, 0, sizeof(pixels));
        CHECK(renderer3d_begin(&renderer, pixels, 80, 60, camera, light));
        for (int j = 0; j < 2; ++j) {
            int i = pass == 0 ? j : 1 - j;
            renderer3d_draw_box(&renderer, boxes[i], colors[i]);
        }
        if (pass == 0)
            memcpy(reference, pixels, sizeof(pixels));
        else
            CHECK(memcmp(reference, pixels, sizeof(pixels)) == 0);
    }
    int red = 0, blue = 0;
    for (int i = 0; i < 80 * 60; ++i) {
        red += pixels[i].r > pixels[i].b;
        blue += pixels[i].b > pixels[i].r;
    }
    CHECK(red > 0 && blue > 0);

    // The apex lies behind the near plane; clipping must retain the visible portion.
    Vector3 vertices[] = { { -1, -1, 2 }, { 0, 1, -0.5f }, { 1, -1, 2 } };
    Triangle3D triangle = { { 0, 1, 2 }, WHITE };
    Mesh3D mesh = { vertices, &triangle, 1 };
    Transform3D identity = { .scale = { 1, 1, 1 } };
    memset(pixels, 0, sizeof(pixels));
    CHECK(renderer3d_begin(&renderer, pixels, 80, 60, camera, light));
    renderer3d_draw_mesh(&renderer, &mesh, identity);
    int covered = 0;
    for (int i = 0; i < 80 * 60; ++i) {
        CHECK(isfinite(renderer.depth[i]));
        CHECK(renderer.depth[i] >= 0 && renderer.depth[i] <= 1.0f / camera.near_plane + 0.001f);
        covered += renderer.depth[i] > 0;
    }
    CHECK(covered > 0 && covered < 80 * 60);

    // This triangle lies on z = y + 3. At pixel (40,30), perspective gives
    // inverse_z = (1 + 0.5 / focal_length) / 3, independently of barycentric code.
    vertices[1] = (Vector3) { 0, 1, 4 };
    CHECK(renderer3d_begin(&renderer, pixels, 80, 60, camera, light));
    renderer3d_draw_mesh(&renderer, &mesh, identity);
    float expected_depth = (1.0f + 0.5f / renderer.focal_length) / 3.0f;
    CHECK(fabsf(renderer.depth[30 * 80 + 40] - expected_depth) < 0.00001f);

    // Fully outside each of the six frustum planes, including behind the camera.
    const Vector3 outside[] = {
        { 0, 0, -10 },
        { 0, 0, 30 },
        { -100, 0, 0 },
        { 100, 0, 0 },
        { 0, -100, 0 },
        { 0, 100, 0 },
    };
    for (size_t i = 0; i < sizeof(outside) / sizeof(outside[0]); ++i) {
        CHECK(renderer3d_begin(&renderer, pixels, 80, 60, camera, light));
        Transform3D transform = identity;
        transform.position = outside[i];
        renderer3d_draw_mesh(&renderer, &mesh, transform);
        for (int j = 0; j < 80 * 60; ++j)
            CHECK(renderer.depth[j] == 0);
    }

    // Moving camera and object together preserves projection and world-space lighting.
    CHECK(renderer3d_begin(&renderer, pixels, 80, 60, camera, light));
    memset(pixels, 0, sizeof(pixels));
    renderer3d_draw_box(&renderer, boxes[0], WHITE);
    memcpy(reference, pixels, sizeof(pixels));
    camera.position.x += 4;
    camera.target.x += 4;
    boxes[0].position.x += 4;
    CHECK(renderer3d_begin(&renderer, pixels, 80, 60, camera, light));
    memset(pixels, 0, sizeof(pixels));
    renderer3d_draw_box(&renderer, boxes[0], WHITE);
    CHECK(memcmp(reference, pixels, sizeof(pixels)) == 0);

    // Resizing reuses capacity but resets the entire active depth buffer.
    CHECK(renderer3d_begin(&renderer, pixels, 40, 30, camera, light));
    CHECK(renderer3d_begin(&renderer, pixels, 80, 60, camera, light));
    for (int i = 0; i < 80 * 60; ++i)
        CHECK(renderer.depth[i] == 0);
    camera.near_plane = 0;
    CHECK(!renderer3d_begin(&renderer, pixels, 80, 60, camera, light));
    renderer3d_destroy(&renderer);
    CHECK(renderer.depth == NULL && renderer.capacity == 0);
    puts("3D renderer validation passed");
    return 0;
}
