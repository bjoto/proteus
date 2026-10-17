#include "boat.h"
#include "boat_map.h"
#include "raymath.h"
#include "waterfront.h"

#include <math.h>

// Normalized mass and world units: these are handling parameters, not a calibrated hull.
static const float ENGINE_ARM = 1.6f;
static const float MAX_ENGINE_ANGLE = 0.55f;
static const float ENGINE_THRUST = 11.0f;
static const float ENGINE_RESPONSE_RATE = 3.5f;
// Overdamped spring return when steering is released; larger values centre faster (1/seconds).
static const float WHEEL_RETURN_RATE = 3.0f;
static const float YAW_INERTIA = 2.2f;
static const float HULL_DRAG_LINEAR = 0.22f;
static const float HULL_DRAG_QUADRATIC = 0.025f;
// Planing is a deliberately simple horizontal handling approximation.  The separate thresholds
// prevent a noisy speed trace from repeatedly changing modes; the state itself is rate-limited.
static const float PLANING_ENTER_SPEED = 6.5f;
static const float PLANING_EXIT_SPEED = 5.0f;
static const float PLANING_RISE_RATE = 0.85f;
static const float PLANING_FALL_RATE = 3.0f;
static const float PLANING_DRAG_MULTIPLIER = 0.62f;
// Presentation only: a transition hump settles to a modest cruise angle. No vertical force.
static const float VISUAL_TRANSITION_PITCH = 8 * DEG2RAD;
static const float VISUAL_CRUISE_PITCH = 3 * DEG2RAD;
static const float VISUAL_PITCH_RESPONSE = 5.0f;
static const float SKEG_RESISTANCE = 0.10f;
// Contact samples form an intentionally simple, forgiving hull footprint.
static const float HULL_BOW = 2.0f;
static const float HULL_STERN = 1.6f;
static const float HULL_HALF_BEAM = .75f;
static const float HULL_SHORE_CLEARANCE = .35f;
static const float CONTACT_RESTITUTION = .12f;
static const float CONTACT_TANGENT_DAMPING = .82f;

static Vector2 thrust_direction(const Boat *boat)
{
    float angle = boat->heading - boat->engine_angle;
    return (Vector2) { sinf(angle), -cosf(angle) };
}

void boat_reset(Boat *boat) { *boat = (Boat) { .position = { 0, 10 } }; }

void boat_reset_location(Boat *boat, bool eriksberg)
{
    boat_reset(boat);
    boat->eriksberg = eriksberg;
    if (eriksberg)
        boat->position = (Vector2) { -12, 60 };
}

typedef struct {
    Vector2 local, normal;
} HullContact;

static Vector2 hull_point(Vector2 forward, Vector2 right, Vector2 position, Vector2 local)
{
    return Vector2Add(
        position, Vector2Add(Vector2Scale(forward, local.y), Vector2Scale(right, local.x)));
}

static bool hull_contact(const Boat *boat, HullContact *contact)
{
    const Vector2 samples[] = { { 0, HULL_BOW }, { -HULL_HALF_BEAM, HULL_BOW },
        { HULL_HALF_BEAM, HULL_BOW }, { 0, -HULL_STERN }, { -HULL_HALF_BEAM, -HULL_STERN },
        { HULL_HALF_BEAM, -HULL_STERN } };
    Vector2 forward = { sinf(boat->heading), -cosf(boat->heading) };
    Vector2 right = { cosf(boat->heading), sinf(boat->heading) };
    for (size_t i = 0; i < sizeof(samples) / sizeof(*samples); ++i) {
        Vector2 point = hull_point(forward, right, boat->position, samples[i]);
        Vector2 normal = { 0 };
        bool water;
        if (boat->eriksberg) {
            water = boat_map_water_contact(point, HULL_SHORE_CLEARANCE, &normal);
        } else {
            water = fabsf(point.x) <= BOAT_BASIN_HALF_X - HULL_SHORE_CLEARANCE
                && fabsf(point.y) <= BOAT_BASIN_HALF_Z - HULL_SHORE_CLEARANCE;
            if (!water) {
                if (fabsf(point.x) - (BOAT_BASIN_HALF_X - HULL_SHORE_CLEARANCE)
                    > fabsf(point.y) - (BOAT_BASIN_HALF_Z - HULL_SHORE_CLEARANCE))
                    normal.x = point.x < 0 ? 1 : -1;
                else
                    normal.y = point.y < 0 ? 1 : -1;
            }
        }
        if (!water) {
            *contact = (HullContact) { samples[i], normal };
            return true;
        }
    }
    return false;
}

