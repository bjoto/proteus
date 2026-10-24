#include "boat_study_net.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>

static const char *phase_text(BoatNetClientPhase phase)
{
    static const char *names[]
        = { "connecting", "handshake", "loading world", "playing", "rejected", "closed" };
    return names[phase];
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
        fprintf(stderr,
            "--connect needs HOST:PORT with an IPv4 address or name and a port "
            "1-65535, for example 127.0.0.1:%d\n",
            BOAT_NET_PORT);
        return false;
    }
    if (!boat_net_enet_startup()) {
        fprintf(stderr, "Cannot initialize networking.\n");
        return false;
    }
    net->enet = boat_net_enet_connect(host, port, error, sizeof(error));
    if (!net->enet) {
        fprintf(stderr, "Cannot connect: %s\n", error);
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
            fprintf(stderr, "Cannot join %s:%u: %s (while %s).\n", host, (unsigned)port,
                boat_net_client_failure(&net->client),
                net->client.phase == BOAT_NET_CLIENT_REJECTED ? "in the handshake" : "connecting");
            if (net->client.phase == BOAT_NET_CLIENT_REJECTED)
                fprintf(stderr, "Server: protocol %u, handling revision %u; this build: %d, %d.\n",
                    (unsigned)net->client.reject.version,
                    (unsigned)net->client.reject.physics_revision, BOAT_NET_VERSION,
                    BOAT_PHYSICS_REVISION);
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
    boat_presentation_reset(&net->other_presentation);
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

bool net_study_frame(NetStudy *net, BoatSession *session, BoatInput input, bool active, bool reset)
{
    BoatNetClient *c = &net->client;
    double now = boat_net_clock();
    if (reset && active)
        boat_net_client_request_reset(c);
    if (!boat_net_client_service(c, now, input, !active)) {
        fprintf(stderr, "Disconnected: %s.\n", boat_net_client_failure(c));
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
            boat_presentation_reset(&session->presentation);
        }
        session->boat = shown(net, c->slot);
    }
    float nearest = INFINITY;
    net->other_visible = false;
    for (unsigned i = 0; i < BOAT_FLEET_SLOTS; ++i) {
        const BoatFleetSlot *slot = &view->state.slots[i];
        float distance = Vector2Distance(slot->boat.position, session->boat.position);
        if (i != c->slot && slot->active && distance < nearest) {
            nearest = distance;
            net->other_slot = i;
            net->other_visible = true;
        }
    }
    if (net->other_visible) {
        const BoatFleetSlot *other = &view->state.slots[net->other_slot];
        if (other->life != net->other_life) {
            net->other_life = other->life;
            boat_presentation_reset(&net->other_presentation);
        }
        net->other = shown(net, net->other_slot);
    }
    uint64_t tick = view->state.tick;
    boat_session_set_tick(session, tick);
    BoatWorld world = session->world;
    for (unsigned steps = 0; net->presented_tick < tick && steps < 12; ++steps) {
        ++net->presented_tick;
        double time = boat_tick_seconds(net->presented_tick);
        if (net->own_visible)
            boat_presentation_step(&session->presentation, &session->boat, &world, time);
        if (net->other_visible)
            boat_presentation_step(&net->other_presentation, &net->other, &world, time);
    }
    net->presented_tick = tick;
    return true;
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
    boat_net_client_leave(&net->client);
    boat_net_enet_close(net->enet, 1);
    net->enet = NULL;
    boat_net_enet_cleanup();
}
