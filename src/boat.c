#include "boat.h"
#include "raymath.h"

#include <math.h>

// Normalized mass and world units: these are handling parameters, not a calibrated hull.
static const float ENGINE_ARM = 1.6f;
static const float MAX_ENGINE_ANGLE = 0.55f;
static const float ENGINE_THRUST = 11.0f;
static const float YAW_INERTIA = 2.2f;

static Vector2 thrust_direction(const Boat *boat)
{
    float angle = boat->heading - boat->engine_angle;
    return (Vector2) { sinf(angle), -cosf(angle) };
}

void boat_reset(Boat *boat) { *boat = (Boat) { .position = { 0, 10 } }; }

static void boundary(float *position, float *velocity, float limit)
{
    if (*position > limit) {
        *position = limit;
        if (*velocity > 0)
            *velocity *= -0.3f;
    } else if (*position < -limit) {
        *position = -limit;
        if (*velocity < 0)
            *velocity *= -0.3f;
    }
}

static void water_emit(Boat *boat, BoatWaterParticle particle)
{
    boat->water[boat->next_particle] = particle;
    boat->next_particle = (boat->next_particle + 1) % BOAT_WATER_PARTICLES;
}

static void water_step(Boat *boat)
{
    for (int i = 0; i < BOAT_WATER_PARTICLES; ++i) {
        BoatWaterParticle *p = &boat->water[i];
        if (p->lifetime <= 0)
            continue;
        p->age += BOAT_STEP;
        p->position = Vector2Add(p->position, Vector2Scale(p->velocity, BOAT_STEP));
        if (p->spray) {
            p->lift -= 12 * BOAT_STEP;
            p->height += p->lift * BOAT_STEP;
        }
        if (p->age >= p->lifetime || p->height < 0 || fabsf(p->position.x) > 29.7f
            || fabsf(p->position.y) > 19.7f)
            p->lifetime = 0;
    }

    float speed = Vector2Length(boat->velocity);
    Vector2 forward = { sinf(boat->heading), -cosf(boat->heading) };
    Vector2 right = { cosf(boat->heading), sinf(boat->heading) };
    Vector2 stern = Vector2Subtract(boat->position, Vector2Scale(forward, ENGINE_ARM));
    // Propeller wash moves opposite the force applied to the stern, even before the hull moves.
    if (boat->throttle > 0.02f) {
        boat->propeller_time += BOAT_STEP;
        if (boat->propeller_time >= 0.04f) {
            boat->propeller_time -= 0.04f;
            water_emit(boat,
                (BoatWaterParticle) { .position = stern,
                    .velocity = Vector2Scale(thrust_direction(boat), -4 * boat->throttle),
                    .height = 0.015f,
                    .lifetime = 0.7f + boat->throttle * 0.5f,
                    .size = 0.3f + boat->throttle * 0.4f });
        }
    } else {
        boat->propeller_time = 0;
    }
    if (speed > 0.8f) {
        // Distance spacing leaves a curved history in the water, independent of render rate.
        boat->wake_distance += speed * BOAT_STEP;
        if (boat->wake_distance >= 0.55f) {
            boat->wake_distance -= 0.55f;
            for (int side = -1; side <= 1; side += 2) {
                water_emit(boat,
                    (BoatWaterParticle) {
                        .position = Vector2Add(stern, Vector2Scale(right, side * 0.65f)),
                        .velocity = Vector2Scale(right, side * (0.45f + speed * 0.035f)),
                        .height = 0.01f,
                        .lifetime = 1.6f + speed * 0.045f,
                        .size = 0.4f });
            }
        }
    } else {
        boat->wake_distance = 0;
    }

    float slip = Vector2DotProduct(boat->velocity, right);
    if (fabsf(slip) > 0.9f && speed > 2) {
        boat->spray_time += BOAT_STEP;
        if (boat->spray_time >= 0.035f) {
            boat->spray_time -= 0.035f;
            float side = slip > 0 ? 1 : -1;
            // Deterministic variation keeps screenshots repeatable and avoids shared RNG state.
            float variation = (float)((++boat->emission * 37u) % 101u) / 100;
            Vector2 out = Vector2Scale(right, side);
            water_emit(boat,
                (BoatWaterParticle) {
                    .position = Vector2Add(boat->position, Vector2Scale(out, 1.05f)),
                    .velocity = Vector2Add(Vector2Scale(boat->velocity, 0.25f),
                        Vector2Scale(out, 1.5f + fabsf(slip) * (0.45f + variation * 0.4f))),
                    .height = 0.25f,
                    .lift = 2.0f + variation * 2 + fminf(fabsf(slip) * 0.25f, 1.5f),
                    .lifetime = 1.1f,
                    .size = 0.22f + variation * 0.18f,
                    .spray = true });
        }
    } else {
        boat->spray_time = 0;
    }
}