static void resolve_hull_contact(Boat *boat, Vector2 safe_position, HullContact contact)
{
    boat->position = safe_position;
    Vector2 forward = { sinf(boat->heading), -cosf(boat->heading) };
    Vector2 right = { cosf(boat->heading), sinf(boat->heading) };
    Vector2 arm
        = Vector2Add(Vector2Scale(right, contact.local.x), Vector2Scale(forward, contact.local.y));
    Vector2 point_velocity = Vector2Add(
        boat->velocity, (Vector2) { -boat->yaw_velocity * arm.y, boat->yaw_velocity * arm.x });
    float normal_speed = Vector2DotProduct(point_velocity, contact.normal);
    if (normal_speed < 0) {
        float arm_cross_normal = arm.x * contact.normal.y - arm.y * contact.normal.x;
        float impulse = -(1 + CONTACT_RESTITUTION) * normal_speed
            / (1 + arm_cross_normal * arm_cross_normal / YAW_INERTIA);
        boat->velocity = Vector2Add(boat->velocity, Vector2Scale(contact.normal, impulse));
        boat->yaw_velocity += arm_cross_normal * impulse / YAW_INERTIA;
    }
    float tangent_speed
        = Vector2DotProduct(boat->velocity, (Vector2) { -contact.normal.y, contact.normal.x });
    boat->velocity = Vector2Subtract(boat->velocity,
        Vector2Scale((Vector2) { -contact.normal.y, contact.normal.x },
            tangent_speed * (1 - CONTACT_TANGENT_DAMPING)));
}

// Raymath's XYZ Euler order is not local pitch followed by heading. Compose the attachment
// in hull space first, then convert to the existing renderer transform convention.
static Vector3 pose_rotation(Matrix rotation)
{
    float y = asinf(Clamp(rotation.m8, -1, 1));
    if (fabsf(cosf(y)) < .0001f)
        return (Vector3) { atan2f(rotation.m6, rotation.m5), y, 0 };
    return (Vector3) { atan2f(-rotation.m9, rotation.m10), y, atan2f(-rotation.m4, rotation.m0) };
}

static Matrix hull_rotation(const Boat *boat)
{
    return MatrixMultiply(MatrixRotateX(boat->visual_pitch), MatrixRotateY(boat->heading));
}

static Vector3 hull_attachment(const Boat *boat, Vector3 local)
{
    return Vector3Add((Vector3) { -boat->position.x, 0, boat->position.y },
        Vector3Transform(local, hull_rotation(boat)));
}

