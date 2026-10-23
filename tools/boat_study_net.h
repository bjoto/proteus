#ifndef BOAT_STUDY_NET_H
#define BOAT_STUDY_NET_H
#include "boat_net_client.h"
#include "boat_net_enet.h"
#include "boat_session.h"

// boat_study --connect (multiplayer M4 in progress): the session's boat, clock and water show
// the client's predicted fleet, so the own boat answers the keys at once. Corrections move the
// physical state immediately; the drawn poses blend small ones away over about 120 ms. The
// nearest other boat is drawn from the same predicted fleet through the ghost path until the
// remote interpolation and fleet renderer steps.
typedef struct {
    BoatNetEnet *enet;
    BoatNetClient client;
    BoatPresentation other_presentation;
    Boat other; // Drawn pose of the nearest other boat.
    bool own_visible, other_visible;
    unsigned other_slot;
    uint32_t own_life, other_life;
    BoatPredictionError offset[BOAT_FLEET_SLOTS]; // Visual correction still being blended.
    uint32_t correction; // Prediction correction serial already folded into the offsets.
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
