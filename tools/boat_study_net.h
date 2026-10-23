#ifndef BOAT_STUDY_NET_H
#define BOAT_STUDY_NET_H
#include "boat_net_client.h"
#include "boat_net_enet.h"
#include "boat_session.h"

// boat_study --connect (multiplayer M3 diagnostic): a server-driven view with no prediction.
// The session's boat, clock and water show the authoritative mirror, interpolated between the
// two newest snapshots (about one snapshot behind). The nearest other boat is drawn through
// the ghost path until M4 adds a fleet renderer.
typedef struct {
    BoatNetEnet *enet;
    BoatNetClient client;
    BoatPresentation other_presentation;
    Boat other; // Interpolated pose of the nearest other boat.
    bool own_visible, other_visible;
    unsigned other_slot;
    uint32_t own_life, other_life;
    double display_tick; // Shown authoritative time in ticks, fractional.
    uint64_t presented_tick; // Last tick the presentations were stepped for.
} NetStudy;

// Connect, handshake and install the world (blocking, at most ten seconds). On failure print
// the reason and return false.
bool net_study_connect(
    NetStudy *net, const char *endpoint, const char *name, const char *token, BoatSession *session);
// One displayed frame: service the network with this frame's controls and update the view.
// False when the connection ended (the reason was printed).
bool net_study_frame(NetStudy *net, BoatSession *session, BoatInput input, bool active, bool reset);
// One HUD line: slot, players, round trip and staleness.
const char *net_study_status(const NetStudy *net);
void net_study_close(NetStudy *net);
#endif
