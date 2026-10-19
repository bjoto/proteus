#include "boat.h"
#include "boat_map.h"
#include "raymath.h"
#include "waterfront.h"

#include <assert.h>
#include <math.h>

// Normalized mass and world units: these are handling parameters, not a calibrated hull.
static const float ENGINE_ARM = 1.6f;
static const float MAX_ENGINE_ANGLE = 0.55f;
static const float ENGINE_THRUST = 11.0f;
static const float ENGINE_RESPONSE_RATE = 3.5f;
// Overdamped spring return when steering is released; larger values centre faster (1/seconds).
static const float WHEEL_RETURN_RATE = 3.0f;
// Keep progressive turn-in, but unwind promptly when the player asks for the other direction.
static const float WHEEL_UNWIND_RATE = 3.0f;
static const float CAMERA_BASE_FOV = 55 * DEG2RAD;
static const float CAMERA_SPEED_FOV = 7 * DEG2RAD;
static const float CAMERA_SPEED_START = 4.0f;
static const float CAMERA_SPEED_FULL = 18.0f;
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
// Planing redistributes the support reference; it does not add a second cosmetic rotation.
static const float TRIM_TRANSITION_PITCH = 8 * DEG2RAD;
static const float TRIM_CRUISE_PITCH = 3 * DEG2RAD;
// Normalized mass=1. Six unilateral supports include weight preload, spring and local damping.
static const float GRAVITY = 9.81f;
static const float BUOYANCY_STIFFNESS = 64.0f;
static const float BUOYANCY_DAMPING = 13.0f;
static const float PITCH_INERTIA = 2.0f;
static const float ROLL_INERTIA = .45f;
static const float PLANING_HEIGHT = .05f;
// Modest encounter damping: at cruise the local normal resistance grows by at most 35%.
static const float WAVE_DAMPING_GAIN = .35f;
static const float WAVE_CRUISE_SPEED = 12.0f;
static const float WAVE_SPRAY_ENTRY_SPEED = 1.2f;
static const float WAVE_SPRAY_RESPONSE = 8.0f;
static const float SKEG_RESISTANCE = 0.10f;
// Contact samples form an intentionally simple, forgiving hull footprint.
static const float HULL_BOW = 2.0f;
static const float HULL_STERN = 1.6f;
static const float HULL_HALF_BEAM = .75f;
static const float HULL_SHORE_CLEARANCE = .35f;
static const float CONTACT_RESTITUTION = .12f;
static const float CONTACT_FRICTION = .15f;

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

void boat_reset_course(Boat *boat)
{
    boat_reset(boat);
    boat->position = (Vector2) { 0, 50 };
    boat->course.enabled = true;
}

typedef struct {
    Vector2 local, normal, velocity;
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
            if (water && waterfront_harbor_contact(point, HULL_SHORE_CLEARANCE, &normal))
                water = false;
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
            *contact = (HullContact) { .local = samples[i], .normal = normal };
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
    float normal_speed
        = Vector2DotProduct(Vector2Subtract(point_velocity, contact.velocity), contact.normal);
    if (normal_speed < 0) {
        float arm_cross_normal = arm.x * contact.normal.y - arm.y * contact.normal.x;
        float impulse = -(1 + CONTACT_RESTITUTION) * normal_speed
            / (1 + arm_cross_normal * arm_cross_normal / YAW_INERTIA);
        boat->velocity = Vector2Add(boat->velocity, Vector2Scale(contact.normal, impulse));
        boat->yaw_velocity += arm_cross_normal * impulse / YAW_INERTIA;
        // Limit tangential friction by impact strength so shallow scrapes retain momentum.
        float tangent_speed = Vector2DotProduct(Vector2Subtract(boat->velocity, contact.velocity),
            (Vector2) { -contact.normal.y, contact.normal.x });
        float friction = fminf(fabsf(tangent_speed), CONTACT_FRICTION * impulse);
        boat->velocity = Vector2Subtract(boat->velocity,
            Vector2Scale((Vector2) { -contact.normal.y, contact.normal.x },
                copysignf(friction, tangent_speed)));
    }
}

