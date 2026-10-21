#include "boat_water.h"
#include "boat_model.h"
#include "raymath.h"
#include "waterfront_scene.h"

#include <math.h>
#include <stdlib.h>

static float pose_height(BoatWater water, Vector3 p, float time)
{
    return boat_water_query(water, (Vector2) { -p.x, p.z }, time).height;
}

Transform3D boat_water_pose(
    BoatWater water, float time, Vector3 p, float yaw, float length, float beam)
{
    Matrix heading = MatrixRotateY(yaw);
    Vector3 fore = Vector3Add(p, Vector3Transform((Vector3) { 0, 0, -length * .35f }, heading));
    Vector3 aft = Vector3Add(p, Vector3Transform((Vector3) { 0, 0, length * .35f }, heading));
    Vector3 left = Vector3Add(p, Vector3Transform((Vector3) { -beam * .35f, 0, 0 }, heading));
    Vector3 right = Vector3Add(p, Vector3Transform((Vector3) { beam * .35f, 0, 0 }, heading));
    float f = pose_height(water, fore, time), a = pose_height(water, aft, time);
    float l = pose_height(water, left, time), r = pose_height(water, right, time);
    p.y = (f + a + l + r) * .25f;
    float pitch = atanf((f - a) / (length * .7f));
    float roll = atanf((r - l) / (beam * .7f));
    Matrix rotation
        = MatrixMultiply(MatrixMultiply(MatrixRotateZ(roll), MatrixRotateX(pitch)), heading);
    return (Transform3D) {
        .position = p, .rotation = boat_model_rotation(rotation), .scale = { 1, 1, 1 }
    };
}

// Three low crossing components. Frequencies approximate deep-water dispersion at this scale.
BoatWater boat_water_gentle(void)
{
    return (BoatWater) { .amplitude = .13f,
        .wavelength = 16,
        .angular_speed = 1.96f,
        .direction = { .6f, -.8f },
        .crossing = { { .amplitude = .06f,
                          .wavelength = 9,
                          .angular_speed = 2.62f,
                          .direction = { -.8f, -.6f } },
            { .amplitude = .025f,
                .wavelength = 5.5f,
                .angular_speed = 3.35f,
                .direction = { .96f, .28f } } } };
}

BoatWater boat_water_test(bool waves, bool current, bool wake)
{
    BoatWater water = waves ? boat_water_gentle() : (BoatWater) { 0 };
    if (current) {
        water.current = (Vector2) { .85f, -.25f };
        water.current_width = 32;
    }
    water.wake_amplitude = wake ? .38f : 0;
    return water;
}

bool boat_water_displaced(BoatWater water)
{
    return water.amplitude != 0 || water.wake_amplitude != 0 || water.player_wake_enabled;
}

void boat_water_emit_wake(
    BoatWater *water, Vector2 start, Vector2 end, float emitted, float duration, float speed)
{
    Vector2 delta = Vector2Subtract(end, start);
    float distance = Vector2Length(delta);
    if (!water->player_wake_enabled || speed <= 2 || distance < .2f || duration <= 0)
        return;
    water->player_wake[water->player_wake_next]
        = (BoatWakeSegment) { .position = Vector2Scale(Vector2Add(start, end), .5f),
              .direction = Vector2Scale(delta, 1 / distance),
              .length = fmaxf(distance, 1.2f),
              .emitted = emitted - duration * .5f,
              .amplitude = .65f * Clamp((speed - 2) / 10, 0, 1),
              .inverse_speed = duration / distance };
    water->player_wake_next = (water->player_wake_next + 1) % BOAT_WAKE_SEGMENTS;
    if (water->player_wake_count < BOAT_WAKE_SEGMENTS)
        ++water->player_wake_count;
}

int boat_water_wake_uniforms(BoatWater water, float time, float *origins, float *parameters)
{
    int count = 0;
    if (!water.player_wake_enabled)
        return 0;
    for (int i = 0; i < water.player_wake_count; ++i) {
        BoatWakeSegment s = water.player_wake[i];
        if (time - s.emitted > BOAT_WAKE_LIFETIME + 1)
            continue;
        float a[] = { -s.position.x, s.position.y, -s.direction.x, s.direction.y };
        float b[] = { s.length, s.emitted, s.amplitude, s.inverse_speed };
        for (int j = 0; j < 4; ++j) {
            origins[count * 4 + j] = a[j];
            parameters[count * 4 + j] = b[j];
        }
        ++count;
    }
    return count;
}

