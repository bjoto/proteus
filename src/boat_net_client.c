#include "boat_net_client.h"

#include <math.h>
#include <string.h>

static const BoatWorldDefinition *basin(void) { return boat_location(BOAT_LOCATION_BASIN); }

static void copy_text(char *storage, const char *text, size_t limit)
{
    size_t length = 0;
    while (text && text[length] && length < limit) {
        storage[length] = text[length];
        ++length;
    }
    storage[length] = 0;
}

void boat_net_client_init(BoatNetClient *client, BoatNetTransport transport,
    const BoatNetClientConfig *config, double now)
{
    *client = (BoatNetClient) { .transport = transport,
        .first_sequence = config->first_sequence,
        .predict = config->predict,
        .started_at = now,
        .mismatch_since = -1,
        .resync_at = -1e9,
        .ping_at = -1e9,
        .lead = 8 };
    copy_text(client->name, config->name, BOAT_NET_NAME_BYTES);
    copy_text(client->token, config->token, BOAT_NET_TOKEN_BYTES);
    copy_text(client->build, config->build, BOAT_NET_BUILD_BYTES);
    for (unsigned c = 0; c < BOAT_NET_CHANNELS; ++c)
        client->sequence[c] = config->first_sequence;
}

static void send_message(BoatNetClient *c, BoatNetMessage *m)
{
    BoatNetChannel channel = boat_net_channel(m->header.type);
    m->header.epoch = m->header.type == BOAT_NET_HELLO ? 0 : c->epoch;
    m->header.sequence = c->sequence[channel]++;
    uint8_t bytes[BOAT_NET_MAX_MESSAGE];
    size_t size = boat_net_encode(m, bytes);
    if (size)
        boat_net_send(&c->transport, 0, channel, bytes, size);
}

static uint64_t mirror_tick(const BoatNetClient *c) { return c->fleet.state.tick; }

// Write a birth into the mirror pool unless the slot already holds it or a newer one. Records
// that the mirror's clock has outlived are kept expired, as the authority's pool keeps them.
static void apply_birth(BoatNetClient *c, const BoatNetBirth *b)
{
    BoatWakeObjects *w = &c->fleet.state.wakes;
    BoatWakeObject *o = &w->objects[b->slot];
    if (b->record.born <= o->born) {
        ++c->stats.births_duplicate;
        return;
    }
    *o = b->record;
    if (w->tick - o->born >= w->life_ticks)
        o->amplitude = 0;
    ++c->stats.births_applied;
}

static void hold_birth(BoatNetClient *c, const BoatNetBirth *b)
{
    for (unsigned i = 0; i < c->pending_count; ++i)
        if (c->pending[i].slot == b->slot && c->pending[i].record.born == b->record.born)
            return;
    if (c->pending_count == BOAT_NET_PENDING_BIRTHS) {
        // Bounded: forget the oldest held birth; a later batch or resync repairs it.
        unsigned oldest = 0;
        for (unsigned i = 1; i < c->pending_count; ++i)
            if (c->pending[i].record.born < c->pending[oldest].record.born)
                oldest = i;
        c->pending[oldest] = c->pending[--c->pending_count];
        ++c->stats.births_dropped;
    }
    c->pending[c->pending_count++] = *b;
    ++c->stats.births_held;
}

// Births whose tick the mirror has reached; a record never enters the pool before its birth.
static void release_births(BoatNetClient *c)
{
    for (unsigned i = 0; i < c->pending_count;) {
        if (c->pending[i].record.born <= mirror_tick(c)) {
            apply_birth(c, &c->pending[i]);
            c->pending[i] = c->pending[--c->pending_count];
        } else {
            ++i;
        }
    }
}

// Compare the mirror's live records with the newest snapshot's digest. Births travel at 10 Hz
// and snapshots at 30 Hz, so a snapshot can name a birth whose batch is still on its way;
// only a mismatch that outlasts BOAT_NET_RESYNC_AFTER asks for a fresh checkpoint.
static void check_wakes(BoatNetClient *c, double now)
{
    bool consistent = boat_net_wake_digest(&c->fleet.state.wakes) == c->wake_digest;
    if (consistent && c->mismatch_since >= 0) {
        double waited = now - c->mismatch_since;
        if (waited > c->stats.max_mismatch)
            c->stats.max_mismatch = waited;
        ++c->stats.repaired;
        c->mismatch_since = -1;
    }
    if (!consistent && c->mismatch_since < 0)
        c->mismatch_since = now;
    c->wake_consistent = consistent;
}