static void resolve_traffic(Boat *boat)
{
    if (!boat->eriksberg)
        return;
    // Separate the full hull footprints, then match contact velocity against the moving hull.
    // Multiple passes cover a rare simultaneous contact without advancing traffic time.
    for (int pass = 0; pass < 4; ++pass) {
        WaterfrontTrafficContact traffic;
        if (!waterfront_traffic_contact(boat->position, boat->heading, HULL_BOW, HULL_STERN,
                HULL_HALF_BEAM, boat->time, &traffic))
            break;
        Vector2 previous = boat->position;
        Vector2 shifted
            = Vector2Add(previous, Vector2Scale(traffic.normal, traffic.penetration + .002f));
        resolve_hull_contact(boat, shifted,
            (HullContact) {
                .local = traffic.local, .normal = traffic.normal, .velocity = traffic.velocity });
        HullContact shore;
        if (hull_contact(boat, &shore))
            resolve_hull_contact(boat, previous, shore);
    }
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
    return MatrixMultiply(MatrixMultiply(MatrixRotateZ(boat->roll), MatrixRotateX(boat->pitch)),
        MatrixRotateY(boat->heading));
}

static Vector3 hull_attachment(const Boat *boat, Vector3 local)
{
    return Vector3Add((Vector3) { -boat->position.x, boat->height, boat->position.y },
        Vector3Transform(local, hull_rotation(boat)));
}

// Derivatives of a renderer-space attachment with respect to the three physical angles.
// Dotting force with each derivative gives its generalized torque, including the mirrored X.
typedef struct {
    Vector3 pitch, roll, yaw, velocity;
} HullMotion;

static HullMotion hull_motion(const Boat *boat, Vector3 local)
{
    Matrix rotation = hull_rotation(boat);
    Vector3 arm = Vector3Transform(local, rotation);
    Vector3 pitch_axis = { cosf(boat->heading), 0, -sinf(boat->heading) };
    Vector3 roll_axis = Vector3Transform((Vector3) { 0, 0, 1 },
        MatrixMultiply(MatrixRotateX(boat->pitch), MatrixRotateY(boat->heading)));
    HullMotion motion = { .pitch = Vector3CrossProduct(pitch_axis, arm),
        .roll = Vector3CrossProduct(roll_axis, arm),
        .yaw = Vector3CrossProduct((Vector3) { 0, 1, 0 }, arm),
        .velocity = { -boat->velocity.x, boat->vertical_velocity, boat->velocity.y } };
    motion.velocity = Vector3Add(motion.velocity, Vector3Scale(motion.pitch, boat->pitch_velocity));
    motion.velocity = Vector3Add(motion.velocity, Vector3Scale(motion.roll, boat->roll_velocity));
    motion.velocity = Vector3Add(motion.velocity, Vector3Scale(motion.yaw, boat->yaw_velocity));
    return motion;
}