static void player_pose(const Boat *boat, WaterfrontPlayerState *player)
{
    Matrix rotation = hull_rotation(boat);
    player->hull = (Transform3D) { .position = { -boat->position.x, 0, boat->position.y },
        .rotation = pose_rotation(rotation),
        .scale = { 1.4f, 1.4f, 1.4f } };
    player->cabin = (Transform3D) { .position = hull_attachment(boat, (Vector3) { 0, 1.1f, 0 }),
        .rotation = player->hull.rotation,
        .scale = { 1, .7f, 1 } };
    player->engine = (Transform3D) { .position
        = hull_attachment(boat, (Vector3) { 0, .4f, ENGINE_ARM }),
        .rotation = pose_rotation(MatrixMultiply(MatrixRotateY(-boat->engine_angle), rotation)),
        .scale = { .55f, .65f, .9f } };
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
        if (p->age >= p->lifetime || p->height < 0
            || (!boat->eriksberg
                && (fabsf(p->position.x) > BOAT_BASIN_HALF_X - 0.3f
                    || fabsf(p->position.y) > BOAT_BASIN_HALF_Z - 0.3f)))
            p->lifetime = 0;
    }

    float speed = Vector2Length(boat->velocity);
    Vector2 right = { cosf(boat->heading), sinf(boat->heading) };
    Vector3 stern_world = hull_attachment(boat, (Vector3) { 0, 0, ENGINE_ARM });
    Vector2 stern = { -stern_world.x, stern_world.z };
    // Propeller wash moves opposite the force applied to the stern, even before the hull moves.
    if (fabsf(boat->engine_response) > 0.02f) {
        boat->propeller_time += BOAT_STEP;
        if (boat->propeller_time >= 0.04f) {
            boat->propeller_time -= 0.04f;
            water_emit(boat,
                (BoatWaterParticle) { .position = stern,
                    .velocity = Vector2Scale(thrust_direction(boat), -4 * boat->engine_response),
                    .height = 0.015f,
                    .lifetime = 0.7f + fabsf(boat->engine_response) * 0.5f,
                    .size = 0.3f + fabsf(boat->engine_response) * 0.4f });
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
                        .velocity = Vector2Scale(
                            right, side * (0.45f + speed * 0.035f) * Lerp(1, 1.6f, boat->planing)),
                        .height = 0.01f,
                        .lifetime = (1.6f + speed * 0.045f) * Lerp(1, 1.25f, boat->planing),
                        .size = Lerp(.4f, .65f, boat->planing) });
            }
        }
    } else {
        boat->wake_distance = 0;
    }

    // Planing adds symmetric low spray in straight running; turning slip still favours one side.
    float slip = Vector2DotProduct(boat->velocity, right);
    bool planing_spray = boat->planing > .05f && speed > 2;
    if ((fabsf(slip) > 0.9f && speed > 2) || planing_spray) {
        boat->spray_time += BOAT_STEP;
        if (boat->spray_time >= 0.035f) {
            boat->spray_time -= 0.035f;
            float strength = fabsf(slip) > .9f ? 1 : boat->planing;
            int first_side = fabsf(slip) > .9f ? (slip > 0 ? 1 : -1) : -1;
            int last_side = fabsf(slip) > .9f ? first_side : 1;
            // Deterministic variation keeps screenshots repeatable and avoids shared RNG state.
            float variation = (float)((++boat->emission * 37u) % 101u) / 100;
            for (int side = first_side; side <= last_side; side += 2) {
                Vector2 out = Vector2Scale(right, side);
                Vector3 origin = hull_attachment(boat, (Vector3) { -side * 1.05f, .25f, -.4f });
                water_emit(boat,
                    (BoatWaterParticle) { .position = { -origin.x, origin.z },
                        .velocity = Vector2Add(Vector2Scale(boat->velocity, 0.25f),
                            Vector2Scale(out,
                                1.5f + fabsf(slip) * (0.45f + variation * 0.4f) + boat->planing)),
                        .height = fmaxf(.03f, origin.y),
                        .lift
                        = (2.0f + variation * 2 + fminf(fabsf(slip) * 0.25f, 1.5f)) * strength,
                        .lifetime = 1.1f,
                        .size = (.22f + variation * .18f) * Lerp(1, 1.3f, boat->planing) * strength,
                        .spray = true });
            }
        }
    } else {
        boat->spray_time = 0;
    }
}

