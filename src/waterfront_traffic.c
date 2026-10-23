#include "waterfront_traffic.h"

#include "raymath.h"

#include <math.h>

static const BoatModelKind kinds[WATERFRONT_TRAFFIC_VESSELS]
    = { BOAT_MODEL_RUNABOUT, BOAT_MODEL_CRUISER, BOAT_MODEL_SAILBOAT, BOAT_MODEL_FERRY };
static const float dock_speed = 2.2f;
static const Vector2 river_along = { .8f, .6f }, river_across = { -.6f, .8f };
static BoatModel models[WATERFRONT_TRAFFIC_VESSELS];
static bool initialized;

BoatModelSpec waterfront_traffic_spec(int index)
{
    if (index < 0 || index >= WATERFRONT_TRAFFIC_VESSELS)
        return (BoatModelSpec) { 0 };
    BoatModelSpec spec = boat_model_preset(kinds[index]);
    if (index == 0) {
        spec.hull = (Color) { 227, 154, 60, 255 };
        spec.trim = (Color) { 242, 227, 196, 255 };
    }
    return spec;
}

const Mesh3D *waterfront_traffic_mesh(int index)
{
    if (index < 0 || index >= WATERFRONT_TRAFFIC_VESSELS)
        return NULL;
    if (!initialized) {
        for (int i = 0; i < WATERFRONT_TRAFFIC_VESSELS; ++i)
            boat_model_generate(&models[i], waterfront_traffic_spec(i));
        initialized = true;
    }
    return &models[index].mesh;
}

double waterfront_traffic_period(int index)
{
    if (index < 0 || index >= WATERFRONT_TRAFFIC_VESSELS)
        return 0;
    return index == 0 ? (260 + 28 * (double)PI) / dock_speed : index == 3 ? 320 : 260;
}

static double wrapped(double value, double period)
{
    value = fmod(value, period);
    return value < 0 ? value + period : value;
}

WaterfrontVesselMotion waterfront_traffic_motion(int index, double time)
{
    WaterfrontVesselMotion out = { 0 };
    if (index < 0 || index >= WATERFRONT_TRAFFIC_VESSELS || !isfinite(time))
        return out;
    Vector2 p, v, a = { 0 };
    if (index == 0) {
        // Two straight lanes with 14 m semicircular turns, entirely south of Dockbron.
        double distance = wrapped((double)time * dock_speed + 70, 260 + 28 * (double)PI);
        float arc = 14 * PI;
        if (distance < 130) {
            p = (Vector2) { 2, -45 + (float)distance };
            v = (Vector2) { 0, dock_speed };
        } else if (distance < 130 + arc) {
            float u = ((float)distance - 130) / 14;
            p = (Vector2) { 16 - 14 * cosf(u), 85 + 14 * sinf(u) };
            v = (Vector2) { dock_speed * sinf(u), dock_speed * cosf(u) };
            a = (Vector2) { dock_speed * dock_speed / 14 * cosf(u),
                -dock_speed * dock_speed / 14 * sinf(u) };
        } else if (distance < 260 + arc) {
            p = (Vector2) { 30, 85 - ((float)distance - 130 - arc) };
            v = (Vector2) { 0, -dock_speed };
        } else {
            float u = ((float)distance - 260 - arc) / 14;
            p = (Vector2) { 16 + 14 * cosf(u), -45 - 14 * sinf(u) };
            v = (Vector2) { -dock_speed * sinf(u), -dock_speed * cosf(u) };
            a = (Vector2) { -dock_speed * dock_speed / 14 * cosf(u),
                dock_speed * dock_speed / 14 * sinf(u) };
        }
    } else {
        // Nested river circuits keep the ferry and smaller vessels in separate corridors.
        float long_radius = index == 3 ? 300 : 210, short_radius = index == 3 ? 65 : 28;
        double rate = 2 * (double)PI / waterfront_traffic_period(index);
        float u
            = (float)wrapped((double)time * rate + (index == 2 ? PI / 2 : -PI / 2), 2 * (double)PI);
        Vector2 local = { long_radius * cosf(u), short_radius * sinf(u) };
        Vector2 first
            = { -long_radius * sinf(u) * (float)rate, short_radius * cosf(u) * (float)rate };
        Vector2 second = { -local.x * (float)(rate * rate), -local.y * (float)(rate * rate) };
        p = Vector2Add((Vector2) { 50, -290 },
            Vector2Add(Vector2Scale(river_along, local.x), Vector2Scale(river_across, local.y)));
        v = Vector2Add(Vector2Scale(river_along, first.x), Vector2Scale(river_across, first.y));
        a = Vector2Add(Vector2Scale(river_along, second.x), Vector2Scale(river_across, second.y));
    }
    out.position = (Vector3) { p.x, 0, p.y };
    out.velocity = (Vector3) { v.x, 0, v.y };
    out.heading = atan2f(-v.x, -v.y);
    out.yaw_rate = (v.y * a.x - v.x * a.y) / Vector2LengthSqr(v);
    return out;
}

