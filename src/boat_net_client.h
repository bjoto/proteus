#ifndef PROTEUS_BOAT_NET_CLIENT_H
#define PROTEUS_BOAT_NET_CLIENT_H

#include "boat_net_transport.h"
#include "boat_prediction.h"
#include "boat_remote_view.h"

#define BOAT_NET_PENDING_BIRTHS 64 // Births held until their tick's snapshot or the baseline.
#define BOAT_NET_STALE 0.25 // Seconds without a snapshot before the view is marked stale.
#define BOAT_NET_RESYNC_AFTER 1.0 // Seconds of wake digest mismatch before a resync request.
#define BOAT_NET_HOLD_INCOMPLETE 0.1 // Seconds a prediction correction waits for missing births.

typedef struct {
    const char *name, *token, *build;
    uint32_t first_sequence; // First outgoing sequence on every channel (wrap tests).
    bool predict; // Run a speculative fleet ahead of the mirror with the own commands.
} BoatNetClientConfig;

typedef enum {
    BOAT_NET_CLIENT_CONNECTING,
    BOAT_NET_CLIENT_HANDSHAKE, // Hello sent.
    BOAT_NET_CLIENT_BOOTSTRAP, // Accepted; assembling the world checkpoint.
    BOAT_NET_CLIENT_PLAYING, // Mirror installed; snapshots and births apply.
    BOAT_NET_CLIENT_REJECTED,
    BOAT_NET_CLIENT_CLOSED,
} BoatNetClientPhase;

typedef struct {
    uint64_t snapshots, stale_snapshots, births_applied, births_duplicate, births_held;
    uint64_t births_dropped, bootstraps, bootstrap_failures, resyncs, decode_errors;
    uint64_t stale_packets, input_packets, commands, skipped_commands, pings, pongs;
    uint64_t consistent, inconsistent; // Snapshots whose wake digest matched on arrival.
    uint64_t repaired; // Mismatches later cleared by births.
    double max_mismatch; // Longest repaired mismatch, seconds.
} BoatNetClientStats;

// The mirror fleet holds the newest authoritative snapshot, rebuilt from a complete checkpoint
// plus births. With prediction, every snapshot or applied birth batch corrects the speculative
// fleet, which each command advances to its target tick; the caller draws that instead.
typedef struct {
    BoatNetTransport transport;
    char name[BOAT_NET_NAME_BYTES + 1], token[BOAT_NET_TOKEN_BYTES + 1];
    char build[BOAT_NET_BUILD_BYTES + 1];
    uint32_t first_sequence;
    bool predict, predict_dirty;
    BoatNetClientPhase phase;
    double started_at;
    BoatNetReject reject; // Valid when REJECTED.
    uint8_t leave; // Server's leave reason when CLOSED, zero for a transport close.
    uint8_t timed_out; // 1: no handshake/world within 10 s, 2: no snapshot for 5 s.
    BoatNetAccept accept;
    uint32_t epoch;
    unsigned slot;
    BoatNetRoster roster;
    // Authoritative mirror: valid once PLAYING.
    BoatFleet fleet;
    BoatNetSlotInput inputs[BOAT_FLEET_SLOTS];
    BoatFleetSlot previous[BOAT_FLEET_SLOTS]; // Slots of the snapshot before the newest.
    uint64_t previous_tick;
    uint64_t wake_digest; // Of the newest snapshot.
    bool wake_consistent;
    double snapshot_at, mismatch_since, resync_at;
    // Bootstrap assembly.
    BoatNetWorldBegin begin;
    bool assembling;
    unsigned pages_received;
    bool page_received[BOAT_NET_MAX_CHECKPOINT / BOAT_NET_PAGE_BYTES];
    uint8_t assembly[BOAT_NET_MAX_CHECKPOINT];
    BoatNetBirth pending[BOAT_NET_PENDING_BIRTHS];
    unsigned pending_count;
    // Sequencing.
    uint32_t sequence[BOAT_NET_CHANNELS];
    uint32_t received[BOAT_NET_CHANNELS];
    bool received_any[BOAT_NET_CHANNELS];
    // Clock estimate and commands.
    double offset; // Estimated server tick minus local seconds times the tick rate.
    bool offset_valid;
    double rtt, ping_at, input_clock;
    unsigned lead; // Ticks ahead of the estimated server tick.
    uint64_t command_tick; // Target tick of the newest command.
    uint32_t command_life;
    unsigned command_count, commands_since_send;
    BoatNetCommand history[BOAT_NET_MAX_COMMANDS]; // Newest last.
    uint32_t reset_request, requests; // Pending reset request ID, and IDs issued.
    BoatNetClientStats stats;
    BoatPrediction prediction; // Valid once PLAYING when predicting.
    BoatRemoteHistory remotes; // Recent authoritative boat states, for drawing other boats.
} BoatNetClient;

void boat_net_client_init(BoatNetClient *client, BoatNetTransport transport,
    const BoatNetClientConfig *config, double now);
// Handle waiting messages and, while playing, generate one command per local 120 Hz tick from
// INPUT (INACTIVE asks the authority to neutralize the controls). False once rejected or closed.
bool boat_net_client_service(BoatNetClient *client, double now, BoatInput input, bool inactive);
// Ask the authority for a new life at a safe spawn (the R key).
void boat_net_client_request_reset(BoatNetClient *client);
// Graceful leave.
void boat_net_client_leave(BoatNetClient *client);
bool boat_net_client_stale(const BoatNetClient *client, double now);
// The client's own slot, or NULL before the baseline or while it has no boat.
const BoatFleetSlot *boat_net_client_own(const BoatNetClient *client);
// Estimated current authoritative tick (fractional), or the mirror's before a clock estimate.
double boat_net_client_server_tick(const BoatNetClient *client, double now);
// The fleet to draw: the speculative one when predicting, else the mirror.
const BoatFleet *boat_net_client_view(const BoatNetClient *client);
// Text for a rejection or close, for the console.
const char *boat_net_client_failure(const BoatNetClient *client);

#endif