void boat_step(Boat *boat, float wheel_change, float throttle_change)
{
    boat->time += BOAT_STEP;
    // One second from centre to either lock. Opposite inputs cancel before this call.
    if (wheel_change != 0) {
        boat->wheel = Clamp(boat->wheel + Clamp(wheel_change, -1, 1) * BOAT_STEP, -1, 1);
    } else {
        boat->wheel *= expf(-WHEEL_RETURN_RATE * BOAT_STEP);
        if (fabsf(boat->wheel) < 0.001f)
            boat->wheel = 0;
    }
    // Four seconds end to end through neutral; integrate on fixed ticks, not keyboard repeat.
    boat->throttle
        = Clamp(boat->throttle + Clamp(throttle_change, -1, 1) * 0.5f * BOAT_STEP, -1, 1);
    float engine_target = boat->throttle;
    boat->engine_response += Clamp(engine_target - boat->engine_response,
        -ENGINE_RESPONSE_RATE * BOAT_STEP, ENGINE_RESPONSE_RATE * BOAT_STEP);
    // The outboard follows the wheel as it turns or springs back to centre.
    float target_angle = boat->wheel * MAX_ENGINE_ANGLE;
    boat->engine_angle += Clamp(target_angle - boat->engine_angle, -2 * BOAT_STEP, 2 * BOAT_STEP);
    Vector2 forward = { sinf(boat->heading), -cosf(boat->heading) };
    Vector2 right = { cosf(boat->heading), sinf(boat->heading) };
    float along = Vector2DotProduct(boat->velocity, forward);
    float sideways = Vector2DotProduct(boat->velocity, right);
    // Only forward water-relative motion can establish plane; physical lift waits for C2.
    float planing_target = 0;
    if (along > (boat->planing > 0 ? PLANING_EXIT_SPEED : PLANING_ENTER_SPEED))
        planing_target = 1;
    float planing_rate = planing_target > boat->planing ? PLANING_RISE_RATE : PLANING_FALL_RATE;
    boat->planing = Clamp(boat->planing
            + Clamp(planing_target - boat->planing, -planing_rate * BOAT_STEP,
                planing_rate * BOAT_STEP),
        0, 1);
    Vector2 force = Vector2Scale(thrust_direction(boat), ENGINE_THRUST * boat->engine_response);
    // Torque = lever arm x force: a portward stern force produces starboard yaw.
    float torque = -ENGINE_ARM * Vector2DotProduct(force, right);
    float drag_multiplier = Lerp(1, PLANING_DRAG_MULTIPLIER, boat->planing);
    float drag = drag_multiplier * (HULL_DRAG_LINEAR + HULL_DRAG_QUADRATIC * fabsf(along)) * along;
    force = Vector2Subtract(force, Vector2Scale(forward, drag));
    // Two hull stations resist their own lateral water velocity, including rotation.
    // This produces both lateral grip and yaw damping rather than prescribing a turn rate.
    for (int end = -1; end <= 1; end += 2) {
        float arm = end * 1.2f;
        float lateral_speed = sideways + boat->yaw_velocity * arm;
        float grip = 0.7f + 0.055f * fabsf(along) + 0.10f * fabsf(lateral_speed);
        float lateral_force = -grip * lateral_speed;
        force = Vector2Add(force, Vector2Scale(right, lateral_force));
        torque += arm * lateral_force;
    }
    // Calm water: point velocity is relative to stationary water, including stern rotation.
    // The submerged outboard resists flow across its angled plane. Apply both components of
    // this force and its stern torque: it can redirect a coast, but cannot add kinetic energy.
    Vector2 stern_velocity
        = Vector2Subtract(boat->velocity, Vector2Scale(right, ENGINE_ARM * boat->yaw_velocity));
    Vector2 skeg_normal = Vector2Add(Vector2Scale(right, cosf(boat->engine_angle)),
        Vector2Scale(forward, sinf(boat->engine_angle)));
    float cross_flow = Vector2DotProduct(stern_velocity, skeg_normal);
    Vector2 skeg_force
        = Vector2Scale(skeg_normal, -SKEG_RESISTANCE * Vector2Length(stern_velocity) * cross_flow);
    force = Vector2Add(force, skeg_force);
    torque -= ENGINE_ARM * Vector2DotProduct(skeg_force, right);
    torque -= 0.45f * boat->yaw_velocity;
    boat->velocity = Vector2Add(boat->velocity, Vector2Scale(force, BOAT_STEP));
    boat->yaw_velocity += torque / YAW_INERTIA * BOAT_STEP;
    boat->heading = remainderf(boat->heading + boat->yaw_velocity * BOAT_STEP, 2 * PI);
    Vector2 previous = boat->position;
    boat->position = Vector2Add(previous, Vector2Scale(boat->velocity, BOAT_STEP));
    HullContact contact;
    if (hull_contact(boat, &contact))
        resolve_hull_contact(boat, previous, contact);
    float camera_delta = remainderf(boat->heading - boat->camera_heading, 2 * PI);
    boat->camera_heading
        = remainderf(boat->camera_heading + camera_delta * (1 - expf(-6 * BOAT_STEP)), 2 * PI);
    float pitch_target = 4 * boat->planing * (1 - boat->planing) * VISUAL_TRANSITION_PITCH
        + boat->planing * VISUAL_CRUISE_PITCH;
    boat->visual_pitch
        += (pitch_target - boat->visual_pitch) * (1 - expf(-VISUAL_PITCH_RESPONSE * BOAT_STEP));
    water_step(boat);
}

