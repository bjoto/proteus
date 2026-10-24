#include "boat_study_net.h"
#include "boat_skin.h"
#include "raylib.h"
#include "raymath.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>

static const char *phase_text(BoatNetClientPhase phase)
{
    static const char *names[]
        = { "connecting", "handshake", "loading world", "playing", "rejected", "closed" };
    return names[phase];
}

// Keep the reason for the title screen as well as the console.
static void fail(NetStudy *net, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(net->failure, sizeof(net->failure), format, arguments);
    va_end(arguments);
    fprintf(stderr, "%s\n", net->failure);
}

static void attach(NetStudy *net, BoatSession *session)
{
    // The session draws the predicted water; its objects pointer borrows that pool.
    session->world.water = boat_fleet_world(boat_net_client_view(&net->client)).water;
}

bool net_study_connect(
    NetStudy *net, const char *endpoint, const char *name, const char *token, BoatSession *session)
{
    char host[256], error[160];
    uint16_t port;
    *net = (NetStudy) { 0 };
    if (!boat_net_parse_endpoint(endpoint, host, sizeof(host), &port)) {
        fail(net, "The address needs HOST:PORT, for example 127.0.0.1:%d", BOAT_NET_PORT);
        return false;
    }
    if (!boat_net_enet_startup()) {
        fail(net, "Cannot initialize networking.");
        return false;
    }
    net->enet = boat_net_enet_connect(host, port, error, sizeof(error));
    if (!net->enet) {
        fail(net, "Cannot connect: %s", error);
        boat_net_enet_cleanup();
        return false;
    }
    BoatNetClientConfig config
        = { .name = name, .token = token, .build = "boat_study M4", .predict = true };
    boat_net_client_init(
        &net->client, boat_net_enet_transport(net->enet), &config, boat_net_clock());
    fprintf(stdout, "Connecting to %s:%u...\n", host, (unsigned)port);
    fflush(stdout);
    while (net->client.phase < BOAT_NET_CLIENT_PLAYING) {
        if (!boat_net_client_service(&net->client, boat_net_clock(), (BoatInput) { 0 }, true)) {
            if (net->client.phase == BOAT_NET_CLIENT_REJECTED)
                fail(net,
                    "Rejected by %s:%u: %s (server protocol %u, revision %u; this build %d, %d)",
                    host, (unsigned)port, boat_net_client_failure(&net->client),
                    (unsigned)net->client.reject.version,
                    (unsigned)net->client.reject.physics_revision, BOAT_NET_VERSION,
                    BOAT_PHYSICS_REVISION);
            else
                fail(net, "Cannot join %s:%u: %s", host, (unsigned)port,
                    boat_net_client_failure(&net->client));
            net_study_close(net);
            return false;
        }
        boat_net_enet_flush(net->enet);
        boat_net_enet_wait(net->enet, .01);
    }
    fprintf(stdout, "Joined as slot %u at tick %llu (%s water).\n", net->client.slot,
        (unsigned long long)net->client.fleet.state.tick,
        net->client.accept.waves ? "wave" : "calm");
    net->presented_tick = boat_net_client_view(&net->client)->state.tick;
    net->correction = net->client.prediction.correction;
    attach(net, session);
    boat_presentation_reset(&session->presentation);
    for (unsigned i = 0; i < BOAT_FLEET_SLOTS; ++i)
        boat_presentation_reset(&net->presentations[i]);
    return true;
}

// Fold the newest correction into the visual offsets, then decay them (about 95% in 120 ms).
static void blend_corrections(NetStudy *net, float dt)
{
    const BoatPrediction *p = &net->client.prediction;
    if (p->correction != net->correction) {
        net->correction = p->correction;
        for (unsigned i = 0; i < BOAT_FLEET_SLOTS; ++i) {
            BoatPredictionError *o = &net->offset[i];
            const BoatPredictionError *e = &p->error[i];
            *o = e->snap ? (BoatPredictionError) { 0 }
                         : (BoatPredictionError) { Vector2Add(o->position, e->position),
                               o->heading + e->heading, o->height + e->height, o->pitch + e->pitch,
                               o->roll + e->roll, false };
            if (Vector2Length(o->position) > BOAT_PREDICTION_SNAP_DISTANCE)
                *o = (BoatPredictionError) { 0 };
        }
    }
    float keep = expf(-dt / .04f);
    for (unsigned i = 0; i < BOAT_FLEET_SLOTS; ++i) {
        BoatPredictionError *o = &net->offset[i];
        *o = (BoatPredictionError) { Vector2Scale(o->position, keep), o->heading * keep,
            o->height * keep, o->pitch * keep, o->roll * keep, false };
    }
}