void boat_player_pose(const Boat *boat, WaterfrontPlayerState *player)
{
    Matrix rotation = hull_rotation(boat);
    player->hull
        = (Transform3D) { .position = { -boat->position.x, boat->height, boat->position.y },
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

static void buoyancy_step(Boat *boat)
{
    float trim = 4 * boat->planing * (1 - boat->planing) * TRIM_TRANSITION_PITCH
        + boat->planing * TRIM_CRUISE_PITCH;
    float acceleration = -GRAVITY, pitch_torque = 0, roll_torque = 0;
    float sp = sinf(boat->pitch), cp = cosf(boat->pitch);
    float sr = sinf(boat->roll), cr = cosf(boat->roll);
    Vector3 wave_force = { 0 };
    float wave_yaw = 0, bow_entry = 0;
    bool waves = boat_water_displaced(boat->water_settings);
    for (int end = -1; end <= 1; ++end) {
        for (int side = -1; side <= 1; side += 2) {
            float fore = end * 1.4f, across = side * .65f;
            Vector3 point = hull_attachment(boat, (Vector3) { across, 0, -fore });
            BoatWaterSample water = boat_water_query(
                boat->water_settings, (Vector2) { -point.x, point.z }, boat->time);
            // Exact vertical Jacobians of local roll, then pitch; heading leaves Y unchanged.
            float jp = fore * cp - across * sr * sp;
            float jr = across * cr * cp;
            float speed = boat->vertical_velocity + jp * boat->pitch_velocity
                + jr * boat->roll_velocity - water.velocity.y;
            float support_height
                = water.height + PLANING_HEIGHT * boat->planing + fore * sinf(trim);
            float force = GRAVITY / 6 + BUOYANCY_STIFFNESS / 6 * (support_height - point.y);
            if (waves) {
                HullMotion motion = hull_motion(boat, (Vector3) { across, 0, -fore });
                Vector3 velocity = { -motion.velocity.x, motion.velocity.y, motion.velocity.z };
                Vector3 relative = Vector3Subtract(velocity, water.velocity);
                float travel = hypotf(relative.x, relative.z);
                float damping = BUOYANCY_DAMPING / 6
                    * (1 + WAVE_DAMPING_GAIN * Clamp(travel / WAVE_CRUISE_SPEED, 0, 1));
                Vector3 resistance = boat_water_resistance(water, velocity, damping, force);
                Vector3 rendered = { -resistance.x, resistance.y, resistance.z };
                wave_force = Vector3Add(wave_force, resistance);
                pitch_torque += Vector3DotProduct(rendered, motion.pitch);
                roll_torque += Vector3DotProduct(rendered, motion.roll);
                wave_yaw += Vector3DotProduct(rendered, motion.yaw);
                if (end == 1 && force > 0)
                    bow_entry += .5f * fmaxf(0, -Vector3DotProduct(relative, water.normal));
                force = fmaxf(0, force); // Hydrostatic support; damping is applied as a 3D force.
            } else {
                // Retain the accepted C2 flat-water support and damping exactly.
                if (force > 0)
                    force = fmaxf(0, force - BUOYANCY_DAMPING / 6 * speed);
                else
                    force = 0;
            }
            acceleration += force;
            pitch_torque += jp * force;
            roll_torque += jr * force;
        }
    }
    boat->velocity.x += wave_force.x * BOAT_STEP;
    boat->velocity.y += wave_force.z * BOAT_STEP;
    boat->yaw_velocity += wave_yaw / YAW_INERTIA * BOAT_STEP;
    acceleration += wave_force.y;
    float spray_target = Clamp(bow_entry / WAVE_SPRAY_ENTRY_SPEED, 0, 1);
    boat->wave_spray = waves
        ? Lerp(boat->wave_spray, spray_target, 1 - expf(-WAVE_SPRAY_RESPONSE * BOAT_STEP))
        : 0;
    boat->vertical_velocity += acceleration * BOAT_STEP;
    boat->pitch_velocity += pitch_torque / PITCH_INERTIA * BOAT_STEP;
    boat->roll_velocity += roll_torque / ROLL_INERTIA * BOAT_STEP;
    boat->height += boat->vertical_velocity * BOAT_STEP;
    boat->pitch += boat->pitch_velocity * BOAT_STEP;
    boat->roll += boat->roll_velocity * BOAT_STEP;
}

// Integer hash gives each foam birth its own identity without disturbing spray variation.
static float wake_variation(Boat *boat)
{
    unsigned int bits = ++boat->wake_emission * 747796405u + 2891336453u;
    bits = ((bits >> ((bits >> 28u) + 4u)) ^ bits) * 277803737u;
    return (float)((bits >> 22u) & 1023u) / 1023;
}

static void water_emit(Boat *boat, BoatWaterParticle particle)
{
    if (!particle.spray)
        particle.height
            = boat_water_query(boat->water_settings, particle.position, boat->time).height + .015f;
    // Prefer expired slots. Foam may replace foam, but never shorten airborne spray.
    for (unsigned int offset = 0; offset < BOAT_WATER_PARTICLES; ++offset) {
        unsigned int slot = (boat->next_particle + offset) % BOAT_WATER_PARTICLES;
        if (boat->water[slot].lifetime <= 0) {
            boat->water[slot] = particle;
            boat->next_particle = (slot + 1) % BOAT_WATER_PARTICLES;
            return;
        }
    }
    for (unsigned int offset = 0; offset < BOAT_WATER_PARTICLES; ++offset) {
        unsigned int slot = (boat->next_particle + offset) % BOAT_WATER_PARTICLES;
        if (!boat->water[slot].spray) {
            boat->water[slot] = particle;
            boat->next_particle = (slot + 1) % BOAT_WATER_PARTICLES;
            return;
        }
    }
}

static void water_step(Boat *boat)
{
    for (int i = 0; i < BOAT_WATER_PARTICLES; ++i) {
        BoatWaterParticle *p = &boat->water[i];
        if (p->lifetime <= 0)
            continue;
        p->age += BOAT_STEP;
        Vector2 velocity = p->velocity;
        if (!p->spray)
            velocity = Vector2Add(velocity, boat_water_current(boat->water_settings, p->position));
        p->position = Vector2Add(p->position, Vector2Scale(velocity, BOAT_STEP));
        if (p->spray) {
            p->lift -= 12 * BOAT_STEP;
            p->height += p->lift * BOAT_STEP;
        }
        float surface = boat_water_query(boat->water_settings, p->position, boat->time).height;
        if (!p->spray)
            p->height = surface + .015f;
        if (p->age >= p->lifetime || (p->spray && p->height < surface)
            || (!boat->eriksberg
                && (fabsf(p->position.x) > BOAT_BASIN_HALF_X - 0.3f
                    || fabsf(p->position.y) > BOAT_BASIN_HALF_Z - 0.3f)))
            p->lifetime = 0;
    }

    Vector2 relative
        = Vector2Subtract(boat->velocity, boat_water_current(boat->water_settings, boat->position));
    float speed = Vector2Length(relative);
    Vector2 right = { cosf(boat->heading), sinf(boat->heading) };
    Vector3 stern_world = hull_attachment(boat, (Vector3) { 0, 0, ENGINE_ARM });
    Vector2 stern = { -stern_world.x, stern_world.z };
    // Propeller wash moves opposite the force applied to the stern, even before the hull moves.
    if (fabsf(boat->engine_response) > 0.02f) {
        boat->propeller_time += BOAT_STEP;
        if (boat->propeller_time >= 0.075f) {
            boat->propeller_time -= 0.075f;
            float variation = wake_variation(boat);
            float power = fabsf(boat->engine_response);
            // Unequal lobes separate as they travel aft, opening gaps instead of a solid stripe.
            for (int side = -1; side <= 1; side += 2) {
                float v = side < 0 ? variation : wake_variation(boat);
                water_emit(boat,
                    (BoatWaterParticle) { .position
                        = Vector2Add(stern, Vector2Scale(right, side * (.12f + .18f * v))),
                        .velocity = Vector2Add(Vector2Scale(thrust_direction(boat),
                                                   -(3.0f + 3.0f * v) * boat->engine_response),
                            Vector2Scale(right, side * (.3f + .65f * v) * power)),
                        .lifetime = .55f + .65f * v,
                        .size = (.28f + .38f * v) * (.5f + .5f * power),
                        .wash = true,
                        .variation = v });
            }
        }
    } else {
        boat->propeller_time = 0;
    }
    if (speed > 0.8f) {
        // Distance spacing leaves a curved history in the water, independent of render rate.
        boat->wake_distance += speed * BOAT_STEP;
        float spacing = .48f + .24f * (float)(boat->wake_emission % 7u) / 6;
        if (boat->wake_distance >= spacing) {
            boat->wake_distance -= spacing;
            for (int side = -1; side <= 1; side += 2) {
                float v = wake_variation(boat);
                water_emit(boat,
                    (BoatWaterParticle) { .position
                        = Vector2Add(stern, Vector2Scale(right, side * (.55f + .25f * v))),
                        .velocity = Vector2Scale(right,
                            side * (0.45f + speed * 0.035f) * Lerp(1, 1.6f, boat->planing)
                                * (1 + .15f * boat->wave_spray) * (.75f + .5f * v)),
                        .height = 0.01f,
                        .lifetime = (1.6f + speed * 0.045f) * Lerp(1, 1.25f, boat->planing)
                            * (.75f + .35f * v),
                        .size = Lerp(.4f, .65f, boat->planing) * (.65f + .55f * v),
                        .variation = v });
            }
        }
    } else {
        boat->wake_distance = 0;
    }

    // Planing adds symmetric low spray in straight running; turning slip still favours one side.
    float slip = Vector2DotProduct(relative, right);
    bool planing_spray = boat->planing > .05f && speed > 2;
    bool wave_spray = boat->wave_spray > .08f && speed > 2;
    if ((fabsf(slip) > 0.9f && speed > 2) || planing_spray || wave_spray) {
        boat->spray_time += BOAT_STEP;
        if (boat->spray_time >= 0.035f) {
            boat->spray_time -= 0.035f;
            float strength = fabsf(slip) > .9f ? 1 : fmaxf(boat->planing, boat->wave_spray);
            int first_side = fabsf(slip) > .9f ? (slip > 0 ? 1 : -1) : -1;
            int last_side = fabsf(slip) > .9f ? first_side : 1;
            // Deterministic variation keeps screenshots repeatable and avoids shared RNG state.
            float variation = (float)((++boat->emission * 37u) % 101u) / 100;
            for (int side = first_side; side <= last_side; side += 2) {
                Vector2 out = Vector2Scale(right, side);
                Vector3 origin = hull_attachment(boat, (Vector3) { -side * 1.05f, .25f, -.4f });
                HullMotion motion = hull_motion(boat, (Vector3) { -side * 1.05f, .25f, -.4f });
                water_emit(boat,
                    (BoatWaterParticle) { .position = { -origin.x, origin.z },
                        .velocity = Vector2Add(Vector2Scale(boat->velocity, 0.25f),
                            Vector2Scale(out,
                                1.5f + fabsf(slip) * (0.45f + variation * 0.4f) + boat->planing)),
                        .height = fmaxf(boat_water_query(boat->water_settings,
                                            (Vector2) { -origin.x, origin.z }, boat->time)
                                            .height
                                + .03f,
                            origin.y),
                        .lift = (2.0f + variation * 2 + fminf(fabsf(slip) * 0.25f, 1.5f)) * strength
                            + (boat_water_displaced(boat->water_settings)
                                    ? motion.velocity.y + boat->wave_spray
                                    : boat->vertical_velocity + .4f * boat->pitch_velocity
                                        - side * 1.05f * boat->roll_velocity),
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
    // One second from centre to either lock. Countersteering spends only 1/3 second
    // unwinding full lock; any time left in this tick uses the normal turn-in rate.
    // Splitting at centre keeps the faster rate from overshooting into the new turn.
    if (wheel_change != 0) {
        float input = Clamp(wheel_change, -1, 1);
        float remaining = BOAT_STEP;
        if (input * boat->wheel < 0) {
            float unwind = fminf(remaining, fabsf(boat->wheel / input) / WHEEL_UNWIND_RATE);
            boat->wheel += input * WHEEL_UNWIND_RATE * unwind;
            remaining -= unwind;
        }
        boat->wheel = Clamp(boat->wheel + input * remaining, -1, 1);
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
    Vector2 relative
        = Vector2Subtract(boat->velocity, boat_water_current(boat->water_settings, boat->position));
    float along = Vector2DotProduct(relative, forward);
    // Only forward water-relative motion can establish plane.
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
        Vector2 station = Vector2Add(boat->position, Vector2Scale(forward, arm));
        Vector2 flow = boat_water_current(boat->water_settings, station);
        float lateral_speed = Vector2DotProduct(Vector2Subtract(boat->velocity, flow), right)
            + boat->yaw_velocity * arm;
        float grip = 0.7f + 0.055f * fabsf(along) + 0.10f * fabsf(lateral_speed);
        float lateral_force = -grip * lateral_speed;
        force = Vector2Add(force, Vector2Scale(right, lateral_force));
        torque += arm * lateral_force;
    }
    // Outboard point velocity is relative to its local flow, including stern rotation.
    // The submerged outboard resists flow across its angled plane. Apply both components of
    // this force and its stern torque: it can redirect a coast, but cannot add kinetic energy.
    Vector2 stern_velocity = Vector2Subtract(
        Vector2Subtract(boat->velocity,
            boat_water_current(boat->water_settings,
                Vector2Subtract(boat->position, Vector2Scale(forward, ENGINE_ARM)))),
        Vector2Scale(right, ENGINE_ARM * boat->yaw_velocity));
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
    // Accept translation and rotation separately. A rejected rotation must not leave a hull
    // sample on land, otherwise even an outward move can be rejected on the next tick.
    Vector2 previous = boat->position;
    boat->position = Vector2Add(previous, Vector2Scale(boat->velocity, BOAT_STEP));
    HullContact contact;
    if (hull_contact(boat, &contact))
        resolve_hull_contact(boat, previous, contact);
    float previous_heading = boat->heading;
    boat->heading = remainderf(boat->heading + boat->yaw_velocity * BOAT_STEP, 2 * PI);
    if (hull_contact(boat, &contact)) {
        resolve_hull_contact(boat, boat->position, contact);
        boat->heading = previous_heading;
    }
    resolve_traffic(boat);
    float camera_delta = remainderf(boat->heading - boat->camera_heading, 2 * PI);
    boat->camera_heading
        = remainderf(boat->camera_heading + camera_delta * (1 - expf(-6 * BOAT_STEP)), 2 * PI);
    buoyancy_step(boat);
    if (boat->water_settings.player_wake_enabled) {
        if (boat->physical_wake_time == 0)
            boat->physical_wake_start = previous;
        boat->physical_wake_time += BOAT_STEP;
        if (boat->physical_wake_time >= .4f) {
            Vector2 flow = boat_water_current(boat->water_settings, boat->position);
            float speed = Vector2Length(Vector2Subtract(boat->velocity, flow));
            // Move the source behind the hull so fresh packets cannot lift their emitter.
            Vector2 offset = Vector2Scale(forward, -ENGINE_ARM);
            boat_water_emit_wake(&boat->water_settings,
                Vector2Add(boat->physical_wake_start, offset), Vector2Add(boat->position, offset),
                boat->time, boat->physical_wake_time, speed);
            boat->physical_wake_time = 0;
        }
    }
    water_step(boat);
    boat_course_step(&boat->course, previous, boat->position, BOAT_STEP);
}

static void box(Renderer3D *renderer, Vector3 position, Vector3 scale, Color color)
{
    renderer3d_draw_box(renderer, (Transform3D) { .position = position, .scale = scale }, color);
}

static View3D boat_camera(const Boat *boat, BoatCamera camera_settings)
{
    // A small, bounded lens change accompanies forward travel. Derive it from recorded
    // pose/velocity so seeking a replay gives the same view without hidden camera history.
    // Ground speed matches scenery passing the camera; reverse/docking retain the base lens.
    Vector2 forward = { sinf(boat->heading), -cosf(boat->heading) };
    float speed = Vector2DotProduct(boat->velocity, forward);
    float travel = camera_settings.fixed_fov
        ? 0
        : Clamp((speed - CAMERA_SPEED_START) / (CAMERA_SPEED_FULL - CAMERA_SPEED_START), 0, 1);
    travel = travel * travel * (3 - 2 * travel);
    Vector3 boat_position = { -boat->position.x, 0, boat->position.y };
    Vector3 follow = { -sinf(boat->camera_heading), 0, -cosf(boat->camera_heading) };
    Vector3 eye = Vector3Subtract(boat_position, Vector3Scale(follow, camera_settings.distance));
    eye.y = camera_settings.height;
    Vector3 look_at = Vector3Add(boat_position, Vector3Scale(follow, 5));
    look_at.y = 0.6f;
    return (View3D) { .position = eye,
        .target = look_at,
        .up = { 0, 1, 0 },
        .vertical_fov = CAMERA_BASE_FOV + CAMERA_SPEED_FOV * travel,
        .near_plane = 0.3f,
        .far_plane = boat->eriksberg ? 2200 : 350 };
}

static float water_particle_size(const BoatWaterParticle *water)
{
    if (water->spray)
        return water->size;
    float life = water->age / water->lifetime;
    if (water->wash)
        return (water->size + water->age * .9f) * (1 - .85f * life * life);
    return (water->size + water->age * .22f) * (1 - .45f * life * life);
}

static Color water_particle_color(const Boat *boat, const BoatWaterParticle *water)
{
    float fade = 1 - water->age / water->lifetime;
    if (!water->spray)
        fade *= (.68f + .32f * water->variation) * fade;
    Color base = boat->eriksberg ? (Color) { 45, 79, 104, 255 } : (Color) { 36, 107, 125, 255 };
    return (Color) { (unsigned char)(base.r + (226 - base.r) * fade),
        (unsigned char)(base.g + (239 - base.g) * fade),
        (unsigned char)(base.b + (244 - base.b) * fade), 255 };
}

static Transform3D water_particle_pose(const Boat *boat, const BoatWaterParticle *water, float size)
{
    Transform3D pose = { .position = { -water->position.x, water->height, water->position.y },
        .scale = { size, water->spray ? size : .015f, size } };
    if (!water->spray && boat_water_displaced(boat->water_settings)) {
        Vector3 normal = boat_water_query(boat->water_settings, water->position, boat->time).normal;
        normal.x = -normal.x;
        pose.rotation = pose_rotation(
            QuaternionToMatrix(QuaternionFromVector3ToVector3((Vector3) { 0, 1, 0 }, normal)));
    }
    return pose;
}

// Shared authored current cues and scripted wake source for CPU/GPU views in all locations.
static void environment_visit(const Boat *boat, BoatCourseBoxDraw draw, void *context)
{
    if (Vector2LengthSqr(boat->water_settings.current) > 0) {
        for (int x = -24; x <= 24; x += 8)
            for (int z = -48; z <= 48; z += 16) {
                float phase = fmodf(boat->time + (float)(x + z + 80) * .11f, 8);
                float fade = sinf(PI * phase / 8);
                float center_z = boat->eriksberg ? roundf(boat->position.y / 16) * 16 : 0;
                Vector2 p = { x + boat->water_settings.current_center, z + center_z };
                Vector2 flow = boat_water_current(boat->water_settings, p);
                if (Vector2LengthSqr(flow) < .01f || fade < .02f)
                    continue;
                p = Vector2Add(p, Vector2Scale(flow, phase));
                if (boat->eriksberg && !boat_map_water(p, .2f))
                    continue;
                float height = boat_water_query(boat->water_settings, p, boat->time).height;
                draw(context,
                    (Transform3D) { .position = { -p.x, height + .025f, p.y },
                        .rotation = { 0, atan2f(-flow.x, flow.y), 0 },
                        .scale = { .12f * fade, .015f, 1.5f * fade } },
                    (Color) { 113, 171, 177, 255 });
            }
    }
    Vector2 source;
    if (boat_water_wake_source(boat->water_settings, boat->time, &source)) {
        float height = boat_water_query(boat->water_settings, source, boat->time).height;
        draw(context,
            (Transform3D) {
                .position = { -source.x, height + .25f, source.y }, .scale = { 4.2f, .5f, 1.5f } },
            (Color) { 205, 151, 64, 255 });
        draw(context,
            (Transform3D) { .position = { -source.x + .5f, height + .8f, source.y },
                .scale = { 1.4f, .7f, 1.15f } },
            (Color) { 226, 222, 202, 255 });
    }
}

static void environment_draw(void *context, Transform3D transform, Color color)
{
    renderer3d_draw_box(context, transform, color);
}

static void basin_box(void *context, Transform3D transform, Color color)
{
    WaterfrontSceneFrame *frame = context;
    assert(frame->scenery_count < (int)(sizeof(frame->scenery) / sizeof(*frame->scenery)));
    int i = frame->scenery_count++;
    frame->scenery[i].transform = transform;
    frame->scenery[i].color = color;
}

static void basin_scenery(const Boat *boat, WaterfrontSceneFrame *frame)
{
    frame->boating_basin = true;
    frame->lamp_count = 0;
    Color quay = { 148, 151, 137, 255 };
    for (int side = -1; side <= 1; side += 2) {
        basin_box(frame,
            (Transform3D) { .position = { side * (BOAT_BASIN_HALF_X + 2), 0, 0 },
                .scale = { 4, 1.8f, 2 * BOAT_BASIN_HALF_Z + 8 } },
            quay);
        basin_box(frame,
            (Transform3D) { .position = { 0, 0, side * (BOAT_BASIN_HALF_Z + 2) },
                .scale = { 2 * BOAT_BASIN_HALF_X, 1.8f, 4 } },
            quay);
        if (!boat->course.enabled) {
            basin_box(frame,
                (Transform3D) { .position = { side * 12, .4f, 0 }, .scale = { .8f, 1, .8f } },
                (Color) { 232, 150, 65, 255 });
            basin_box(frame,
                (Transform3D) { .position = { side * 12, 1.1f, 0 }, .scale = { .25f, .5f, .25f } },
                (Color) { 248, 225, 175, 255 });
        }
    }
    if (!boat_water_displaced(boat->water_settings))
        for (int z = -(int)BOAT_BASIN_HALF_Z + 4; z < BOAT_BASIN_HALF_Z; z += 8)
            for (int x = -(int)BOAT_BASIN_HALF_X + 4; x < BOAT_BASIN_HALF_X; x += 8)
                basin_box(frame,
                    (Transform3D) {
                        .position = { x + (z % 4), .01f, z }, .scale = { 1.3f, .04f, .12f } },
                    (Color) { 62, 132, 142, 255 });
    boat_course_visit(&boat->course, basin_box, frame);
    environment_visit(boat, basin_box, frame);
}

void boat_scene_frame(const Boat *boat, BoatCamera camera, WaterfrontSceneFrame *frame)
{
    _Static_assert(BOAT_WATER_PARTICLES <= WATERFRONT_PLAYER_PARTICLES, "Player particle capacity");
    waterfront_scene_evaluate(0, true, frame);
    frame->camera = boat_camera(boat, camera);
    waterfront_scene_evaluate_actors(boat->time, boat->water_settings, frame);
    frame->player_active = true;
    if (!boat->eriksberg)
        basin_scenery(boat, frame);
    else
        environment_visit(boat, basin_box, frame);
    frame->player_water = boat->water_settings;
    WaterfrontPlayerState *p = &frame->player;
    boat_player_pose(boat, p);
    if (boat->eriksberg)
        waterfront_boating_gulls(boat->time, p->hull.position,
            (Vector3) { -boat->velocity.x, 0, boat->velocity.y }, frame);
    for (int i = 0; i < BOAT_WATER_PARTICLES; ++i) {
        const BoatWaterParticle *water = &boat->water[i];
        if (water->lifetime <= 0 || (boat->eriksberg && !boat_map_water(water->position, 0)))
            continue;
        float size = water_particle_size(water);
        p->particles[p->particle_count++]
            = (WaterfrontPlayerParticle) { .transform = water_particle_pose(boat, water, size),
                  .color = water_particle_color(boat, water),
                  .spray = water->spray };
    }
}

bool boat_draw(Renderer3D *renderer, Color *pixels, int width, int height, const Boat *boat,
    BoatCamera camera_settings)
{
    return boat_draw_with_ghost(renderer, pixels, width, height, boat, camera_settings, NULL, 0);
}

bool boat_draw_with_ghost(Renderer3D *renderer, Color *pixels, int width, int height,
    const Boat *boat, BoatCamera camera_settings, const Boat *ghost, float opacity)
{
    for (int i = 0; i < width * height; ++i)
        pixels[i] = (Color) { 21, 36, 48, 255 };
    View3D camera = boat_camera(boat, camera_settings);
    if (boat->eriksberg) {
        if (!waterfront_draw_boating(renderer, pixels, width, height, camera,
                boat_water_displaced(boat->water_settings)))
            return false;
    } else {
        if (!renderer3d_begin(renderer, pixels, width, height, camera, (Vector3) { -1, 3, 2 }))
            return false;

        if (!boat_water_displaced(boat->water_settings)) {
            box(renderer, (Vector3) { 0, -0.6f, 0 },
                (Vector3) { 2 * BOAT_BASIN_HALF_X, 1, 2 * BOAT_BASIN_HALF_Z },
                (Color) { 36, 107, 125, 255 });
            // Static water marks provide a speed reference.
            for (int z = -(int)BOAT_BASIN_HALF_Z + 4; z < BOAT_BASIN_HALF_Z; z += 8)
                for (int x = -(int)BOAT_BASIN_HALF_X + 4; x < BOAT_BASIN_HALF_X; x += 8)
                    box(renderer, (Vector3) { x + (z % 4), -0.08f, z },
                        (Vector3) { 1.3f, 0.04f, 0.12f }, (Color) { 62, 132, 142, 255 });
        }
    }
    if (boat_water_displaced(boat->water_settings)) {
        const Mesh3D *surface
            = boat_water_surface(boat->water_settings, boat->time, boat->eriksberg);
        if (!surface)
            return false;
        renderer3d_draw_emissive_mesh(renderer, surface, (Transform3D) { .scale = { 1, 1, 1 } });
    }
    if (boat->eriksberg) {
        WaterfrontTrafficFrame traffic;
        waterfront_traffic_evaluate(boat->water_settings, boat->time, &traffic);
        waterfront_draw_traffic(renderer, &traffic);
        WaterfrontHarborFrame harbor;
        waterfront_harbor_evaluate(boat->water_settings, boat->time, &harbor);
        waterfront_draw_harbor(renderer, &harbor);
        WaterfrontSceneFrame birds = { 0 };
        waterfront_boating_gulls(boat->time,
            (Vector3) { -boat->position.x, boat->height, boat->position.y },
            (Vector3) { -boat->velocity.x, 0, boat->velocity.y }, &birds);
        for (int i = 0; i < birds.gull_count; ++i)
            for (int part = 0; part < WATERFRONT_GULL_PARTS; ++part)
                renderer3d_draw_emissive_mesh(
                    renderer, waterfront_gull_mesh(part), birds.gulls[i][part]);
    }
    for (int i = 0; i < BOAT_WATER_PARTICLES; ++i) {
        const BoatWaterParticle *p = &boat->water[i];
        if (p->lifetime <= 0)
            continue;
        if (boat->eriksberg && !boat_map_water(p->position, 0))
            continue;
        renderer3d_draw_box(renderer, water_particle_pose(boat, p, water_particle_size(p)),
            water_particle_color(boat, p));
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
        for (int side = -1; !boat->course.enabled && side <= 1; side += 2) {
            box(renderer, (Vector3) { side * 12, 0.4f, 0 }, (Vector3) { 0.8f, 1, 0.8f },
                (Color) { 232, 150, 65, 255 });
            box(renderer, (Vector3) { side * 12, 1.1f, 0 }, (Vector3) { 0.25f, 0.5f, 0.25f },
                (Color) { 248, 225, 175, 255 });
        }
    }
    environment_visit(boat, environment_draw, renderer);
    boat_course_draw(&boat->course, renderer);
    WaterfrontPlayerState player = { 0 };
    boat_player_pose(boat, &player);
    renderer3d_draw_mesh(renderer, waterfront_player_hull_mesh(), player.hull);
    renderer3d_draw_mesh(renderer, waterfront_player_cabin_mesh(), player.cabin);
    renderer3d_draw_mesh(renderer, waterfront_player_engine_mesh(), player.engine);
    if (ghost && opacity > 0) {
        boat_player_pose(ghost, &player);
        renderer->ghost_opacity = opacity;
        renderer3d_draw_mesh(renderer, waterfront_player_hull_mesh(), player.hull);
        renderer3d_draw_mesh(renderer, waterfront_player_cabin_mesh(), player.cabin);
        renderer3d_draw_mesh(renderer, waterfront_player_engine_mesh(), player.engine);
        renderer->ghost_opacity = 0;
    }
    return true;
}