void boat_step(Boat *boat, float wheel_change, float throttle_change)
{
    // One second from centre to either lock. Opposite inputs cancel before this call.
    boat->wheel = Clamp(boat->wheel + Clamp(wheel_change, -1, 1) * BOAT_STEP, -1, 1);
    // Two seconds end to end; integrate on simulation ticks, independent of keyboard repeat.
    boat->throttle = Clamp(boat->throttle + Clamp(throttle_change, -1, 1) * 0.5f * BOAT_STEP, 0, 1);
    // The outboard follows the held wheel position; releasing keys does not centre it.
    float target_angle = boat->wheel * MAX_ENGINE_ANGLE;
    boat->engine_angle += Clamp(target_angle - boat->engine_angle, -2 * BOAT_STEP, 2 * BOAT_STEP);
    Vector2 forward = { sinf(boat->heading), -cosf(boat->heading) };
    Vector2 right = { cosf(boat->heading), sinf(boat->heading) };
    float along = Vector2DotProduct(boat->velocity, forward);
    float sideways = Vector2DotProduct(boat->velocity, right);
    Vector2 force = Vector2Scale(thrust_direction(boat), ENGINE_THRUST * boat->throttle);
    // Torque = lever arm x force: a portward stern force produces starboard yaw.
    float torque = -ENGINE_ARM * Vector2DotProduct(force, right);
    force = Vector2Subtract(force, Vector2Scale(forward, 0.55f * along));
    // Two hull stations resist their own lateral water velocity, including rotation.
    // This produces both lateral grip and yaw damping rather than prescribing a turn rate.
    for (int end = -1; end <= 1; end += 2) {
        float arm = end * 1.2f;
        float lateral_force = -1.1f * (sideways + boat->yaw_velocity * arm);
        force = Vector2Add(force, Vector2Scale(right, lateral_force));
        torque += arm * lateral_force;
    }
    torque -= 0.45f * boat->yaw_velocity;
    boat->velocity = Vector2Add(boat->velocity, Vector2Scale(force, BOAT_STEP));
    boat->yaw_velocity += torque / YAW_INERTIA * BOAT_STEP;
    boat->heading = remainderf(boat->heading + boat->yaw_velocity * BOAT_STEP, 2 * PI);
    boat->position = Vector2Add(boat->position, Vector2Scale(boat->velocity, BOAT_STEP));
    boundary(&boat->position.x, &boat->velocity.x, BOAT_LIMIT_X);
    boundary(&boat->position.y, &boat->velocity.y, BOAT_LIMIT_Z);
    water_step(boat);
}

static void box(Renderer3D *renderer, Vector3 position, Vector3 scale, Color color)
{
    renderer3d_draw_box(renderer, (Transform3D) { .position = position, .scale = scale }, color);
}