static Vector3 wake_point(int index, double time, float age, int side)
{
    BoatModelSpec spec = waterfront_traffic_spec(index);
    WaterfrontVesselMotion motion = waterfront_traffic_motion(index, time - age);
    Vector3 forward = Vector3Normalize(motion.velocity);
    Vector3 right = { -forward.z, 0, forward.x };
    return Vector3Add(Vector3Subtract(motion.position, Vector3Scale(forward, spec.length * .54f)),
        Vector3Scale(right, side * (spec.beam * .42f + age * .38f)));
}

void waterfront_traffic_evaluate(BoatWater water, double time, WaterfrontTrafficFrame *frame)
{
    *frame = (WaterfrontTrafficFrame) { 0 };
    for (int i = 0; i < WATERFRONT_TRAFFIC_VESSELS; ++i) {
        BoatModelSpec spec = waterfront_traffic_spec(i);
        WaterfrontVesselState *vessel = &frame->vessels[i];
        vessel->id = 3000 + i;
        vessel->motion = waterfront_traffic_motion(i, time);
        vessel->pose = boat_water_pose(
            water, time, vessel->motion.position, vessel->motion.heading, spec.length, spec.beam);
        for (int light = 0; light < WATERFRONT_TRAFFIC_LIGHTS; ++light)
            vessel->lights[light] = boat_model_point(vessel->pose,
                (Vector3) { light == 2 ? 0 : (light ? 1 : -1) * spec.beam * .42f,
                    spec.freeboard + .32f, light == 2 ? spec.length * .42f : -spec.length * .12f });
        for (int side = -1; side <= 1; side += 2)
            for (int j = 0; j < WATERFRONT_TRAFFIC_WAKE_SEGMENTS; ++j) {
                float age = j * .7f;
                Vector3 a = wake_point(i, time, age, side);
                Vector3 b = wake_point(i, time, age + .7f, side);
                Vector3 center = Vector3Scale(Vector3Add(a, b), .5f);
                float length = Vector3Distance(a, b);
                Vector3 direction = Vector3Subtract(b, a);
                float yaw = atan2f(-direction.x, -direction.z);
                float width = i == 3 ? .48f : .22f;
                Transform3D pose = boat_water_pose(water, time, center, yaw, length, width);
                pose.position.y += .035f;
                pose.scale = (Vector3) { width, 1, length };
                float fade
                    = (1 - (j + .5f) / WATERFRONT_TRAFFIC_WAKE_SEGMENTS) * (i == 2 ? .65f : 1);
                frame->wakes[frame->wake_count++] = (WaterfrontTrafficWake) { .pose = pose,
                    .color
                    = { (unsigned char)Lerp(45, 188, fade), (unsigned char)Lerp(79, 207, fade),
                        (unsigned char)Lerp(104, 202, fade), 255 } };
            }
    }
}