static void install(BoatNetClient *c, double now)
{
    static BoatFleetState state;
    BoatNetSlotInput inputs[BOAT_FLEET_SLOTS];
    const BoatNetWorldBegin *b = &c->begin;
    c->assembling = false;
    bool complete = c->pages_received == b->pages
        && boat_net_bytes_digest(c->assembly, b->bytes) == b->digest
        && boat_net_checkpoint_decode(c->assembly, b->bytes, &state, inputs) == BOAT_NET_OK
        && boat_fleet_hash(&state) == b->fleet_hash
        && state.wakes.life_ticks == c->accept.wake_life_ticks
        && state.wakes.crest_height == c->accept.crest_height;
    BoatFleet fleet;
    if (complete)
        complete = boat_fleet_init(&fleet, basin(),
            boat_world_test_water(basin(), c->accept.waves, false, false),
            c->accept.wake_life_ticks, c->accept.crest_height);
    if (!complete) {
        ++c->stats.bootstrap_failures;
        c->resync_at = now;
        BoatNetMessage m = { .header = { .type = BOAT_NET_RESYNC_REQUEST, .tick = state.tick },
            .resync = { .reason = 2, .snapshot_tick = mirror_tick(c) } };
        send_message(c, &m);
        return;
    }
    // A resync replaces the mirror only when the checkpoint is newer than it.
    if (c->phase == BOAT_NET_CLIENT_PLAYING && state.tick < mirror_tick(c))
        return;
    c->fleet = fleet;
    boat_fleet_restore(&c->fleet, &state);
    memcpy(c->inputs, inputs, sizeof(inputs));
    memcpy(c->previous, state.slots, sizeof(c->previous));
    c->previous_tick = state.tick;
    c->wake_digest = boat_net_wake_digest(&state.wakes);
    c->wake_consistent = true;
    c->mismatch_since = -1;
    c->snapshot_at = now;
    ++c->stats.bootstraps;
    release_births(c);
    check_wakes(c, now);
    if (c->predict && c->phase != BOAT_NET_CLIENT_PLAYING)
        boat_prediction_reset(&c->prediction, &c->fleet, c->inputs, c->slot);
    c->predict_dirty = true;
    BoatNetMessage m
        = { .header = { .type = BOAT_NET_READY, .tick = state.tick }, .checkpoint = b->checkpoint };
    send_message(c, &m);
    c->phase = BOAT_NET_CLIENT_PLAYING;
}

static void snapshot(BoatNetClient *c, const BoatNetMessage *m, double now)
{
    const BoatNetSnapshot *s = &m->snapshot;
    uint64_t tick = m->header.tick;
    if (tick <= mirror_tick(c)) {
        ++c->stats.stale_snapshots;
        return;
    }
    BoatFleetState *state = &c->fleet.state;
    BoatWakeObjects *w = &state->wakes;
    memcpy(c->previous, state->slots, sizeof(c->previous));
    c->previous_tick = state->tick;
    for (unsigned i = 0; i < BOAT_OBJECT_CAPACITY; ++i) {
        BoatWakeObject *o = &w->objects[i];
        if (o->amplitude && tick - o->born >= w->life_ticks)
            o->amplitude = 0;
    }
    state->tick = w->tick = tick;
    state->next_request = s->next_request;
    w->live = s->live;
    w->peak_live = s->peak_live;
    w->emitted = s->emitted;
    w->expired = s->expired;
    for (unsigned i = 0; i < BOAT_FLEET_SLOTS; ++i) {
        state->slots[i] = s->slots[i];
        w->emitters[i] = s->emitters[i];
        c->inputs[i] = s->inputs[i];
    }
    c->wake_digest = s->wake_digest;
    release_births(c);
    check_wakes(c, now);
    ++*(c->wake_consistent ? &c->stats.consistent : &c->stats.inconsistent);
    c->snapshot_at = now;
    c->predict_dirty = true;
    ++c->stats.snapshots;
    // The authority sends snapshot n right after tick n: allow half the round trip.
    double sample = (double)tick + c->rtt * .5 * BOAT_TICK_HZ - now * BOAT_TICK_HZ;
    if (!c->offset_valid || fabs(sample - c->offset) > 12) {
        c->offset = sample;
        c->offset_valid = true;
    } else {
        c->offset += (sample - c->offset) * .05;
    }
}