Vector2 boat_water_current(BoatWater water, Vector2 position)
{
    float weight = 1;
    if (water.current_width > 0) {
        float x = (position.x - water.current_center) / water.current_width;
        weight = fmaxf(0, 1 - x * x);
        weight *= weight;
    }
    return Vector2Scale(water.current, weight);
}

bool boat_water_wake_source(BoatWater water, float time, Vector2 *position)
{
    *position = Vector2Add(water.wake_offset, (Vector2) { -24 + 4 * time, -8 });
    return water.wake_amplitude != 0 && time >= 0 && time <= 12;
}

// Compact wave packet with analytic spatial/time derivatives. Keep the matching GLSL
// recipe in waterfront_water.vs/fs in sync. It is a scripted wake, not fluid simulation.
static void crossing_wake(
    BoatWater water, Vector2 p, float time, float *height, Vector2 *gradient, float *vertical_speed)
{
    if (water.wake_amplitude == 0)
        return;
    p = Vector2Subtract(p, water.wake_offset);
    float emitted = (p.x + 24) / 4;
    float age = time - emitted;
    if (emitted <= 0 || emitted >= 12 || age <= 0 || age >= 12)
        return;
    float radius = sqrtf((p.y + 8) * (p.y + 8) + .25f);
    float q = radius - 2.4f * age;
    if (fabsf(q) >= 5)
        return;
    float k = 2 * PI / 6;
    float edge = 1 - q * q / 25;
    float f = sinf(k * q) * edge * edge * edge;
    float df = k * cosf(k * q) * edge * edge * edge - sinf(k * q) * 6 * q / 25 * edge * edge;
    float window = sinf(PI * emitted / 12), envelope = sinf(PI * age / 12);
    float dw = PI / 12 * sinf(2 * PI * emitted / 12);
    float de = PI / 12 * sinf(2 * PI * age / 12);
    window *= window;
    envelope *= envelope;
    float a = water.wake_amplitude;
    *height += a * window * envelope * f;
    gradient->x += a / 4 * ((dw * envelope - window * de) * f + window * envelope * df * 2.4f);
    gradient->y += a * window * envelope * df * (p.y + 8) / radius;
    *vertical_speed += a * window * (de * f - envelope * df * 2.4f);
}

static float wake_smooth(float start, float end, float x, float *derivative)
{
    float t = Clamp((x - start) / (end - start), 0, 1);
    *derivative = 6 * t * (1 - t) / (end - start);
    return t * t * (3 - 2 * t);
}

