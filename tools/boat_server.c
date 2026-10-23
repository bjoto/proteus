/* Multiplayer M2 authority process, scripted mode only: one headless basin fleet driven by the
 * boat_fleet_bots drivers for a fixed number of ticks, then a summary and exit. No socket,
 * window, audio, asset or file access; links only proteus_boat_core. The network protocol and
 * the 120 Hz wall-clock scheduler arrive with M3. */
#include "boat_fleet.h"
#include "boat_fleet_bots.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void usage(FILE *out)
{
    fprintf(out,
        "usage: boat_server --scripted [--ticks N] [--boats 1-4] [--location basin]\n"
        "                   [--report-every N]\n"
        "Scripted local authority: bots drive N ticks (default 7200, one minute at 120 Hz)\n"
        "as fast as possible, then print the fleet hash and per-tick cost. No networking yet.\n");
}

static bool parse_count(const char *text, unsigned long low, unsigned long high, unsigned long *out)
{
    char *end;
    unsigned long value = strtoul(text, &end, 10);
    if (!*text || *end || text[0] == '-' || value < low || value > high)
        return false;
    *out = value;
    return true;
}

static double now_seconds(void)
{
    struct timespec t;
    timespec_get(&t, TIME_UTC);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

static int compare_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

int main(int argc, char **argv)
{
    unsigned long ticks = 7200, boats = BOAT_FLEET_SLOTS, report = 0;
    bool scripted = false;
    for (int i = 1; i < argc; ++i) {
        const char *value = i + 1 < argc ? argv[i + 1] : NULL;
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(stdout);
            return 0;
        } else if (strcmp(argv[i], "--scripted") == 0) {
            scripted = true;
        } else if (strcmp(argv[i], "--ticks") == 0 && value
            && parse_count(value, 1, 120ul * 60 * 60 * 24, &ticks)) {
            ++i;
        } else if (strcmp(argv[i], "--boats") == 0 && value
            && parse_count(value, 1, BOAT_FLEET_SLOTS, &boats)) {
            ++i;
        } else if (strcmp(argv[i], "--report-every") == 0 && value
            && parse_count(value, 1, 120ul * 60 * 60 * 24, &report)) {
            ++i;
        } else if (strcmp(argv[i], "--location") == 0 && value) {
            if (strcmp(value, "basin") != 0) {
                fprintf(stderr, "boat_server: only the basin hosts a fleet before M7.\n");
                return 2;
            }
            ++i;
        } else {
            fprintf(stderr, "boat_server: invalid argument: %s\n", argv[i]);
            usage(stderr);
            return 2;
        }
    }
    if (!scripted) {
        fprintf(stderr, "boat_server: networking is not implemented yet; use --scripted.\n");
        usage(stderr);
        return 2;
    }
    static BoatFleet fleet;
    if (!boat_fleet_init(&fleet, boat_location(BOAT_LOCATION_BASIN), (BoatWater) { 0 },
            BOAT_OBJECT_LIFE_TICKS, 1.8f)) {
        fprintf(stderr, "boat_server: cannot create the basin fleet.\n");
        return 1;
    }
    for (unsigned slot = 0; slot < boats; ++slot)
        boat_fleet_request_spawn(&fleet, slot);
    double *cost = malloc(ticks * sizeof(*cost));
    if (!cost) {
        fprintf(stderr, "boat_server: cannot allocate the timing record.\n");
        return 1;
    }
    BoatFleetBots bots = { 0 };
    unsigned long contacts = 0, residual = 0, passes = 0, spawned = 0, blocked = 0, failures = 0;
    unsigned max_passes = 0, max_pair_tests = 0, peak_live = 0, flights = 0;
    for (unsigned long tick = 0; tick < ticks; ++tick) {
        BoatInput inputs[BOAT_FLEET_SLOTS];
        boat_fleet_bots_drive(&bots, &fleet, inputs);
        bool airborne[BOAT_FLEET_SLOTS];
        for (unsigned i = 0; i < BOAT_FLEET_SLOTS; ++i)
            airborne[i] = fleet.state.slots[i].boat.motion_state == BOAT_AIRBORNE;
        double started = now_seconds();
        boat_fleet_step(&fleet, inputs);
        cost[tick] = (now_seconds() - started) * 1000;
        const BoatFleetStats *s = &fleet.stats;
        contacts += s->contacts;
        residual += s->residual;
        passes += s->passes;
        spawned += s->spawned;
        blocked += s->spawn_blocked;
        failures += s->emission_failures;
        max_passes = s->passes > max_passes ? s->passes : max_passes;
        max_pair_tests = s->pair_tests > max_pair_tests ? s->pair_tests : max_pair_tests;
        peak_live = fleet.state.wakes.live > peak_live ? fleet.state.wakes.live : peak_live;
        for (unsigned i = 0; i < BOAT_FLEET_SLOTS; ++i)
            flights += fleet.state.slots[i].active && !airborne[i]
                && fleet.state.slots[i].boat.motion_state == BOAT_AIRBORNE;
        if (report && (tick + 1) % report == 0)
            printf("tick %" PRIu64 " hash %016" PRIx64 " live %u\n", fleet.state.tick,
                boat_fleet_hash(&fleet.state), fleet.state.wakes.live);
    }
    qsort(cost, ticks, sizeof(*cost), compare_double);
    double total = 0;
    for (unsigned long i = 0; i < ticks; ++i)
        total += cost[i];
    printf("boat_server scripted: boats=%lu ticks=%" PRIu64 " hash=%016" PRIx64 "\n", boats,
        fleet.state.tick, boat_fleet_hash(&fleet.state));
    printf("wakes: emitted=%" PRIu64 " live=%u peak_live=%u failures=%lu\n",
        fleet.state.wakes.emitted, fleet.state.wakes.live, peak_live, failures);
    printf("contacts: pairs=%lu passes=%lu max_passes=%u max_pair_tests=%u residual=%lu\n",
        contacts, passes, max_passes, max_pair_tests, residual);
    printf("lives: spawned=%lu blocked_retries=%lu bot_recoveries=%u flights=%u\n", spawned,
        blocked, bots.resets, flights);
    printf("tick_ms: mean=%.4f p50=%.4f p95=%.4f p99=%.4f max=%.4f checkpoint_bytes=%zu\n",
        total / (double)ticks, cost[ticks / 2], cost[ticks * 95 / 100], cost[ticks * 99 / 100],
        cost[ticks - 1], sizeof(BoatFleetState));
    free(cost);
    if (failures) {
        fprintf(stderr, "boat_server: wake emission contract failed.\n");
        return 1;
    }
    return 0;
}