static void births(BoatNetClient *c, const BoatNetWakeBirths *b, double now)
{
    for (unsigned i = 0; i < b->count; ++i) {
        if (c->phase == BOAT_NET_CLIENT_PLAYING && b->births[i].record.born <= mirror_tick(c)) {
            uint64_t applied = c->stats.births_applied;
            apply_birth(c, &b->births[i]);
            c->predict_dirty |= c->stats.births_applied != applied;
        } else
            hold_birth(c, &b->births[i]);
    }
    if (c->phase == BOAT_NET_CLIENT_PLAYING)
        check_wakes(c, now);
}

static void page(BoatNetClient *c, const BoatNetWorldPage *p)
{
    const BoatNetWorldBegin *b = &c->begin;
    if (!c->assembling || p->checkpoint != b->checkpoint || p->count != b->pages
        || c->page_received[p->index])
        return;
    size_t offset = (size_t)p->index * BOAT_NET_PAGE_BYTES;
    size_t expected
        = b->bytes - offset < BOAT_NET_PAGE_BYTES ? b->bytes - offset : BOAT_NET_PAGE_BYTES;
    if (p->size != expected) {
        c->assembling = false;
        ++c->stats.bootstrap_failures;
        return;
    }
    memcpy(c->assembly + offset, p->data, p->size);
    c->page_received[p->index] = true;
    ++c->pages_received;
}

static void message(BoatNetClient *c, const BoatNetEvent *event, double now)
{
    static BoatNetMessage m;
    if (boat_net_decode(event->data, event->size, &m) != BOAT_NET_OK) {
        ++c->stats.decode_errors;
        return;
    }
    BoatNetChannel channel = boat_net_channel(m.header.type);
    if (channel != event->channel) {
        ++c->stats.decode_errors;
        return;
    }
    if (channel == BOAT_NET_STATE || channel == BOAT_NET_BIRTHS) {
        if (c->received_any[channel]
            && !boat_net_sequence_newer(m.header.sequence, c->received[channel])) {
            ++c->stats.stale_packets;
            return;
        }
        c->received_any[channel] = true;
        c->received[channel] = m.header.sequence;
    }
    if (m.header.type == BOAT_NET_REJECT && c->phase == BOAT_NET_CLIENT_HANDSHAKE) {
        c->reject = m.reject;
        c->phase = BOAT_NET_CLIENT_REJECTED;
        return;
    }
    if (m.header.type == BOAT_NET_ACCEPT && c->phase == BOAT_NET_CLIENT_HANDSHAKE) {
        c->accept = m.accept;
        c->epoch = m.header.epoch;
        c->slot = m.accept.slot;
        c->phase = BOAT_NET_CLIENT_BOOTSTRAP;
        return;
    }
    if (c->phase < BOAT_NET_CLIENT_BOOTSTRAP || m.header.epoch != c->epoch) {
        ++c->stats.stale_packets;
        return;
    }
    switch (m.header.type) {
    case BOAT_NET_ROSTER:
        c->roster = m.roster;
        break;
    case BOAT_NET_WORLD_BEGIN:
        c->begin = m.world_begin;
        c->assembling = true;
        c->pages_received = 0;
        memset(c->page_received, 0, sizeof(c->page_received));
        break;
    case BOAT_NET_WORLD_PAGE:
        page(c, &m.world_page);
        break;
    case BOAT_NET_WORLD_END:
        if (c->assembling && m.checkpoint == c->begin.checkpoint)
            install(c, now);
        break;
    case BOAT_NET_SNAPSHOT:
        if (c->phase == BOAT_NET_CLIENT_PLAYING)
            snapshot(c, &m, now);
        break;
    case BOAT_NET_WAKE_BIRTHS:
        births(c, &m.births, now);
        break;
    case BOAT_NET_PONG: {
        double sent = (double)m.clock * 1e-6;
        if (sent <= now && now - sent < 10) {
            // Falls at once, rises slowly: a pong read late after a local stall (a window
            // opening, a slow frame) must not throw the clock estimate and lead far ahead.
            double rtt = now - sent;
            c->rtt = !c->stats.pongs || rtt < c->rtt ? rtt : c->rtt + (rtt - c->rtt) * .1;
            ++c->stats.pongs;
            unsigned lead = (unsigned)ceil(c->rtt * .5 * BOAT_TICK_HZ) + 4;
            c->lead = lead < 4 ? 4 : lead > 16 ? 16 : lead;
        }
        break;
    }
    case BOAT_NET_DISCONNECT:
        c->leave = m.leave;
        c->phase = BOAT_NET_CLIENT_CLOSED;
        break;
    default:
        ++c->stats.decode_errors; // The authority never sends the other types.
    }
}

