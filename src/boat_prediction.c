#include "boat_prediction.h"

#include "boat_net_server.h"
#include "raymath.h"

#include <math.h>

void boat_prediction_reset(BoatPrediction *p, const BoatFleet *authority,
    const BoatNetSlotInput inputs[BOAT_FLEET_SLOTS], unsigned slot)
{
    *p = (BoatPrediction) {
        .fleet = *authority, .authority_tick = authority->state.tick, .slot = slot
    };
    p->fleet.world.water.objects = &p->fleet.state.wakes;
    for (unsigned i = 0; i < BOAT_FLEET_SLOTS; ++i)
        p->remote[i] = inputs[i].used;
}

void boat_prediction_record(BoatPrediction *p, uint64_t tick, uint32_t life, BoatNetCommand command)
{
    p->own[tick % BOAT_PREDICTION_HISTORY] = (BoatPredictionCommand) { tick, life, command };
}

// The authority's loss policy for the own slot, as far as the client can know it: the command
// sent for the tick, else the last one for up to BOAT_NET_REPEAT_TICKS, else zero rates.
static BoatInput own_input(const BoatPrediction *p, uint64_t tick)
{
    const BoatFleetSlot *slot = &p->fleet.state.slots[p->slot];
    for (uint64_t back = 0; back <= BOAT_NET_REPEAT_TICKS && back < tick; ++back) {
        const BoatPredictionCommand *c = &p->own[(tick - back) % BOAT_PREDICTION_HISTORY];
        if (c->tick != tick - back || c->life != slot->life)
            continue;
        BoatNetCommand command = c->command;
        if (command.flags & BOAT_NET_COMMAND_INACTIVE)
            command = boat_net_neutral_command(&slot->boat, command.flags);
        return boat_net_command_input(command);
    }
    return (BoatInput) { 0 };
}

static void step(BoatPrediction *p)
{
    uint64_t tick = p->fleet.state.tick + 1;
    BoatInput inputs[BOAT_FLEET_SLOTS];
    for (unsigned i = 0; i < BOAT_FLEET_SLOTS; ++i) {
        if (i == p->slot)
            inputs[i] = own_input(p, tick);
        else if (tick - p->authority_tick <= BOAT_PREDICTION_REMOTE_TICKS)
            inputs[i] = boat_net_command_input(p->remote[i]);
        else
            inputs[i] = (BoatInput) { 0 };
    }
    boat_fleet_step(&p->fleet, inputs);
}

static unsigned run_to(BoatPrediction *p, uint64_t tick)
{
    unsigned steps = 0;
    while (p->fleet.state.tick < tick
        && p->fleet.state.tick < p->authority_tick + BOAT_PREDICTION_MAX_AHEAD) {
        step(p);
        ++steps;
    }
    return steps;
}

unsigned boat_prediction_advance(BoatPrediction *p, uint64_t tick)
{
    unsigned steps = run_to(p, tick);
    p->stats.advanced_ticks += steps;
    if (p->fleet.state.tick < tick)
        p->stats.held_ticks += tick - p->fleet.state.tick;
    return steps;
}

static float angle(float a, float b) { return remainderf(a - b, 2 * PI); }

void boat_prediction_correct(
    BoatPrediction *p, const BoatFleet *authority, const BoatNetSlotInput inputs[BOAT_FLEET_SLOTS])
{
    uint64_t target = p->fleet.state.tick;
    BoatFleetSlot before[BOAT_FLEET_SLOTS];
    for (unsigned i = 0; i < BOAT_FLEET_SLOTS; ++i)
        before[i] = p->fleet.state.slots[i];
    boat_fleet_restore(&p->fleet, &authority->state);
    p->authority_tick = authority->state.tick;
    for (unsigned i = 0; i < BOAT_FLEET_SLOTS; ++i)
        p->remote[i] = inputs[i].used;
    unsigned replay = run_to(p, target);
    p->stats.replayed_ticks += replay;
    if (replay > p->stats.max_replay)
        p->stats.max_replay = replay;
    ++p->stats.corrections;
    ++p->correction;
    bool same_tick = p->fleet.state.tick == target;
    for (unsigned i = 0; i < BOAT_FLEET_SLOTS; ++i) {
        const BoatFleetSlot *after = &p->fleet.state.slots[i];
        BoatPredictionError *e = &p->error[i];
        *e = (BoatPredictionError) { .snap = true };
        if (!same_tick || !before[i].active || !after->active || before[i].life != after->life)
            continue;
        const Boat *a = &before[i].boat, *b = &after->boat;
        *e = (BoatPredictionError) { Vector2Subtract(a->position, b->position),
            angle(a->heading, b->heading), a->height - b->height, angle(a->pitch, b->pitch),
            angle(a->roll, b->roll), false };
        float distance = Vector2Length(e->position);
        e->snap = distance > BOAT_PREDICTION_SNAP_DISTANCE
            || fabsf(e->heading) > BOAT_PREDICTION_SNAP_ANGLE
            || fabsf(e->pitch) > BOAT_PREDICTION_SNAP_ANGLE
            || fabsf(e->roll) > BOAT_PREDICTION_SNAP_ANGLE;
        p->stats.snaps += e->snap;
        if (i == p->slot) {
            p->stats.total_error += distance;
            p->stats.max_error = fmaxf(p->stats.max_error, distance);
        } else {
            p->stats.total_remote_error += distance;
            p->stats.max_remote_error = fmaxf(p->stats.max_remote_error, distance);
            ++p->stats.remote_errors;
        }
    }
}
