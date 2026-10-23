#include "boat_presentation.h"
#include "boat_pose.h"
#include "raymath.h"

#include <math.h>

void boat_presentation_reset(BoatPresentation *presentation)
{
    *presentation = (BoatPresentation) { 0 };
}

void boat_presentation_step_flag(BoatFlag *flag, const Boat *boat, double time)
{
    boat_flag_step(flag, boat_pose_hull(boat), boat_pose_velocity(boat), time);
}

// Integer hash gives each foam birth its own identity without disturbing spray variation.
static float wake_variation(BoatPresentation *presentation)
{
    unsigned int bits = ++presentation->wake_emission * 747796405u + 2891336453u;
    bits = ((bits >> ((bits >> 28u) + 4u)) ^ bits) * 277803737u;
    return (float)((bits >> 22u) & 1023u) / 1023;
}

static void water_emit(
    BoatPresentation *presentation, const BoatWorld *world, double time, BoatWaterParticle particle)
{
    if (!particle.spray)
        particle.height
            = boat_water_query_field(world->water, world->wake_field, particle.position, time)
                  .height
            + .015f;
    // Prefer expired slots. Foam may replace foam, but never shorten airborne spray.
    for (unsigned int offset = 0; offset < BOAT_WATER_PARTICLES; ++offset) {
        unsigned int slot = (presentation->next_particle + offset) % BOAT_WATER_PARTICLES;
        if (presentation->water[slot].lifetime <= 0) {
            presentation->water[slot] = particle;
            presentation->next_particle = (slot + 1) % BOAT_WATER_PARTICLES;
            return;
        }
    }
    for (unsigned int offset = 0; offset < BOAT_WATER_PARTICLES; ++offset) {
        unsigned int slot = (presentation->next_particle + offset) % BOAT_WATER_PARTICLES;
        if (!presentation->water[slot].spray) {
            presentation->water[slot] = particle;
            presentation->next_particle = (slot + 1) % BOAT_WATER_PARTICLES;
            return;
        }
    }
}