static Boat shown(const NetStudy *net, unsigned slot)
{
    Boat boat = boat_net_client_view(&net->client)->state.slots[slot].boat;
    const BoatPredictionError *o = &net->offset[slot];
    boat.position = Vector2Add(boat.position, o->position);
    boat.heading += o->heading;
    boat.height += o->height;
    boat.pitch += o->pitch;
    boat.roll += o->roll;
    return boat;
}

bool net_study_frame(NetStudy *net, BoatSession *session, BoatInput input, float thrust_target,
    float steering_target, bool active, bool reset)
{
    BoatNetClient *c = &net->client;
    double now = boat_net_clock();
    if (reset && active)
        boat_net_client_request_reset(c);
    if (!boat_net_client_service_targets(c, now, input, !active, thrust_target, steering_target)) {
        fail(net, "Disconnected: %s.", boat_net_client_failure(c));
        return false;
    }
    boat_net_enet_flush(net->enet);
    attach(net, session);
    blend_corrections(net, fminf(GetFrameTime(), .1f));
    const BoatFleet *view = boat_net_client_view(c);
    const BoatFleetSlot *own = &view->state.slots[c->slot];
    net->own_visible = own->active;
    if (own->active) {
        if (own->life != net->own_life) {
            net->own_life = own->life;
            net->respawned_at = now;
            boat_presentation_reset(&session->presentation);
        }
        session->boat = shown(net, c->slot);
    }
    // Every remote's proximity mode follows the physical predicted hulls.
    double server_tick = boat_net_client_server_tick(c, now);
    float dt = fminf(GetFrameTime(), .1f);
    for (unsigned i = 0; i < BOAT_FLEET_SLOTS; ++i) {
        const BoatFleetSlot *slot = &view->state.slots[i];
        net->visible[i] = i != c->slot && slot->active;
        if (!net->visible[i])
            continue;
        float weight = own->active ? boat_remote_proximity_update(&net->proximity[i], &own->boat,
                                         &slot->boat, slot->life, now, dt)
                                   : 0;
        Boat proxy = shown(net, i), past;
        uint32_t life = slot->life;
        BoatRemoteSample sample = boat_remote_history_pose(
            &c->remotes, i, server_tick - BOAT_REMOTE_DELAY_TICKS, &past, &life);
        ++net->samples[sample];
        ++net->frames;
        net->near_frames += weight > 0;
        // A pose of another life than the proxy's is never blended with it.
        if (sample == BOAT_REMOTE_NONE || life != slot->life)
            past = proxy, life = slot->life;
        if (life != net->lives[i]) {
            net->lives[i] = life;
            boat_presentation_reset(&net->presentations[i]);
        }
        net->shown[i] = boat_remote_blend(&past, &proxy, weight);
    }
    uint64_t tick = view->state.tick;
    boat_session_set_tick(session, tick);
    BoatWorld world = session->world;
    for (unsigned steps = 0; net->presented_tick < tick && steps < 12; ++steps) {
        ++net->presented_tick;
        double time = boat_tick_seconds(net->presented_tick);
        if (net->own_visible)
            boat_presentation_step(&session->presentation, &session->boat, &world, time);
        for (unsigned i = 0; i < BOAT_FLEET_SLOTS; ++i)
            if (net->visible[i])
                boat_presentation_step(&net->presentations[i], &net->shown[i], &world, time);
    }
    net->presented_tick = tick;
    return true;
}

unsigned net_study_skin(unsigned slot) { return slot % BOAT_SKIN_COUNT; }

void net_study_rivals(const NetStudy *net, double time, BoatRivals *rivals)
{
    rivals->count = 0;
    for (unsigned i = 0; i < BOAT_FLEET_SLOTS && rivals->count < WATERFRONT_RIVALS; ++i) {
        if (!net->visible[i])
            continue;
        BoatRival *r = &rivals->boats[rivals->count++];
        *r = (BoatRival) { .view = { .boat = net->shown[i], .time = time },
            .presentation = &net->presentations[i] };
        r->view.skin = net_study_skin(i);
    }
}