bool boat_draw(Renderer3D *renderer, Color *pixels, int width, int height, const Boat *boat)
{
    for (int i = 0; i < width * height; ++i)
        pixels[i] = (Color) { 21, 36, 48, 255 };
    View3D camera = { .position = { 0, 60, 40 },
        .target = { 0, 0, 0 },
        .up = { 0, 1, 0 },
        .vertical_fov = 45 * DEG2RAD,
        .near_plane = 1,
        .far_plane = 180 };
    if (!renderer3d_begin(renderer, pixels, width, height, camera, (Vector3) { -1, 3, 2 }))
        return false;

    box(renderer, (Vector3) { 0, -0.6f, 0 }, (Vector3) { 62, 1, 42 },
        (Color) { 36, 107, 125, 255 });
    // Static water marks provide a speed reference.
    for (int z = -18; z <= 18; z += 6)
        for (int x = -27; x <= 27; x += 6)
            box(renderer, (Vector3) { x + (z % 4), -0.08f, z }, (Vector3) { 1.3f, 0.04f, 0.12f },
                (Color) { 62, 132, 142, 255 });
    for (int i = 0; i < BOAT_WATER_PARTICLES; ++i) {
        const BoatWaterParticle *p = &boat->water[i];
        if (p->lifetime <= 0)
            continue;
        float fade = 1 - p->age / p->lifetime;
        Color color = { (unsigned char)(36 + 190 * fade), (unsigned char)(107 + 132 * fade),
            (unsigned char)(125 + 119 * fade), 255 };
        float size = p->size + (p->spray ? 0 : p->age * 0.35f);
        box(renderer, (Vector3) { -p->position.x, p->height, p->position.y },
            (Vector3) { size, p->spray ? size : 0.015f, size }, color);
    }
    Color quay = { 148, 151, 137, 255 };
    box(renderer, (Vector3) { -32, 0, 0 }, (Vector3) { 4, 1.8f, 46 }, quay);
    box(renderer, (Vector3) { 32, 0, 0 }, (Vector3) { 4, 1.8f, 46 }, quay);
    box(renderer, (Vector3) { 0, 0, -22 }, (Vector3) { 60, 1.8f, 4 }, quay);
    box(renderer, (Vector3) { 0, 0, 22 }, (Vector3) { 60, 1.8f, 4 }, quay);
    // Only the basin walls collide in this sketch; markers are visual turning references.
    for (int side = -1; side <= 1; side += 2) {
        box(renderer, (Vector3) { side * 12, 0.4f, 0 }, (Vector3) { 0.8f, 1, 0.8f },
            (Color) { 232, 150, 65, 255 });
        box(renderer, (Vector3) { side * 12, 1.1f, 0 }, (Vector3) { 0.25f, 0.5f, 0.25f },
            (Color) { 248, 225, 175, 255 });
    }

    const Vector3 vertices[]
        = { { 0, 0.55f, -1.65f }, { -0.75f, 0.55f, 0.9f }, { 0.75f, 0.55f, 0.9f },
              { 0, 0.05f, -1.65f }, { -0.75f, 0.05f, 0.9f }, { 0.75f, 0.05f, 0.9f } };
    const Color deck = { 246, 217, 155, 255 }, hull = { 229, 94, 59, 255 };
    const Triangle3D triangles[] = { { { 0, 1, 2 }, deck }, { { 0, 3, 4 }, hull },
        { { 0, 4, 1 }, hull }, { { 0, 2, 5 }, hull }, { { 0, 5, 3 }, hull }, { { 1, 4, 5 }, hull },
        { { 1, 5, 2 }, hull } };
    Mesh3D mesh = { vertices, triangles, sizeof(triangles) / sizeof(triangles[0]) };
    // This renderer's +Z-forward view basis puts world -X on screen right from this camera.
    // Reflect simulation X and heading together so right steering is clockwise on screen.
    Transform3D transform = { .position = { -boat->position.x, 0, boat->position.y },
        .rotation = { 0, boat->heading, 0 },
        .scale = { 1.4f, 1.4f, 1.4f } };
    renderer3d_draw_mesh(renderer, &mesh, transform);
    transform.position.y = 1.1f;
    transform.scale = (Vector3) { 1, 0.7f, 1 };
    renderer3d_draw_box(renderer, transform, (Color) { 44, 60, 70, 255 });
    Vector2 stern = Vector2Subtract(boat->position,
        Vector2Scale((Vector2) { sinf(boat->heading), -cosf(boat->heading) }, ENGINE_ARM));
    transform.position = (Vector3) { -stern.x, 0.4f, stern.y };
    transform.rotation.y = boat->heading - boat->engine_angle;
    transform.scale = (Vector3) { 0.55f, 0.65f, 0.9f };
    renderer3d_draw_box(renderer, transform, (Color) { 91, 100, 111, 255 });
    return true;
}