static void water_step(
    BoatPresentation *presentation, const Boat *boat, const BoatWorld *world, double time)
{
    for (int i = 0; i < BOAT_WATER_PARTICLES; ++i) {
        BoatWaterParticle *p = &presentation->water[i];
        if (p->lifetime <= 0)
            continue;
        p->age += BOAT_STEP;
        Vector2 velocity = p->velocity;
        if (!p->spray)
            velocity = Vector2Add(velocity, boat_water_current(world->water, p->position));
        p->position = Vector2Add(p->position, Vector2Scale(velocity, BOAT_STEP));
        if (p->spray) {
            p->lift -= 12 * BOAT_STEP;
            p->height += p->lift * BOAT_STEP;
        }
        float surface
            = boat_water_query_field(world->water, world->wake_field, p->position, time).height;
        if (!p->spray)
            p->height = surface + .015f;
        if (p->age >= p->lifetime || (p->spray && p->height < surface)
            || (!world->definition->mapped
                && (fabsf(p->position.x) > world->definition->half_extent.x - 0.3f
                    || fabsf(p->position.y) > world->definition->half_extent.y - 0.3f)))
            p->lifetime = 0;
    }

    Vector2 relative
        = Vector2Subtract(boat->velocity, boat_water_current(world->water, boat->position));
    float speed = Vector2Length(relative);
    Vector2 right = { cosf(boat->heading), sinf(boat->heading) };
    Vector3 stern_world = boat_pose_attachment(boat, (Vector3) { 0, 0, BOAT_ENGINE_ARM });
    Vector2 stern = { -stern_world.x, stern_world.z };
    // Propeller wash moves opposite the force applied to the stern, even before the hull moves.
    if (fabsf(boat->engine_response) > 0.02f) {
        presentation->propeller_time += BOAT_STEP;
        if (presentation->propeller_time >= 0.075f) {
            presentation->propeller_time -= 0.075f;
            float variation = wake_variation(presentation);
            float power = fabsf(boat->engine_response);
            // Unequal lobes separate as they travel aft, opening gaps instead of a solid stripe.
            for (int side = -1; side <= 1; side += 2) {
                float v = side < 0 ? variation : wake_variation(presentation);
                water_emit(presentation, world, time,
                    (BoatWaterParticle) { .position
                        = Vector2Add(stern, Vector2Scale(right, side * (.12f + .18f * v))),
                        .velocity = Vector2Add(Vector2Scale(boat_pose_thrust_direction(boat),
                                                   -(3.0f + 3.0f * v) * boat->engine_response),
                            Vector2Scale(right, side * (.3f + .65f * v) * power)),
                        .lifetime = .55f + .65f * v,
                        .size = (.28f + .38f * v) * (.5f + .5f * power),
                        .wash = true,
                        .variation = v });
            }
        }
    } else {
        presentation->propeller_time = 0;
    }
    if (speed > 0.8f) {
        // Distance spacing leaves a curved history in the water, independent of render rate.
        presentation->wake_distance += speed * BOAT_STEP;
        float spacing = .48f + .24f * (float)(presentation->wake_emission % 7u) / 6;
        if (presentation->wake_distance >= spacing) {
            presentation->wake_distance -= spacing;
            for (int side = -1; side <= 1; side += 2) {
                float v = wake_variation(presentation);
                water_emit(presentation, world, time,
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
        presentation->wake_distance = 0;
    }

    // Planing adds symmetric low spray in straight running; turning slip still favours one side.
    float slip = Vector2DotProduct(relative, right);
    bool planing_spray = boat->planing > .05f && speed > 2;
    bool wave_spray = boat->wave_spray > .08f && speed > 2;
    if ((fabsf(slip) > 0.9f && speed > 2) || planing_spray || wave_spray) {
        presentation->spray_time += BOAT_STEP;
        if (presentation->spray_time >= 0.035f) {
            presentation->spray_time -= 0.035f;
            float strength = fabsf(slip) > .9f ? 1 : fmaxf(boat->planing, boat->wave_spray);
            int first_side = fabsf(slip) > .9f ? (slip > 0 ? 1 : -1) : -1;
            int last_side = fabsf(slip) > .9f ? first_side : 1;
            // Deterministic variation keeps screenshots repeatable and avoids shared RNG state.
            float variation = (float)((++presentation->emission * 37u) % 101u) / 100;
            for (int side = first_side; side <= last_side; side += 2) {
                Vector2 out = Vector2Scale(right, side);
                Vector3 origin
                    = boat_pose_attachment(boat, (Vector3) { -side * 1.05f, .25f, -.4f });
                BoatPoseMotion motion
                    = boat_pose_motion(boat, (Vector3) { -side * 1.05f, .25f, -.4f });
                water_emit(presentation, world, time,
                    (BoatWaterParticle) { .position = { -origin.x, origin.z },
                        .velocity = Vector2Add(Vector2Scale(boat->velocity, 0.25f),
                            Vector2Scale(out,
                                1.5f + fabsf(slip) * (0.45f + variation * 0.4f) + boat->planing)),
                        .height = fmaxf(boat_water_query_field(world->water, world->wake_field,
                                            (Vector2) { -origin.x, origin.z }, time)
                                            .height
                                + .03f,
                            origin.y),
                        .lift = (2.0f + variation * 2 + fminf(fabsf(slip) * 0.25f, 1.5f)) * strength
                            + motion.velocity.y
                            + (boat_world_displaced(world) ? boat->wave_spray : 0),
                        .lifetime = 1.1f,
                        .size = (.22f + variation * .18f) * Lerp(1, 1.3f, boat->planing) * strength,
                        .spray = true });
            }
        }
    } else {
        presentation->spray_time = 0;
    }
}

void boat_presentation_step(
    BoatPresentation *presentation, const Boat *boat, const BoatWorld *world, double now)
{
    // Heading is final once contacts resolve; buoyancy and the cloth leave it unchanged.
    float camera_delta = remainderf(boat->heading - presentation->camera_heading, 2 * PI);
    presentation->camera_heading = remainderf(
        presentation->camera_heading + camera_delta * (1 - expf(-6 * BOAT_STEP)), 2 * PI);
    boat_presentation_step_flag(&presentation->flag, boat, now);
    water_step(presentation, boat, world, now);
}