// A path-aligned packet expands from both sides of each travelled segment. Smooth
// normalization bounds overlapping/looped wakes, including all derivative terms.
static void player_wake(const BoatWater *water, Vector2 p, float time, float *height,
    Vector2 *gradient, float *vertical_speed)
{
    if (!water->player_wake_enabled)
        return;
    float sum = 0, weight = 0, sum_t = 0, weight_t = 0;
    Vector2 sum_g = { 0 }, weight_g = { 0 };
    for (int i = 0; i < water->player_wake_count; ++i) {
        const BoatWakeSegment *s = &water->player_wake[i];
        float clock = time - s->emitted;
        if (clock < -1 || clock > BOAT_WAKE_LIFETIME + 1)
            continue;
        Vector2 delta = Vector2Subtract(p, s->position);
        Vector2 normal = { -s->direction.y, s->direction.x };
        float u = Vector2DotProduct(delta, s->direction);
        if (fabsf(u) >= s->length)
            continue;
        float age = clock - u * s->inverse_speed;
        if (age <= .4f || age >= BOAT_WAKE_LIFETIME)
            continue;
        float v = Vector2DotProduct(delta, normal);
        float radius = sqrtf(v * v + .25f);
        float q = radius - .9f - 1.6f * age;
        if (fabsf(q) >= 4.5f)
            continue;
        float a = 1 - u * u / (s->length * s->length), b = 1 - q * q / 20.25f;
        float wl = a * a * a, wq = b * b * b;
        float dl = -6 * u / (s->length * s->length) * a * a;
        float dq = -6 * q / 20.25f * b * b;
        Vector2 age_g = Vector2Scale(s->direction, -s->inverse_speed);
        Vector2 q_g = Vector2Subtract(Vector2Scale(normal, v / radius), Vector2Scale(age_g, 1.6f));
        float w = wl * wq, wt = -1.6f * wl * dq;
        Vector2 wg = Vector2Add(Vector2Scale(s->direction, dl * wq), Vector2Scale(q_g, wl * dq));
        float dr, df;
        float rise = wake_smooth(.4f, 1.4f, age, &dr);
        float fall = 1 - wake_smooth(14, BOAT_WAKE_LIFETIME, age, &df);
        float envelope = rise * fall, de = dr * fall - rise * df;
        // Crests outrun the packet: phase speed 3 m/s, envelope speed 1.6 m/s.
        float phase = q - 1.4f * age;
        Vector2 phase_g = Vector2Subtract(q_g, Vector2Scale(age_g, 1.4f));
        float k = 2 * PI / 6, wave = sinf(k * phase), slope = k * cosf(k * phase);
        float value = s->amplitude * envelope * wave;
        Vector2 value_g = Vector2Scale(
            Vector2Add(Vector2Scale(age_g, de * wave), Vector2Scale(phase_g, envelope * slope)),
            s->amplitude);
        sum += w * value;
        sum_g = Vector2Add(sum_g, Vector2Add(Vector2Scale(wg, value), Vector2Scale(value_g, w)));
        sum_t += wt * value + w * s->amplitude * (de * wave - 3.0f * envelope * slope);
        weight += w;
        weight_g = Vector2Add(weight_g, wg);
        weight_t += wt;
    }
    float divisor = sqrtf(1 + weight * weight);
    float correction = sum * weight / (1 + weight * weight);
    *height += sum / divisor;
    *gradient = Vector2Add(*gradient,
        Vector2Scale(Vector2Subtract(sum_g, Vector2Scale(weight_g, correction)), 1 / divisor));
    *vertical_speed += (sum_t - correction * weight_t) / divisor;
}

BoatWaterSample boat_water_query(BoatWater water, Vector2 position, float time)
{
    BoatWaterSample sample = { .normal = { 0, 1, 0 } };
    Vector2 current = boat_water_current(water, position);
    sample.velocity.x = current.x;
    sample.velocity.z = current.y;
    if (!boat_water_displaced(water))
        return sample;
    const BoatWave waves[]
        = { { water.amplitude, water.wavelength, water.angular_speed, water.direction },
              water.crossing[0], water.crossing[1] };
    Vector2 gradient = { 0 };
    for (int i = 0; i < 3; ++i) {
        BoatWave w = waves[i];
        if (w.amplitude == 0 || w.wavelength <= 0)
            continue;
        float k = 2 * PI / w.wavelength;
        float phase = k * Vector2DotProduct(w.direction, position) - w.angular_speed * time;
        float slope = w.amplitude * k * cosf(phase);
        sample.height += w.amplitude * sinf(phase);
        gradient = Vector2Add(gradient, Vector2Scale(w.direction, slope));
        sample.velocity.y -= w.amplitude * w.angular_speed * cosf(phase);
    }
    crossing_wake(water, position, time, &sample.height, &gradient, &sample.velocity.y);
    player_wake(&water, position, time, &sample.height, &gradient, &sample.velocity.y);
    // Material surface velocity includes the current's travel along the local slope.
    sample.velocity.y += Vector2DotProduct(current, gradient);
    sample.normal = Vector3Normalize((Vector3) { -gradient.x, 1, -gradient.y });
    // Horizontal velocity is the authored current; waves add vertical surface motion.
    return sample;
}

Vector3 boat_water_resistance(
    BoatWaterSample water, Vector3 point_velocity, float damping, float support)
{
    if (support <= 0 || damping <= 0 || water.normal.y <= 0)
        return (Vector3) { 0 };
    Vector3 relative = Vector3Subtract(point_velocity, water.velocity);
    float force = -damping * Vector3DotProduct(relative, water.normal);
    force = fmaxf(force, -support / water.normal.y);
    return Vector3Scale(water.normal, force);
}