static void box(Renderer3D *renderer, Vector3 position, Vector3 scale, Color color)
{
    renderer3d_draw_box(renderer, (Transform3D) { .position = position, .scale = scale }, color);
}

static View3D boat_camera(const Boat *boat, BoatCamera camera_settings)
{
    Vector3 boat_position = { -boat->position.x, 0, boat->position.y };
    Vector3 follow = { -sinf(boat->camera_heading), 0, -cosf(boat->camera_heading) };
    Vector3 eye = Vector3Subtract(boat_position, Vector3Scale(follow, camera_settings.distance));
    eye.y = camera_settings.height;
    Vector3 look_at = Vector3Add(boat_position, Vector3Scale(follow, 5));
    look_at.y = 0.6f;
    return (View3D) { .position = eye,
        .target = look_at,
        .up = { 0, 1, 0 },
        .vertical_fov = 55 * DEG2RAD,
        .near_plane = 0.3f,
        .far_plane = boat->eriksberg ? 2200 : 350 };
}

void boat_scene_frame(const Boat *boat, BoatCamera camera, WaterfrontSceneFrame *frame)
{
    _Static_assert(BOAT_WATER_PARTICLES <= WATERFRONT_PLAYER_PARTICLES, "Player particle capacity");
    waterfront_scene_evaluate(0, true, frame);
    frame->camera = boat_camera(boat, camera);
    frame->time = boat->time;
    frame->player_active = true;
    WaterfrontPlayerState *p = &frame->player;
    player_pose(boat, p);
    for (int i = 0; i < BOAT_WATER_PARTICLES; ++i) {
        const BoatWaterParticle *water = &boat->water[i];
        if (water->lifetime <= 0 || (boat->eriksberg && !boat_map_water(water->position, 0)))
            continue;
        float fade = 1 - water->age / water->lifetime;
        Color base = boat->eriksberg ? (Color) { 45, 79, 104, 255 } : (Color) { 36, 107, 125, 255 };
        float size = water->size + (water->spray ? 0 : water->age * .35f);
        p->particles[p->particle_count++] = (WaterfrontPlayerParticle) { .transform
            = { .position = { -water->position.x, water->height, water->position.y },
                .scale = { size, water->spray ? size : .015f, size } },
            .color = { (unsigned char)(base.r + (226 - base.r) * fade),
                (unsigned char)(base.g + (239 - base.g) * fade),
                (unsigned char)(base.b + (244 - base.b) * fade), 255 },
            .spray = water->spray };
    }
}

