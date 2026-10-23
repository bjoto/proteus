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
    // The session draws the mirror's water; its objects pointer borrows the mirror pool.
    session->world.water = boat_fleet_world(&net->client.fleet).water;
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
    BoatNetClientConfig config = { .name = name, .token = token, .build = "boat_study M3" };
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
    net->display_tick = (double)net->client.fleet.state.tick;
    net->presented_tick = net->client.fleet.state.tick;
    attach(net, session);
    boat_presentation_reset(&session->presentation);
    boat_presentation_reset(&net->other_presentation);
    return true;
}

// Continuous pose values between two snapshots of one life; discrete state from the newer.
static Boat blend(const Boat *a, const Boat *b, float t)
{
    Boat boat = *b;
    if (fabsf(b->heading - a->heading) > PI || Vector2Distance(a->position, b->position) > 10)
        return boat; // A teleport or wrap: show the newer pose.
    boat.position = Vector2Lerp(a->position, b->position, t);
    boat.velocity = Vector2Lerp(a->velocity, b->velocity, t);
    const float *from[] = { &a->heading, &a->height, &a->pitch, &a->roll, &a->wheel, &a->throttle,
        &a->engine_response, &a->engine_angle, &a->planing, &a->wave_spray };
    float *to[]
        = { &boat.heading, &boat.height, &boat.pitch, &boat.roll, &boat.wheel, &boat.throttle,
              &boat.engine_response, &boat.engine_angle, &boat.planing, &boat.wave_spray };
    for (size_t i = 0; i < sizeof(to) / sizeof(*to); ++i)
        *to[i] = Lerp(*from[i], *to[i], t);
    return boat;
}

static Boat shown(const NetStudy *net, unsigned slot, float t)
{
    const BoatNetClient *c = &net->client;
    const BoatFleetSlot *now = &c->fleet.state.slots[slot], *before = &c->previous[slot];
    if (!before->active || before->life != now->life)
        return now->boat;
    return blend(&before->boat, &now->boat, t);
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
    // Show authoritative time one snapshot behind the newest, advancing at 120 Hz.
    uint64_t newest = c->fleet.state.tick;
    double earliest = (double)c->previous_tick;
    net->display_tick += fmin(GetFrameTime(), .1) * BOAT_TICK_HZ;
    if (net->display_tick < earliest || net->display_tick > (double)newest)
        net->display_tick = net->display_tick < earliest ? earliest : (double)newest;
    double span = (double)(newest - c->previous_tick);
    float t = span > 0 ? (float)((net->display_tick - earliest) / span) : 1;
    const BoatFleetSlot *own = &c->fleet.state.slots[c->slot];
    net->own_visible = own->active;
    if (own->active) {
        if (own->life != net->own_life) {
            net->own_life = own->life;
            boat_presentation_reset(&session->presentation);
        }
        session->boat = shown(net, c->slot, t);
    }
    float nearest = INFINITY;
    net->other_visible = false;
    for (unsigned i = 0; i < BOAT_FLEET_SLOTS; ++i) {
        const BoatFleetSlot *slot = &c->fleet.state.slots[i];
        float distance = Vector2Distance(slot->boat.position, session->boat.position);
        if (i != c->slot && slot->active && distance < nearest) {
            nearest = distance;
            net->other_slot = i;
            net->other_visible = true;
        }
    }
    if (net->other_visible) {
        const BoatFleetSlot *other = &c->fleet.state.slots[net->other_slot];
        if (other->life != net->other_life) {
            net->other_life = other->life;
            boat_presentation_reset(&net->other_presentation);
        }
        net->other = shown(net, net->other_slot, t);
    }
    uint64_t tick = (uint64_t)net->display_tick;
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
    boat_net_client_leave(&net->client);
    boat_net_enet_close(net->enet, 1);
    net->enet = NULL;
    boat_net_enet_cleanup();
}