const char *net_study_notice(const NetStudy *net)
{
    static char text[80];
    double seconds;
    BoatNetResetState reset = boat_net_client_reset_state(&net->client, boat_net_clock(), &seconds);
    if (!net->own_visible)
        return "WAITING FOR A SAFE SPAWN";
    if (reset == BOAT_NET_RESET_NONE)
        return NULL;
    // A clear spot is normally found within a tick or two; say so only when it is not.
    if (reset == BOAT_NET_RESET_SENT || seconds < .5)
        return "RESETTING";
    snprintf(text, sizeof(text), "RESETTING / WAITING FOR A CLEAR SPOT %.0f s", seconds);
    return text;
}

float net_study_fade(const NetStudy *net)
{
    double age = boat_net_clock() - net->respawned_at;
    return net->respawned_at > 0 && age < .35 ? (float)(1 - age / .35) : 0;
}

const char *net_study_status(const NetStudy *net)
{
    static char text[160];
    const BoatNetClient *c = &net->client;
    unsigned players = 0;
    for (unsigned i = 0; i < BOAT_FLEET_SLOTS; ++i)
        players += c->roster.slots[i].state != BOAT_NET_ROSTER_EMPTY;
    snprintf(text, sizeof(text), "NET SLOT %u / %u PLAYERS / RTT %.0f ms / %s%s%s", c->slot + 1,
        players, c->rtt * 1000, phase_text(c->phase),
        boat_net_client_stale(c, boat_net_clock()) ? " / STALE" : "",
        c->wake_consistent ? "" : " / WAKES REPAIRING");
    return text;
}

void net_study_draw_roster(const NetStudy *net, int right, int top)
{
    const BoatNetClient *c = &net->client;
    for (unsigned i = 0; i < BOAT_FLEET_SLOTS; ++i) {
        if (c->roster.slots[i].state == BOAT_NET_ROSTER_EMPTY)
            continue;
        const char *line = TextFormat("%s %s%s", c->roster.slots[i].name,
            TextToUpper(boat_skin_name(net_study_skin(i))), i == c->slot ? " (YOU)" : "");
        int x = right - MeasureText(line, 12);
        DrawRectangle(x - 4, top - 2, right - x + 8, 15, Fade(BLACK, .45f));
        DrawText(line, x, top, 12, i == c->slot ? RAYWHITE : LIGHTGRAY);
        top += 14;
    }
}

void net_study_close(NetStudy *net)
{
    if (!net->enet)
        return;
    const BoatPredictionStats *p = &net->client.prediction.stats;
    if (p->corrections)
        fprintf(stdout,
            "prediction: %llu corrections replaying %.1f ticks (max %u); own error mean %.3f m "
            "max %.3f m; remote mean %.3f m max %.3f m; %llu snaps; %llu held ticks; wakes "
            "vanished %llu (%llu visible), carried %llu, incomplete %llu\n",
            (unsigned long long)p->corrections, (double)p->replayed_ticks / (double)p->corrections,
            p->max_replay, p->total_error / (double)p->corrections, p->max_error,
            p->remote_errors ? p->total_remote_error / (double)p->remote_errors : 0,
            p->max_remote_error, (unsigned long long)p->snaps, (unsigned long long)p->held_ticks,
            (unsigned long long)p->wakes_vanished, (unsigned long long)p->visible_vanished,
            (unsigned long long)p->carried, (unsigned long long)p->incomplete_corrections);
    if (net->frames)
        fprintf(stdout,
            "other boat: %llu frames, %.1f%% interpolated, %.1f%% extrapolated, %.1f%% held, "
            "%.1f%% in proximity mode\n",
            (unsigned long long)net->frames,
            100.0 * (double)net->samples[BOAT_REMOTE_INTERPOLATED] / (double)net->frames,
            100.0 * (double)net->samples[BOAT_REMOTE_EXTRAPOLATED] / (double)net->frames,
            100.0 * (double)net->samples[BOAT_REMOTE_HELD] / (double)net->frames,
            100.0 * (double)net->near_frames / (double)net->frames);
    const BoatNetClientStats *n = &net->client.stats;
    if (n->resets || n->resets_dropped)
        fprintf(stdout, "resets: %llu completed, longest %.0f ms; %llu dropped\n",
            (unsigned long long)n->resets, n->max_reset * 1000,
            (unsigned long long)n->resets_dropped);
    boat_net_client_leave(&net->client);
    boat_net_enet_close(net->enet, 1);
    net->enet = NULL;
    boat_net_enet_cleanup();
}