BoatWaterSample boat_water_query_field(
    BoatWater water, const BoatWake *field, Vector2 position, float time)
{
    BoatWaterSample s = boat_water_query(water, position, time);
    if (!field)
        return s;
    BoatWakeSample q;
    if (!boat_wake_query(field, position.x, position.y, &q))
        return s;
    float gx = -s.normal.x / s.normal.y + (float)q.gradient_x;
    float gz = -s.normal.z / s.normal.y + (float)q.gradient_z;
    s.height += (float)q.height;
    s.normal = Vector3Normalize((Vector3) { -gx, 1, -gz });
    s.velocity = Vector3Add(s.velocity,
        (Vector3) { (float)q.velocity_x, (float)q.vertical_velocity, (float)q.velocity_z });
    return s;
}

typedef struct {
    Vector3 *vertices;
    Triangle3D *triangles;
    size_t count, capacity;
    Mesh3D mesh;
} Surface;
static Surface surfaces[2];
static Surface candidate_surface;
static Surface patches[2];
static Vector2 patch_centers[2];

static bool surface_triangle(Surface *s, Vector3 a, Vector3 b, Vector3 c)
{
    if (s->count == s->capacity) {
        size_t capacity = s->capacity ? s->capacity * 2 : 1024;
        Vector3 *v = realloc(s->vertices, capacity * 3 * sizeof(*v));
        if (!v)
            return false;
        s->vertices = v;
        Triangle3D *t = realloc(s->triangles, capacity * sizeof(*t));
        if (!t)
            return false;
        s->triangles = t;
        s->capacity = capacity;
    }
    unsigned int i = (unsigned int)(s->count * 3);
    s->vertices[i] = a;
    s->vertices[i + 1] = b;
    s->vertices[i + 2] = c;
    s->triangles[s->count++] = (Triangle3D) { .vertices = { i, i + 1, i + 2 } };
    return true;
}

static bool subdivide(Surface *s, Vector3 a, Vector3 b, Vector3 c, int depth)
{
    // CPU reference retains its original two-unit tessellation and displacement.
    float ab = Vector3DistanceSqr(a, b), bc = Vector3DistanceSqr(b, c);
    float ca = Vector3DistanceSqr(c, a);
    if (depth < 24 && fmaxf(ab, fmaxf(bc, ca)) > 4) {
        if (bc > ab && bc >= ca)
            return subdivide(s, b, c, a, depth);
        if (ca > ab && ca > bc)
            return subdivide(s, c, a, b, depth);
        Vector3 mid = Vector3Scale(Vector3Add(a, b), .5f);
        return subdivide(s, a, mid, c, depth + 1) && subdivide(s, mid, b, c, depth + 1);
    }
    return surface_triangle(s, a, b, c);
}

void boat_water_destroy(void)
{
    free(candidate_surface.vertices);
    free(candidate_surface.triangles);
    candidate_surface = (Surface) { 0 };
    for (int i = 0; i < 2; ++i) {
        free(surfaces[i].vertices);
        free(surfaces[i].triangles);
        surfaces[i] = (Surface) { 0 };
        free(patches[i].vertices);
        free(patches[i].triangles);
        patches[i] = (Surface) { 0 };
    }
}

// Clip a convex polygon against one axis-aligned cell edge, retaining input winding.
static int clip_edge(
    const Vector3 *input, int count, Vector3 *output, int axis, float edge, bool greater)
{
    int written = 0;
    if (!count)
        return 0;
    Vector3 a = input[count - 1];
    float da = (axis ? a.z : a.x) - edge;
    for (int i = 0; i < count; ++i) {
        Vector3 b = input[i];
        float db = (axis ? b.z : b.x) - edge;
        bool inside_a = greater ? da >= 0 : da <= 0;
        bool inside_b = greater ? db >= 0 : db <= 0;
        if (inside_a != inside_b) {
            Vector3 p = Vector3Lerp(a, b, da / (da - db));
            if (axis)
                p.z = edge;
            else
                p.x = edge;
            output[written++] = p;
        }
        if (inside_b)
            output[written++] = b;
        a = b;
        da = db;
    }
    return written;
}