bool boat_draw(Renderer3D *renderer, Color *pixels, int width, int height, const Boat *boat,
    BoatCamera camera_settings)
{
    for (int i = 0; i < width * height; ++i)
        pixels[i] = (Color) { 21, 36, 48, 255 };
    View3D camera = boat_camera(boat, camera_settings);
    if (boat->eriksberg) {
        if (!waterfront_draw_boating(renderer, pixels, width, height, camera))
            return false;
    } else {
        if (!renderer3d_begin(renderer, pixels, width, height, camera, (Vector3) { -1, 3, 2 }))
            return false;

        box(renderer, (Vector3) { 0, -0.6f, 0 },
            (Vector3) { 2 * BOAT_BASIN_HALF_X, 1, 2 * BOAT_BASIN_HALF_Z },
            (Color) { 36, 107, 125, 255 });
        // Static water marks provide a speed reference.
        for (int z = -(int)BOAT_BASIN_HALF_Z + 4; z < BOAT_BASIN_HALF_Z; z += 8)
            for (int x = -(int)BOAT_BASIN_HALF_X + 4; x < BOAT_BASIN_HALF_X; x += 8)
                box(renderer, (Vector3) { x + (z % 4), -0.08f, z },
                    (Vector3) { 1.3f, 0.04f, 0.12f }, (Color) { 62, 132, 142, 255 });
    }
    for (int i = 0; i < BOAT_WATER_PARTICLES; ++i) {
        const BoatWaterParticle *p = &boat->water[i];
        if (p->lifetime <= 0)
            continue;
        if (boat->eriksberg && !boat_map_water(p->position, 0))
            continue;
        float fade = 1 - p->age / p->lifetime;
        Color water
            = boat->eriksberg ? (Color) { 45, 79, 104, 255 } : (Color) { 36, 107, 125, 255 };
        Color color = { (unsigned char)(water.r + (226 - water.r) * fade),
            (unsigned char)(water.g + (239 - water.g) * fade),
            (unsigned char)(water.b + (244 - water.b) * fade), 255 };
        float size = p->size + (p->spray ? 0 : p->age * 0.35f);
        box(renderer, (Vector3) { -p->position.x, p->height, p->position.y },
            (Vector3) { size, p->spray ? size : 0.015f, size }, color);
    }
    if (!boat->eriksberg) {
        Color quay = { 148, 151, 137, 255 };
        for (int side = -1; side <= 1; side += 2) {
            box(renderer, (Vector3) { side * (BOAT_BASIN_HALF_X + 2), 0, 0 },
                (Vector3) { 4, 1.8f, 2 * BOAT_BASIN_HALF_Z + 8 }, quay);
            box(renderer, (Vector3) { 0, 0, side * (BOAT_BASIN_HALF_Z + 2) },
                (Vector3) { 2 * BOAT_BASIN_HALF_X, 1.8f, 4 }, quay);
        }
        // Only the basin walls collide in this sketch; markers are visual turning references.
        for (int side = -1; side <= 1; side += 2) {
            box(renderer, (Vector3) { side * 12, 0.4f, 0 }, (Vector3) { 0.8f, 1, 0.8f },
                (Color) { 232, 150, 65, 255 });
            box(renderer, (Vector3) { side * 12, 1.1f, 0 }, (Vector3) { 0.25f, 0.5f, 0.25f },
                (Color) { 248, 225, 175, 255 });
        }
    }
    WaterfrontPlayerState player = { 0 };
    player_pose(boat, &player);
    renderer3d_draw_mesh(renderer, waterfront_player_hull_mesh(), player.hull);
    renderer3d_draw_box(renderer, player.cabin, (Color) { 44, 60, 70, 255 });
    renderer3d_draw_box(renderer, player.engine, (Color) { 91, 100, 111, 255 });
    return true;
}