const BoatFleetSlot *boat_net_client_own(const BoatNetClient *c)
{
    if (c->phase != BOAT_NET_CLIENT_PLAYING || !c->fleet.state.slots[c->slot].active)
        return NULL;
    return &c->fleet.state.slots[c->slot];
}

const BoatFleet *boat_net_client_view(const BoatNetClient *c)
{
    return c->predict ? &c->prediction.fleet : &c->fleet;
}

// One command per local tick, aimed at the estimated server tick plus the lead.
static void command(BoatNetClient *c, double now, BoatNetCommand next)
{
    const BoatFleetSlot *own = boat_net_client_own(c);
    if (!own || !c->offset_valid)
        return;
    if (own->life != c->command_life) {
        c->command_life = own->life;
        c->command_count = 0;
        c->command_tick = 0;
    }
    uint64_t target = (uint64_t)fmax(0, floor(now * BOAT_TICK_HZ + c->offset)) + c->lead;
    uint64_t tick = c->command_tick + 1;
    if (c->command_tick == 0 || target > tick + 2) {
        tick = target; // Start, or jump forward after the estimate moved.
    } else if (target + 3 < tick) {
        ++c->stats.skipped_commands; // Running ahead: let the authority catch up.
        return;
    }
    if (tick <= mirror_tick(c))
        tick = mirror_tick(c) + 1; // Finalized ticks take no commands.
    if (tick != c->command_tick + 1)
        c->command_count = 0; // A batch covers consecutive ticks only.
    if (c->command_count == BOAT_NET_MAX_COMMANDS)
        memmove(c->history, c->history + 1, sizeof(*c->history) * (BOAT_NET_MAX_COMMANDS - 1));
    else
        ++c->command_count;
    c->history[c->command_count - 1] = next;
    c->command_tick = tick;
    ++c->stats.commands;
    if (c->predict) {
        boat_prediction_record(&c->prediction, tick, own->life, next);
        boat_prediction_advance(&c->prediction, tick);
    }
    if (++c->commands_since_send < (c->accept.input_ticks ? c->accept.input_ticks : 2))
        return;
    c->commands_since_send = 0;
    BoatNetMessage m = { .header = { .type = BOAT_NET_INPUT_BATCH, .tick = tick },
        .input = { .life = own->life,
            .count = (uint8_t)c->command_count,
            .snapshot_tick = mirror_tick(c) } };
    memcpy(m.input.commands, c->history, sizeof(*c->history) * c->command_count);
    send_message(c, &m);
    ++c->stats.input_packets;
}