static bool patch_triangle(Surface *s, Vector3 a, Vector3 b, Vector3 c, Vector2 origin)
{
    const float spacing = 1.5f;
    int min_z = (int)floorf((fminf(a.z, fminf(b.z, c.z)) - origin.y) / spacing);
    int max_z = (int)floorf((fmaxf(a.z, fmaxf(b.z, c.z)) - origin.y) / spacing);
    min_z = min_z < 0 ? 0 : min_z;
    max_z = max_z > 127 ? 127 : max_z;
    for (int z = min_z; z <= max_z; ++z) {
        float bottom = origin.y + z * spacing;
        Vector3 triangle[3] = { a, b, c }, strip[12], temporary[12];
        int strip_count = clip_edge(triangle, 3, temporary, 1, bottom, true);
        strip_count = clip_edge(temporary, strip_count, strip, 1, bottom + spacing, false);
        if (strip_count < 3)
            continue;
        // A row-clipped triangle often spans only a few cells even when its whole AABB
        // is large. Avoid clipping every empty cell in long diagonal source triangles.
        float min = strip[0].x, max = strip[0].x;
        for (int j = 1; j < strip_count; ++j) {
            min = fminf(min, strip[j].x);
            max = fmaxf(max, strip[j].x);
        }
        int min_x = (int)floorf((min - origin.x) / spacing);
        int max_x = (int)floorf((max - origin.x) / spacing);
        min_x = min_x < 0 ? 0 : min_x;
        max_x = max_x > 127 ? 127 : max_x;
        for (int x = min_x; x <= max_x; ++x) {
            float left = origin.x + x * spacing;
            Vector3 p[12], q[12];
            int count = clip_edge(strip, strip_count, q, 0, left, true);
            count = clip_edge(q, count, p, 0, left + spacing, false);
            for (int j = 1; j + 1 < count; ++j) {
                Vector3 ab = Vector3Subtract(p[j], p[0]);
                Vector3 ac = Vector3Subtract(p[j + 1], p[0]);
                if (fabsf(ab.x * ac.z - ab.z * ac.x) < 1e-7f)
                    continue;
                if (!surface_triangle(s, p[0], p[j], p[j + 1]))
                    return false;
            }
        }
    }
    return true;
}

const Mesh3D *boat_water_patch(Vector2 world_center, bool eriksberg)
{
    Surface *s = &patches[eriksberg];
    Vector2 center = { roundf(world_center.x / 12) * 12, roundf(world_center.y / 12) * 12 };
    if (s->count && Vector2Equals(center, patch_centers[eriksberg]))
        return &s->mesh;
    s->count = 0;
    Vector2 origin = { center.x - 96, center.y - 96 };
    bool ok = true;
    if (eriksberg) {
        const Mesh3D *city = waterfront_scene_city_mesh();
        for (size_t i = 0; ok && i < city->triangle_count; ++i) {
            Triangle3D t = city->triangles[i];
            Vector3 a = city->vertices[t.vertices[0]], b = city->vertices[t.vertices[1]];
            Vector3 c = city->vertices[t.vertices[2]];
            if (a.y != 0 || b.y != 0 || c.y != 0)
                break;
            ok = patch_triangle(s, a, b, c, origin);
        }
    } else {
        ok = patch_triangle(s, (Vector3) { -120, 0, -80 }, (Vector3) { -120, 0, 80 },
                 (Vector3) { 120, 0, 80 }, origin)
            && patch_triangle(s, (Vector3) { -120, 0, -80 }, (Vector3) { 120, 0, 80 },
                (Vector3) { 120, 0, -80 }, origin);
    }
    if (!ok) {
        s->count = 0;
        return NULL;
    }
    patch_centers[eriksberg] = center;
    s->mesh = (Mesh3D) { s->vertices, s->triangles, s->count };
    return &s->mesh;
}

