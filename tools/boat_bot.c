/* Multiplayer M3 scripted client: joins a boat_server room over ENet, drives its server-owned
 * boat with the boat_fleet_bots route (or, with --hunt, at the nearest other boat) from the
 * authoritative mirror, and checks the mirror's wake reconstruction against each snapshot's
 * digest. In deathmatch rooms it prints each finished round's scoreboard as received.
 * Headless; for process smoke tests. */
#include "boat_fleet_bots.h"
#include "boat_net_client.h"
#include "boat_net_enet.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *out)
{
    fprintf(out,
        "usage: boat_bot --connect HOST:PORT [--name TEXT] [--token TEXT] [--seconds S]\n"
        "                [--hunt] [--expect-reject]\n"
        "Join a boat_server room, drive one boat by script for S seconds (default 5), then\n"
        "leave and print a summary. --hunt drives at the nearest other boat to ram it or cross\n"
        "it with its wake (for deathmatch rooms). --expect-reject succeeds only when the room\n"
        "refuses it.\n"
        "Exit status: 0 success, 1 connection failure, 2 usage, 3 rejected.\n");
}

int main(int argc, char **argv)
{
    const char *endpoint = NULL, *name = "bot", *token = "";
    unsigned long seconds = 5;
    bool expect_reject = false, hunt = false;
    for (int i = 1; i < argc; ++i) {
        const char *value = i + 1 < argc ? argv[i + 1] : NULL;
        if (strcmp(argv[i], "--help") == 0) {
            usage(stdout);
            return 0;
        } else if (strcmp(argv[i], "--connect") == 0 && value) {
            endpoint = argv[++i];
        } else if (strcmp(argv[i], "--name") == 0 && value
            && strlen(value) <= BOAT_NET_NAME_BYTES) {
            name = argv[++i];
        } else if (strcmp(argv[i], "--token") == 0 && value
            && strlen(value) <= BOAT_NET_TOKEN_BYTES) {
            token = argv[++i];
        } else if (strcmp(argv[i], "--seconds") == 0 && value) {
            char *end;
            seconds = strtoul(value, &end, 10);
            if (!*value || *end || value[0] == '-' || seconds < 1 || seconds > 86400) {
                usage(stderr);
                return 2;
            }
            ++i;
        } else if (strcmp(argv[i], "--hunt") == 0) {
            hunt = true;
        } else if (strcmp(argv[i], "--expect-reject") == 0) {
            expect_reject = true;
        } else {
            fprintf(stderr, "boat_bot: invalid argument: %s\n", argv[i]);
            usage(stderr);
            return 2;
        }
    }
    char host[256], error[160];
    uint16_t port;
    if (!endpoint || !boat_net_parse_endpoint(endpoint, host, sizeof(host), &port)) {
        fprintf(stderr, "boat_bot: --connect needs HOST:PORT.\n");
        usage(stderr);
        return 2;
    }
    if (!boat_net_enet_startup()) {
        fprintf(stderr, "boat_bot: cannot initialize networking.\n");
        return 1;
    }
    BoatNetEnet *enet = boat_net_enet_connect(host, port, error, sizeof(error));
    if (!enet) {
        fprintf(stderr, "boat_bot: %s\n", error);
        boat_net_enet_cleanup();
        return 1;
    }
    static BoatNetClient client;
    BoatNetClientConfig config = { .name = name, .token = token, .build = "boat_bot D2" };
    boat_net_client_init(&client, boat_net_enet_transport(enet), &config, boat_net_clock());
    BoatFleetBots bots = { 0 };
    double started = 0, end = 0;
    double travelled = 0;
    Vector2 last_position = { 0 };
    uint32_t last_life = 0;
    uint64_t received_inputs = 0, input_samples = 0, last_snapshots = 0;
    unsigned capsized = 0, resets = 0, mtu = 0;
    uint32_t reported_round = 0;
    bool joined = false, running = true;
    while (running) {
        double now = boat_net_clock();
        BoatInput input = { 0 };
        const BoatFleetSlot *own = boat_net_client_own(&client);
        if (own) {
            input = hunt ? boat_fleet_bot_hunt_input(&bots, &client.fleet, client.slot)
                         : boat_fleet_bot_input(&bots, &client.fleet, client.slot);
            // A deathmatch room respawns capsized boats itself.
            if (own->boat.motion_state == BOAT_CAPSIZED
                && client.accept.mode == BOAT_NET_MODE_FREE) {
                if (++capsized == 2 * 60) { // Two seconds of 60 Hz service.
                    boat_net_client_request_reset(&client);
                    bots.waypoint[client.slot] = 0;
                    ++resets;
                }
            } else {
                capsized = 0;
            }
        }
        running = boat_net_client_service(&client, now, input, false);
        boat_net_enet_flush(enet);
        if (!joined && client.phase == BOAT_NET_CLIENT_PLAYING) {
            joined = true;
            started = now;
            end = now + (double)seconds;
            printf("boat_bot joined: slot=%u tick=%" PRIu64 " waves=%d\n", client.slot + 1,
                client.fleet.state.tick, client.accept.waves);
            fflush(stdout);
        }
        if (joined && client.stats.snapshots != last_snapshots) {
            last_snapshots = client.stats.snapshots;
            own = boat_net_client_own(&client);
            if (own) {
                if (own->life == last_life)
                    travelled += Vector2Distance(last_position, own->boat.position);
                last_life = own->life;
                last_position = own->boat.position;
                ++input_samples;
                received_inputs += client.inputs[client.slot].state == BOAT_NET_INPUT_RECEIVED;
            }
        }
        if (client.result.round != reported_round) {
            reported_round = client.result.round;
            printf("boat_bot round %" PRIu32 " end:", reported_round);
            for (unsigned i = 0; i < BOAT_FLEET_SLOTS; ++i) {
                const BoatMatchRow *row = &client.result.slots[i].row;
                if (client.result.slots[i].present)
                    printf(" %u=%" PRId32 "/%" PRIu32 "/%" PRIu32 "/%" PRIu32, i + 1, row->score,
                        row->caused, row->capsized, row->self);
            }
            printf(" (score/caused/capsized/self)\n");
            fflush(stdout);
        }
        unsigned peer_mtu = boat_net_enet_peer_mtu(enet, 0);
        mtu = peer_mtu > mtu ? peer_mtu : mtu;
        if (joined && now >= end)
            break;
        boat_net_enet_wait(enet, 1.0 / 60);
    }
    BoatNetTransportStats net = boat_net_enet_transport(enet).stats(enet);
    BoatNetClientPhase phase = client.phase;
    const char *failure = boat_net_client_failure(&client);
    if (running)
        boat_net_client_leave(&client);
    boat_net_enet_close(enet, 1);
    boat_net_enet_cleanup();
    if (phase == BOAT_NET_CLIENT_REJECTED) {
        printf("boat_bot rejected: %s (server protocol %u, handling %" PRIu32 ")\n", failure,
            (unsigned)client.reject.version, client.reject.physics_revision);
        return expect_reject ? 0 : 3;
    }
    if (expect_reject) {
        fprintf(stderr, "boat_bot: expected a rejection.\n");
        return 1;
    }
    if (!joined || !running) {
        fprintf(stderr, "boat_bot: %s\n", joined ? failure : "could not join the room");
        return 1;
    }
    const BoatNetClientStats *s = &client.stats;
    printf("boat_bot summary: seconds=%.2f snapshots=%" PRIu64 " stale=%" PRIu64
           " consistent=%" PRIu64 " inconsistent=%" PRIu64 " repaired=%" PRIu64
           " max_mismatch_ms=%.1f resyncs=%" PRIu64 " bootstraps=%" PRIu64 " final=%s\n",
        boat_net_clock() - started, s->snapshots, s->stale_snapshots, s->consistent,
        s->inconsistent, s->repaired, s->max_mismatch * 1000, s->resyncs, s->bootstraps,
        client.wake_consistent ? "consistent" : "inconsistent");
    printf("births: applied=%" PRIu64 " duplicate=%" PRIu64 " held=%" PRIu64 " dropped=%" PRIu64
           " live=%u\n",
        s->births_applied, s->births_duplicate, s->births_held, s->births_dropped,
        client.fleet.state.wakes.live);
    printf("control: commands=%" PRIu64 " packets=%" PRIu64 " received_ratio=%.3f lead=%u "
           "rtt_ms=%.2f travelled_m=%.1f resets=%u decode_errors=%" PRIu64 "\n",
        s->commands, s->input_packets,
        input_samples ? (double)received_inputs / (double)input_samples : 0, client.lead,
        client.rtt * 1000, travelled, resets, s->decode_errors);
    if (client.accept.mode == BOAT_NET_MODE_DEATHMATCH)
        printf("match: events=%" PRIu64 " round=%" PRIu32 " phase=%u finished=%" PRIu32 "\n",
            client.feed_count, client.match.round, (unsigned)client.match.phase,
            client.result.round);
    printf("transport: sent=%" PRIu64 " bytes=%" PRIu64 " received=%" PRIu64 " bytes=%" PRIu64
           " dropped=%" PRIu64 " max_message=%zu max_datagram=%zu peer_mtu=%u\n",
        net.sent, net.sent_bytes, net.received, net.received_bytes, net.dropped, net.max_message,
        net.max_datagram, mtu);
    return 0;
}
