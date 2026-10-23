#ifndef PROTEUS_BOAT_PREDICTION_H
#define PROTEUS_BOAT_PREDICTION_H

#include "boat_net_protocol.h"

// Multiplayer M4 client prediction: a speculative copy of the four-boat fleet run ahead of the
// authority with the client's own commands. Every correction restores the newest authoritative
// state and replays the recorded commands to the speculative tick. Remote boats repeat the
// rates the authority last used for them. No device, clock or transport access.
#define BOAT_PREDICTION_HISTORY 240 // Own commands kept (2 s).
#define BOAT_PREDICTION_MAX_AHEAD                                                                  \
    32 // Furthest speculative tick past the authority, so the
       // longest replay; beyond it the prediction holds.
#define BOAT_PREDICTION_REMOTE_TICKS 24 // Remote rates repeat 200 ms past their state, then zero.
#define BOAT_PREDICTION_SNAP_DISTANCE 2.0f // Larger corrections snap instead of blending.
#define BOAT_PREDICTION_SNAP_ANGLE 0.5235988f // 30 degrees.

// Speculative minus corrected pose of one slot at the same tick; the view blends it away.
typedef struct {
    Vector2 position;
    float heading, height, pitch, roll;
    bool snap; // Too large to blend, or a different life: show the corrected pose at once.
} BoatPredictionError;

typedef struct {
    uint64_t corrections, replayed_ticks, advanced_ticks, held_ticks, snaps;
    unsigned max_replay; // Longest replay, ticks.
    float max_error, total_error; // Own slot's position corrections, metres.
    float max_remote_error, total_remote_error; // Other active slots', metres.
    uint64_t remote_errors; // Remote corrections counted in total_remote_error.
} BoatPredictionStats;

typedef struct {
    uint64_t tick;
    uint32_t life;
    BoatNetCommand command;
} BoatPredictionCommand;

typedef struct {
    BoatFleet fleet; // Speculative world; fleet.state.tick is the predicted tick.
    uint64_t authority_tick; // Tick of the state the latest correction replayed from.
    unsigned slot; // The client's own slot.
    BoatPredictionCommand own[BOAT_PREDICTION_HISTORY]; // Indexed by tick modulo the history.
    BoatNetCommand remote[BOAT_FLEET_SLOTS]; // Rates the authority last integrated.
    BoatPredictionError error[BOAT_FLEET_SLOTS]; // Of the latest correction.
    uint32_t correction; // Serial of the latest correction.
    BoatPredictionStats stats;
} BoatPrediction;

// Start from AUTHORITY (copied; nothing is borrowed) with no recorded commands.
void boat_prediction_reset(BoatPrediction *prediction, const BoatFleet *authority,
    const BoatNetSlotInput inputs[BOAT_FLEET_SLOTS], unsigned slot);
// The own command sent for TICK, steering slot life LIFE.
void boat_prediction_record(
    BoatPrediction *prediction, uint64_t tick, uint32_t life, BoatNetCommand command);
// Step the speculative fleet toward TICK, never beyond BOAT_PREDICTION_MAX_AHEAD past the
// authority. Returns the ticks stepped.
unsigned boat_prediction_advance(BoatPrediction *prediction, uint64_t tick);
// Restore AUTHORITY and its INPUTS, then replay to the previous speculative tick (or adopt the
// authority's when it is newer). Records each slot's correction in prediction->error.
void boat_prediction_correct(BoatPrediction *prediction, const BoatFleet *authority,
    const BoatNetSlotInput inputs[BOAT_FLEET_SLOTS]);

#endif