bool boat_net_client_service(BoatNetClient *c, double now, BoatInput input, bool inactive)
{
    BoatNetEvent event;
    for (unsigned handled = 0;
        handled < BOAT_NET_MESSAGES_PER_SERVICE && boat_net_poll(&c->transport, &event);
        ++handled) {
        if (event.kind == BOAT_NET_EVENT_CONNECT && c->phase == BOAT_NET_CLIENT_CONNECTING) {
            BoatNetMessage m = { .header = { .type = BOAT_NET_HELLO },
                .hello = { .physics_revision = BOAT_PHYSICS_REVISION,
                    .content_digest = boat_net_content_digest(basin()) } };
            copy_text(m.hello.name, c->name, BOAT_NET_NAME_BYTES);
            copy_text(m.hello.token, c->token, BOAT_NET_TOKEN_BYTES);
            copy_text(m.hello.build, c->build, BOAT_NET_BUILD_BYTES);
            send_message(c, &m);
            c->phase = BOAT_NET_CLIENT_HANDSHAKE;
        } else if (event.kind == BOAT_NET_EVENT_DISCONNECT) {
            if (c->phase != BOAT_NET_CLIENT_REJECTED)
                c->phase = BOAT_NET_CLIENT_CLOSED;
        } else if (event.kind == BOAT_NET_EVENT_MESSAGE) {
            message(c, &event, now);
        }
        if (c->phase >= BOAT_NET_CLIENT_REJECTED)
            return false;
    }
    if (c->phase >= BOAT_NET_CLIENT_REJECTED)
        return false;
    bool joining = c->phase < BOAT_NET_CLIENT_PLAYING;
    if (joining ? now - c->started_at > BOAT_NET_HANDSHAKE_TIMEOUT
                : now - c->snapshot_at > BOAT_NET_TIMEOUT) {
        c->timed_out = joining ? 1 : 2;
        c->phase = BOAT_NET_CLIENT_CLOSED;
        c->transport.disconnect(c->transport.context, 0);
        return false;
    }
    if (c->phase >= BOAT_NET_CLIENT_BOOTSTRAP && now - c->ping_at >= .5) {
        c->ping_at = now;
        BoatNetMessage m = { .header = { .type = BOAT_NET_PING, .tick = mirror_tick(c) },
            .clock = (uint64_t)(now * 1e6) };
        send_message(c, &m);
        ++c->stats.pings;
    }
    if (c->phase != BOAT_NET_CLIENT_PLAYING)
        return true;
    if (!c->wake_consistent && c->mismatch_since >= 0
        && now - c->mismatch_since > BOAT_NET_RESYNC_AFTER && now - c->resync_at > 1) {
        c->resync_at = now;
        ++c->stats.resyncs;
        BoatNetMessage m = { .header = { .type = BOAT_NET_RESYNC_REQUEST, .tick = mirror_tick(c) },
            .resync = { .reason = 1, .snapshot_tick = mirror_tick(c) } };
        send_message(c, &m);
    }
    if (c->predict && c->predict_dirty) {
        // Births travel at 10 Hz: hold a correction up to BOAT_NET_HOLD_INCOMPLETE for births
        // the snapshot counts rather than replay a wake set known to be incomplete.
        bool hold = c->mismatch_since >= 0 && now - c->mismatch_since < BOAT_NET_HOLD_INCOMPLETE;
        BoatPredictionWakes wakes = { c->wake_digest, c->pending, c->pending_count, hold };
        c->predict_dirty = !boat_prediction_correct(&c->prediction, &c->fleet, c->inputs, &wakes);
    }
    if (c->input_clock == 0 || now - c->input_clock > .25)
        c->input_clock = now; // Never replay a backlog of held keys after a stall.
    BoatNetCommand next = boat_net_command(input, inactive ? BOAT_NET_COMMAND_INACTIVE : 0);
    while (c->input_clock + BOAT_TICK_SECONDS <= now) {
        c->input_clock += BOAT_TICK_SECONDS;
        command(c, now, next);
    }
    if (c->reset_request) {
        const BoatFleetSlot *own = &c->fleet.state.slots[c->slot];
        BoatNetMessage m = { .header = { .type = BOAT_NET_SPAWN_REQUEST, .tick = mirror_tick(c) },
            .spawn = { .request = c->reset_request, .life = own->life } };
        send_message(c, &m);
        c->reset_request = 0;
    }
    return true;
}

void boat_net_client_request_reset(BoatNetClient *c)
{
    c->reset_request = ++c->requests ? c->requests : ++c->requests;
}

void boat_net_client_leave(BoatNetClient *c)
{
    if (c->phase >= BOAT_NET_CLIENT_HANDSHAKE && c->phase <= BOAT_NET_CLIENT_PLAYING) {
        BoatNetMessage m = { .header = { .type = BOAT_NET_DISCONNECT, .tick = mirror_tick(c) },
            .leave = BOAT_NET_LEAVE_QUIT };
        send_message(c, &m);
    }
    c->transport.disconnect(c->transport.context, 0);
    if (c->phase != BOAT_NET_CLIENT_REJECTED)
        c->phase = BOAT_NET_CLIENT_CLOSED;
}

bool boat_net_client_stale(const BoatNetClient *c, double now)
{
    return c->phase != BOAT_NET_CLIENT_PLAYING || now - c->snapshot_at > BOAT_NET_STALE;
}

const char *boat_net_client_failure(const BoatNetClient *c)
{
    if (c->phase == BOAT_NET_CLIENT_REJECTED)
        return boat_net_reject_text(c->reject.reason);
    if (c->timed_out == 1)
        return "no answer within 10 s (is boat_server running there, and is its UDP port "
               "reachable?)";
    if (c->timed_out == 2)
        return "no snapshot for 5 s";
    switch (c->leave) {
    case BOAT_NET_LEAVE_TIMEOUT:
        return "the server timed this client out";
    case BOAT_NET_LEAVE_SHUTDOWN:
        return "the server closed the room";
    case BOAT_NET_LEAVE_MISBEHAVING:
        return "the server dropped this client for invalid messages";
    case BOAT_NET_LEAVE_QUIT:
        return "left the room";
    default:
        return c->phase == BOAT_NET_CLIENT_CLOSED ? "connection closed or timed out" : "";
    }
}