const Mesh3D *boat_water_surface(BoatWater water, float time, bool eriksberg)
{
    Surface *s = &surfaces[eriksberg];
    if (!s->count) {
        bool ok = true;
        if (eriksberg) {
            const Mesh3D *city = waterfront_scene_city_mesh();
            for (size_t i = 0; ok && i < city->triangle_count; ++i) {
                Triangle3D t = city->triangles[i];
                Vector3 a = city->vertices[t.vertices[0]], b = city->vertices[t.vertices[1]];
                Vector3 c = city->vertices[t.vertices[2]];
                if (a.y != 0 || b.y != 0 || c.y != 0)
                    break; // Imported water triangles precede land.
                ok = subdivide(s, a, b, c, 0);
            }
        } else {
            ok = subdivide(s, (Vector3) { -120, 0, -80 }, (Vector3) { -120, 0, 80 },
                     (Vector3) { 120, 0, 80 }, 0)
                && subdivide(s, (Vector3) { -120, 0, -80 }, (Vector3) { 120, 0, 80 },
                    (Vector3) { 120, 0, -80 }, 0);
        }
        if (!ok) {
            boat_water_destroy();
            return NULL;
        }
    }
    BoatWater ambient = water;
    ambient.wake_amplitude = 0;
    ambient.player_wake_enabled = false;
    for (size_t i = 0; i < s->count; ++i) {
        Vector3 centre = { 0 };
        for (int j = 0; j < 3; ++j) {
            Vector3 *v = &s->vertices[i * 3 + j];
            v->y = boat_water_query(water, (Vector2) { -v->x, v->z }, time).height;
            centre = Vector3Add(centre, Vector3Scale(*v, 1.0f / 3));
        }
        BoatWaterSample q = boat_water_query(water, (Vector2) { -centre.x, centre.z }, time);
        // Restrained slope light; geometry and GPU reflections carry the wave shape.
        float light = Clamp(.95f + .55f * (-q.normal.x + q.normal.z), .85f, 1.05f);
        Color base = eriksberg ? (Color) { 45, 79, 104, 255 } : (Color) { 36, 107, 125, 255 };
        float crest = 0;
        if (water.wake_amplitude > 0 || water.player_wake_enabled)
            crest = .65f
                * Clamp(
                    (q.height
                        - boat_water_query(ambient, (Vector2) { -centre.x, centre.z }, time).height)
                        / .38f,
                    0, 1);
        s->triangles[i].color = (Color) { (unsigned char)Lerp(base.r * light, 166, crest),
            (unsigned char)Lerp(base.g * light, 199, crest),
            (unsigned char)Lerp(base.b * light, 204, crest), 255 };
    }
    s->mesh = (Mesh3D) { s->vertices, s->triangles, s->count };
    return &s->mesh;
}

const Mesh3D *boat_water_surface_field(BoatWater water, const BoatWake *field, float time)
{
    Surface *s = &candidate_surface;
    if (!s->count) {
        for (int z = 0; z < 215; ++z)
            for (int x = 0; x < 321; ++x) {
                float left = x == 0 ? -120 : -119.625f + (x - 1) * .75f;
                float right = x == 320 ? 120 : -119.625f + x * .75f;
                float bottom = z == 0 ? -80 : -79.875f + (z - 1) * .75f;
                float top = z == 214 ? 80 : -79.875f + z * .75f;
                Vector3 a = { left, 0, bottom }, b = { left, 0, top }, c = { right, 0, top },
                        d = { right, 0, bottom };
                if (!surface_triangle(s, a, b, c) || !surface_triangle(s, a, c, d)) {
                    s->count = 0; // Retry the complete basin after an allocation failure.
                    return NULL;
                }
            }
    }
    for (size_t i = 0; i < s->count; ++i) {
        Vector3 centre = { 0 };
        for (int j = 0; j < 3; ++j) {
            Vector3 *v = &s->vertices[3 * i + j];
            v->y = boat_water_query_field(water, field, (Vector2) { -v->x, v->z }, time).height;
            centre = Vector3Add(centre, Vector3Scale(*v, 1.0f / 3));
        }
        BoatWaterSample q
            = boat_water_query_field(water, field, (Vector2) { -centre.x, centre.z }, time);
        float light = Clamp(.95f + .55f * (-q.normal.x + q.normal.z), .85f, 1.05f);
        float crest = .65f * Clamp(q.height / .38f, 0, 1);
        s->triangles[i].color = (Color) { (unsigned char)Lerp(36 * light, 166, crest),
            (unsigned char)Lerp(107 * light, 199, crest),
            (unsigned char)Lerp(125 * light, 204, crest), 255 };
    }
    s->mesh = (Mesh3D) { s->vertices, s->triangles, s->count };
    return &s->mesh;
}