// SAT uses both hull frames, avoiding centre-point or corner-sample tunnelling at crossings.
static bool rectangles(Vector2 delta, Vector2 player_forward, Vector2 player_right,
    Vector2 vessel_forward, Vector2 vessel_right, Vector2 player_half, Vector2 vessel_half,
    Vector2 *normal, float *penetration)
{
    Vector2 axes[] = { player_forward, player_right, vessel_forward, vessel_right };
    *penetration = INFINITY;
    for (int i = 0; i < 4; ++i) {
        Vector2 axis = axes[i];
        float radius = player_half.x * fabsf(Vector2DotProduct(player_right, axis))
            + player_half.y * fabsf(Vector2DotProduct(player_forward, axis))
            + vessel_half.x * fabsf(Vector2DotProduct(vessel_right, axis))
            + vessel_half.y * fabsf(Vector2DotProduct(vessel_forward, axis));
        float distance = Vector2DotProduct(delta, axis);
        float overlap = radius - fabsf(distance);
        if (overlap <= 0)
            return false;
        if (overlap < *penetration) {
            *penetration = overlap;
            *normal = Vector2Scale(axis, distance < 0 ? -1 : 1);
        }
    }
    return true;
}

bool waterfront_traffic_contact(Vector2 position, float heading, float bow, float stern,
    float half_beam, double time, WaterfrontTrafficContact *contact)
{
    return waterfront_traffic_contact_height(position, heading, bow, stern, half_beam, -INFINITY,
        INFINITY, (BoatWater) { 0 }, time, contact);
}

bool waterfront_traffic_contact_height(Vector2 position, float heading, float bow, float stern,
    float half_beam, float bottom, float top, BoatWater water, double time,
    WaterfrontTrafficContact *contact)
{
    Vector2 forward = { sinf(heading), -cosf(heading) }, right = { cosf(heading), sinf(heading) };
    float mid = (bow - stern) * .5f;
    Vector2 center = Vector2Add(position, Vector2Scale(forward, mid));
    for (int i = 0; i < WATERFRONT_TRAFFIC_VESSELS; ++i) {
        BoatModelSpec spec = waterfront_traffic_spec(i);
        WaterfrontVesselMotion motion = waterfront_traffic_motion(i, time);
        Vector2 ship = { -motion.position.x, motion.position.z };
        Vector2 vf = { sinf(motion.heading), -cosf(motion.heading) };
        Vector2 vr = { cosf(motion.heading), sinf(motion.heading) };
        Vector2 normal;
        float depth;
        if (!rectangles(Vector2Subtract(center, ship), forward, right, vf, vr,
                (Vector2) { half_beam + .15f, (bow + stern) * .5f + .15f },
                (Vector2) { spec.beam * .5f + .25f, spec.length * .5f + .5f }, &normal, &depth))
            continue;
        Transform3D pose
            = boat_water_pose(water, time, motion.position, motion.heading, spec.length, spec.beam);
        float low, high;
        boat_model_vertical_bounds(spec, pose, &low, &high);
        if (bottom > high || top < low)
            continue;
        float nr = Vector2DotProduct(normal, right), nf = Vector2DotProduct(normal, forward);
        Vector2 local = { fabsf(nr) < .001f ? 0 : -copysignf(half_beam, nr),
            fabsf(nf) < .001f ? mid
                : nf < 0      ? bow
                              : -stern };
        Vector2 point = Vector2Add(
            position, Vector2Add(Vector2Scale(right, local.x), Vector2Scale(forward, local.y)));
        Vector2 arm = Vector2Subtract(point, ship);
        if (contact)
            *contact = (WaterfrontTrafficContact) { .vessel = i,
                .normal = normal,
                .local = local,
                .penetration = depth,
                .velocity = { -motion.velocity.x - motion.yaw_rate * arm.y,
                    motion.velocity.z + motion.yaw_rate * arm.x } };
        return true;
    }
    return false;
}

bool waterfront_traffic_camera_clear(Vector3 p, double time, float clearance)
{
    for (int i = 0; i < WATERFRONT_TRAFFIC_VESSELS; ++i) {
        BoatModelSpec spec = waterfront_traffic_spec(i);
        WaterfrontVesselMotion m = waterfront_traffic_motion(i, time);
        Vector3 q = Vector3Transform(Vector3Subtract(p, m.position), MatrixRotateY(-m.heading));
        float top = fmaxf(spec.mast_height,
            spec.freeboard + spec.cabin_height + (spec.passenger_deck ? 2.6f : .8f));
        if (fabsf(q.x) < spec.beam * .5f + clearance
            && fabsf(q.z) < spec.length * .5f + 1 + clearance && q.y > -spec.draft - clearance
            && q.y < top + clearance)
            return false;
    }
    return true;
}
