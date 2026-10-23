/* Multiplayer M3: protocol codec, authority/client sessions and the in-memory impairment
 * transport. Links only proteus_boat_net (and through it the core); no sockets or devices.
 * Golden bytes pin the wire layout; every truncation and the invalid-field cases must be
 * rejected; checkpoints decode to bit-identical fleets; and seeded virtual networks check
 * handshake rejection, bounded queues, input policy, idempotence, sequence wrap, impairment,
 * timeouts and that a client mirror reproduces the authority's state hash. */
#include "boat_fleet_bots.h"
#include "boat_net_client.h"
#include "boat_net_server.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test);                             \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

#define RUN(test)                                                                                  \
    do {                                                                                           \
        if (test)                                                                                  \
            return 1;                                                                              \
    } while (0)

static const BoatWorldDefinition *basin(void) { return boat_location(BOAT_LOCATION_BASIN); }

static size_t encode(BoatNetMessage m, uint8_t out[BOAT_NET_MAX_MESSAGE])
{
    return boat_net_encode(&m, out);
}

static bool same_bytes(const uint8_t *a, size_t size, const char *hex)
{
    if (strlen(hex) != size * 2)
        return false;
    for (size_t i = 0; i < size; ++i) {
        unsigned value;
        if (sscanf(hex + 2 * i, "%2x", &value) != 1 || value != a[i])
            return false;
    }
    return true;
}

static void put32(uint8_t *at, uint32_t value)
{
    for (int i = 0; i < 4; ++i)
        at[i] = (uint8_t)(value >> (24 - 8 * i));
}

static void putf(uint8_t *at, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    put32(at, bits);
}

// A fleet with every slot used, wakes and a pending reset, after TICKS bot ticks.
static void busy_fleet(BoatFleet *fleet, unsigned ticks, bool waves)
{
    boat_fleet_init(fleet, basin(), boat_world_test_water(basin(), waves, false, false),
        BOAT_OBJECT_LIFE_TICKS, 1.8f);
    for (unsigned i = 0; i < BOAT_FLEET_SLOTS; ++i)
        boat_fleet_request_spawn(fleet, i);
    BoatFleetBots bots = { 0 };
    for (unsigned t = 0; t < ticks; ++t) {
        BoatInput inputs[BOAT_FLEET_SLOTS];
        boat_fleet_bots_drive(&bots, fleet, inputs);
        boat_fleet_step(fleet, inputs);
    }
}

static int golden(void)
{
    uint8_t bytes[BOAT_NET_MAX_MESSAGE];
    size_t size = encode((BoatNetMessage) { .header = { .type = BOAT_NET_PING,
                                                .epoch = 0x01020304u,
                                                .sequence = 0xa0b0c0d0u,
                                                .tick = UINT64_C(0x1122334455667788) },
                             .clock = UINT64_C(0x0102030405060708) },
        bytes);
    CHECK(size == 40);
    CHECK(same_bytes(bytes, size,
        "50424e5400010d0001020304a0b0c0d01122334455667788000800000000000001020304050607"
        "08"));
    BoatNetMessage input = { .header = { .type = BOAT_NET_INPUT_BATCH, .epoch = 9, .tick = 200 },
        .input = { .life = 7, .count = 2, .snapshot_tick = 100 } };
    input.input.commands[0] = boat_net_command((BoatInput) { 1, -1 }, 0);
    input.input.commands[1] = boat_net_command((BoatInput) { 0, 0 }, BOAT_NET_COMMAND_INACTIVE);
    size = encode(input, bytes);
    CHECK(size == 56);
    CHECK(same_bytes(bytes, size,
        "50424e54000109000000000900000000000000000000"
        "00c80018000000000000"
        "00000007020000000000000000000064" // Life, count, latest snapshot tick.
        "7f810000" // Steering +1, throttle -1.
        "00000100")); // Zero rates, inactive.
    BoatNetMessage hello = { .header = { .type = BOAT_NET_HELLO },
        .hello = { .physics_revision = 6,
            .content_digest = UINT64_C(0xdeadbeefcafef00d),
            .name = "ab",
            .token = "",
            .build = "x" } };
    size = encode(hello, bytes);
    CHECK(size == 32 + 4 + 8 + 3 + 1 + 2);
    CHECK(same_bytes(bytes + 32, size - 32,
        "00000006deadbeefcafef00d"
        "026162"
        "00"
        "0178"));
    // Snapshot layout: sizes and a pinned digest of a hand-built state's bytes.
    BoatFleetState state = { .tick = 1000, .next_request = 5 };
    BoatNetSlotInput records[BOAT_FLEET_SLOTS]
        = { { .tick = 999, .state = 1, .used = { 127, -64, 1 } } };
    state.slots[0] = (BoatFleetSlot) { .requested = 3, .life = 2, .request = 1, .active = true };
    state.slots[0].boat.position = (Vector2) { 12.5f, -3.25f };
    state.slots[0].boat.heading = 1.5f;
    state.slots[0].boat.motion_state = BOAT_AIRBORNE;
    state.slots[0].boat.landings = 4;
    state.wakes = (BoatWakeObjects) { .tick = 1000,
        .life_ticks = 1920,
        .crest_height = 1.8f,
        .emitted = 9,
        .expired = 8,
        .live = 1,
        .peak_live = 3 };
    state.wakes.objects[33] = (BoatWakeObject) { { 1, 2 }, { 0, 1 }, 2, .5f, .25f, 990 };
    state.wakes.emitters[1] = (BoatWakeObjectEmitter) { { 4, 5 }, 900, 1000, 2, .5f, 3, true };
    BoatNetMessage snapshot = { .header = { .type = BOAT_NET_SNAPSHOT, .epoch = 1, .tick = 1000 } };
    boat_net_snapshot_of(&state, records, &snapshot.snapshot);
    size = encode(snapshot, bytes);
    CHECK(size == 752);
    uint8_t expected[4];
    putf(expected, 12.5f);
    CHECK(memcmp(bytes + 32 + 32 + 36, expected, 4) == 0); // Slot 0 position.x.
    CHECK(bytes[32 + 32 + 1] == BOAT_AIRBORNE && bytes[32 + 32 + 28] == 0x7f);
    CHECK(bytes[32 + 32 + 29] == 0xc0); // -64 as a two's-complement byte.
    uint64_t digest = boat_net_bytes_digest(bytes, size);
    printf("golden: snapshot digest %016" PRIx64 "\n", digest);
    CHECK(digest == UINT64_C(0x57447ded5b742cfe));
    // Command quantization: keyboard values survive exactly.
    for (float value = -1; value <= 1; value += 1) {
        BoatInput back = boat_net_command_input(boat_net_command((BoatInput) { value, value }, 0));
        CHECK(back.steering_rate == value && back.throttle_rate == value);
    }
    CHECK(boat_net_command((BoatInput) { NAN, 2 }, 0).steering == 0);
    CHECK(boat_net_command((BoatInput) { NAN, 2 }, 0).throttle == 127);
    CHECK(boat_net_sequence_newer(0, UINT32_MAX) && !boat_net_sequence_newer(UINT32_MAX, 0));
    CHECK(!boat_net_sequence_newer(5, 5) && !boat_net_sequence_newer(UINT32_C(0x80000000), 0));
    CHECK(boat_net_sequence_newer(UINT32_C(0x7fffffff), 0));
    printf("golden: header, input, hello and snapshot layouts pinned\n");
    return 0;
}

typedef struct {
    uint8_t bytes[BOAT_NET_MAX_MESSAGE];
    size_t size;
} Sample;

static unsigned samples(Sample *out)
{
    static BoatFleet fleet;
    busy_fleet(&fleet, 1500, false);
    BoatNetSlotInput records[BOAT_FLEET_SLOTS] = { 0 };
    unsigned n = 0;
    BoatNetMessage m[BOAT_NET_TYPE_LIMIT] = { 0 };
    m[0] = (BoatNetMessage) { .header = { .type = BOAT_NET_HELLO },
        .hello = { BOAT_PHYSICS_REVISION, 42, "name", "token", "build" } };
    m[1] = (BoatNetMessage) { .header = { .type = BOAT_NET_ACCEPT, .epoch = 3 },
        .accept = { 2, true, 4, 12, 2, 12, 24, 120, 1920, 1.8f } };
    m[2] = (BoatNetMessage) { .header = { .type = BOAT_NET_REJECT },
        .reject = { BOAT_NET_REJECT_TOKEN, 1, 6, 42 } };
    m[3] = (BoatNetMessage) { .header = { .type = BOAT_NET_ROSTER, .epoch = 3 } };
    m[3].roster.slots[1].state = BOAT_NET_ROSTER_PLAYING;
    strcpy(m[3].roster.slots[1].name, "sixteen-chars-ok");
    m[4] = (BoatNetMessage) { .header = { .type = BOAT_NET_WORLD_BEGIN, .epoch = 3, .tick = 9 },
        .world_begin = { 1, 5332, 6, 77, 88 } };
    m[5] = (BoatNetMessage) { .header = { .type = BOAT_NET_WORLD_PAGE, .epoch = 3 },
        .world_page = { .checkpoint = 1, .index = 5, .count = 6, .size = 212 } };
    m[6] = (BoatNetMessage) { .header = { .type = BOAT_NET_WORLD_END, .epoch = 3 },
        .checkpoint = 1 };
    m[7] = (BoatNetMessage) { .header = { .type = BOAT_NET_READY, .epoch = 3 }, .checkpoint = 1 };
    m[8] = (BoatNetMessage) { .header = { .type = BOAT_NET_INPUT_BATCH, .epoch = 3, .tick = 50 },
        .input = { .life = 1, .count = BOAT_NET_MAX_COMMANDS, .snapshot_tick = 40 } };
    m[9] = (BoatNetMessage) { .header
        = { .type = BOAT_NET_SNAPSHOT, .epoch = 3, .tick = fleet.state.tick } };
    boat_net_snapshot_of(&fleet.state, records, &m[9].snapshot);
    m[10] = (BoatNetMessage) { .header
        = { .type = BOAT_NET_WAKE_BIRTHS, .epoch = 3, .tick = fleet.state.tick } };
    for (unsigned i = 0; i < BOAT_OBJECT_CAPACITY && m[10].births.count < BOAT_NET_MAX_BIRTHS; ++i)
        if (fleet.state.wakes.objects[i].amplitude)
            m[10].births.births[m[10].births.count++]
                = (BoatNetBirth) { (uint8_t)i, fleet.state.wakes.objects[i] };
    m[11] = (BoatNetMessage) { .header = { .type = BOAT_NET_SPAWN_REQUEST, .epoch = 3 },
        .spawn = { 4, 2 } };
    m[12] = (BoatNetMessage) { .header = { .type = BOAT_NET_PING, .epoch = 3 }, .clock = 5 };
    m[13] = (BoatNetMessage) { .header = { .type = BOAT_NET_PONG, .epoch = 3 }, .clock = 5 };
    m[14] = (BoatNetMessage) { .header = { .type = BOAT_NET_RESYNC_REQUEST, .epoch = 3 },
        .resync = { 1, 30 } };
    m[15] = (BoatNetMessage) { .header = { .type = BOAT_NET_DISCONNECT, .epoch = 3 },
        .leave = BOAT_NET_LEAVE_QUIT };
    for (unsigned i = 0; i < BOAT_NET_TYPE_LIMIT - 1; ++i) {
        out[n].size = boat_net_encode(&m[i], out[n].bytes);
        if (!out[n].size) {
            fprintf(stderr, "sample type %u does not encode\n", (unsigned)m[i].header.type);
            return 0;
        }
        ++n;
    }
    return n;
}

static int codec(void)
{
    static Sample sample[BOAT_NET_TYPE_LIMIT];
    unsigned n = samples(sample);
    CHECK(n == BOAT_NET_TYPE_LIMIT - 1);
    static BoatNetMessage m;
    unsigned truncations = 0;
    for (unsigned i = 0; i < n; ++i) {
        const Sample *s = &sample[i];
        CHECK(s->size <= BOAT_NET_MAX_MESSAGE);
        CHECK(boat_net_decode(s->bytes, s->size, &m) == BOAT_NET_OK);
        uint8_t again[BOAT_NET_MAX_MESSAGE];
        CHECK(boat_net_encode(&m, again) == s->size && memcmp(again, s->bytes, s->size) == 0);
        for (size_t length = 0; length < s->size; ++length, ++truncations)
            CHECK(boat_net_decode(s->bytes, length, &m) != BOAT_NET_OK);
        uint8_t longer[BOAT_NET_MAX_MESSAGE + 1];
        memcpy(longer, s->bytes, s->size);
        longer[s->size] = 0;
        CHECK(boat_net_decode(longer, s->size + 1, &m) == BOAT_NET_BAD_LENGTH);
        // A payload byte added and declared: still rejected by the type's exact layout,
        // except pages, whose data length is variable.
        if (s->bytes[6] != BOAT_NET_WORLD_PAGE && s->size < BOAT_NET_MAX_MESSAGE) {
            longer[25] = (uint8_t)(longer[25] + 1);
            if (longer[25] == 0)
                ++longer[24];
            CHECK(boat_net_decode(longer, s->size + 1, &m) != BOAT_NET_OK);
        }
    }
    printf("codec: %u types round-trip; %u truncations rejected\n", n, truncations);

    // Header faults.
    uint8_t b[BOAT_NET_MAX_MESSAGE];
    const Sample *ping = &sample[12];
    struct {
        size_t at;
        uint8_t value;
        BoatNetError error;
    } header_faults[] = { { 0, 0x51, BOAT_NET_BAD_MAGIC }, { 5, 2, BOAT_NET_BAD_VERSION },
        { 6, 0, BOAT_NET_BAD_TYPE }, { 6, BOAT_NET_TYPE_LIMIT, BOAT_NET_BAD_TYPE },
        { 7, 1, BOAT_NET_BAD_HEADER }, { 26, 1, BOAT_NET_BAD_HEADER },
        { 31, 1, BOAT_NET_BAD_HEADER }, { 24, 0xff, BOAT_NET_BAD_HEADER } };
    for (size_t i = 0; i < sizeof(header_faults) / sizeof(*header_faults); ++i) {
        memcpy(b, ping->bytes, ping->size);
        b[header_faults[i].at] = header_faults[i].value;
        CHECK(boat_net_decode(b, ping->size, &m) == header_faults[i].error);
    }

    // Every snapshot float of every slot: NaN and infinity are refused.
    const Sample *snap = &sample[9];
    unsigned nonfinite = 0;
    for (unsigned slot = 0; slot < BOAT_FLEET_SLOTS; ++slot) {
        size_t base = 32 + 32 + slot * 172;
        size_t floats[27];
        for (unsigned k = 0; k < 25; ++k)
            floats[k] = base + 36 + 4 * k;
        floats[25] = base + 136;
        floats[26] = base + 140;
        for (unsigned k = 0; k < 27; ++k)
            for (int inf = 0; inf < 2; ++inf, ++nonfinite) {
                memcpy(b, snap->bytes, snap->size);
                putf(b + floats[k], inf ? INFINITY : NAN);
                CHECK(boat_net_decode(b, snap->size, &m) == BOAT_NET_NONFINITE);
            }
        size_t more[] = { base + 164, base + 168 };
        for (unsigned k = 0; k < 2; ++k, ++nonfinite) {
            memcpy(b, snap->bytes, snap->size);
            putf(b + more[k], NAN);
            CHECK(boat_net_decode(b, snap->size, &m) == BOAT_NET_NONFINITE);
        }
    }
    struct {
        size_t at;
        uint8_t value;
        BoatNetError error;
    } snapshot_faults[] = {
        { 64 + 0, 4, BOAT_NET_BAD_FIELD }, // Slot flags.
        { 64 + 1, 4, BOAT_NET_BAD_FIELD }, // Motion state.
        { 64 + 2, BOAT_NET_INPUT_STATES, BOAT_NET_BAD_FIELD },
        { 64 + 3, 1, BOAT_NET_BAD_FIELD }, // Reserved.
        { 64 + 12, 0xff, BOAT_NET_BAD_FIELD }, // Request tick after the snapshot.
        { 64 + 28, 0x80, BOAT_NET_BAD_FIELD }, // -128 is never a rate.
        { 64 + 30, 2, BOAT_NET_BAD_FIELD }, // Command flags.
        { 64 + 160, 32, BOAT_NET_BAD_FIELD }, // Emitter cursor.
        { 64 + 161, 2, BOAT_NET_BAD_FIELD }, // Emitter initialized.
        { 32 + 4, 0xff, BOAT_NET_BAD_FIELD }, // Live count.
    };
    for (size_t i = 0; i < sizeof(snapshot_faults) / sizeof(*snapshot_faults); ++i) {
        memcpy(b, snap->bytes, snap->size);
        b[snapshot_faults[i].at] = snapshot_faults[i].value;
        CHECK(boat_net_decode(b, snap->size, &m) == snapshot_faults[i].error);
    }
    float ranges[][2] = { { 64 + 36, 1e4f }, { 64 + 36 + 4 * 5, 1.5f }, { 64 + 36 + 4 * 7, -.1f },
        { 64 + 36 + 4 * 8, -2000 }, { 64 + 36 + 4 * 14, -1 }, { 64 + 36 + 4 * 20, 4 } };
    for (size_t i = 0; i < sizeof(ranges) / sizeof(*ranges); ++i) {
        memcpy(b, snap->bytes, snap->size);
        putf(b + (size_t)ranges[i][0], ranges[i][1]);
        CHECK(boat_net_decode(b, snap->size, &m) == BOAT_NET_OUT_OF_RANGE);
    }
    // Other messages' counts, indices, strings and enums.
    struct {
        unsigned sample;
        size_t at;
        uint8_t value;
    } field_faults[] = { { 8, 36, 0 }, { 8, 36, BOAT_NET_MAX_COMMANDS + 1 }, { 8, 50, 2 },
        { 8, 51, 1 }, { 10, 32, BOAT_NET_MAX_BIRTHS + 1 }, { 10, 36, 200 }, { 5, 36, 6 },
        { 4, 40, 7 }, { 2, 32, 0 }, { 2, 32, BOAT_NET_REJECT_LIMIT }, { 0, 44, 0x01 },
        { 0, 46, 0x7f }, { 0, 44, 17 }, { 1, 32, BOAT_FLEET_SLOTS }, { 1, 33, 2 }, { 1, 41, 0 },
        { 3, 32 + 19, 3 }, { 3, 32 + 20, 17 }, { 3, 32 + 21 + 15, 'x' }, { 14, 32, 3 },
        { 15, 32, 0 }, { 6, 36, 1 } };
    for (size_t i = 0; i < sizeof(field_faults) / sizeof(*field_faults); ++i) {
        const Sample *s = &sample[field_faults[i].sample];
        memcpy(b, s->bytes, s->size);
        b[field_faults[i].at] = field_faults[i].value;
        if (boat_net_decode(b, s->size, &m) == BOAT_NET_OK) {
            fprintf(stderr, "field fault %zu accepted\n", i);
            return 1;
        }
    }
    // The encoder refuses what the decoder would refuse.
    BoatNetMessage bad
        = { .header = { .type = BOAT_NET_INPUT_BATCH, .tick = 5 }, .input = { .count = 0 } };
    CHECK(boat_net_encode(&bad, b) == 0);
    bad = (BoatNetMessage) { .header = { .type = BOAT_NET_HELLO } };
    strcpy(bad.hello.name, "tab\there");
    CHECK(boat_net_encode(&bad, b) == 0);
    printf("codec: %u non-finite floats and header/field/range faults rejected\n", nonfinite);
    return 0;
}

static int checkpoints(void)
{
    static BoatFleet fleet, copy;
    static uint8_t bytes[BOAT_NET_MAX_CHECKPOINT], again[BOAT_NET_MAX_CHECKPOINT];
    static BoatFleetState state;
    for (int waves = 0; waves < 2; ++waves) {
        busy_fleet(&fleet, 2400, waves);
        boat_fleet_request_spawn(&fleet, 2); // A pending reset is part of the checkpoint.
        BoatNetSlotInput records[BOAT_FLEET_SLOTS] = { { 2399, 1, { 127, 0, 0 } } };
        size_t size = boat_net_checkpoint_encode(&fleet.state, records, bytes, sizeof(bytes));
        CHECK(size > 0 && size <= BOAT_NET_MAX_CHECKPOINT);
        CHECK(fleet.state.wakes.live > 20);
        BoatNetSlotInput decoded[BOAT_FLEET_SLOTS];
        CHECK(boat_net_checkpoint_decode(bytes, size, &state, decoded) == BOAT_NET_OK);
        CHECK(boat_fleet_hash(&state) == boat_fleet_hash(&fleet.state));
        CHECK(decoded[0].tick == 2399 && decoded[0].used.steering == 127);
        CHECK(boat_net_checkpoint_encode(&state, decoded, again, sizeof(again)) == size);
        CHECK(memcmp(bytes, again, size) == 0);
        boat_fleet_init(&copy, basin(), boat_world_test_water(basin(), waves, false, false),
            BOAT_OBJECT_LIFE_TICKS, 1.8f);
        boat_fleet_restore(&copy, &state);
        BoatFleetBots a = { 0 }, b = { 0 };
        for (unsigned t = 0; t < 600; ++t) {
            BoatInput ia[BOAT_FLEET_SLOTS], ib[BOAT_FLEET_SLOTS];
            boat_fleet_bots_drive(&a, &fleet, ia);
            boat_fleet_bots_drive(&b, &copy, ib);
            boat_fleet_step(&fleet, ia);
            boat_fleet_step(&copy, ib);
        }
        CHECK(boat_fleet_hash(&copy.state) == boat_fleet_hash(&fleet.state));
        for (size_t length = 0; length < size; length += 97)
            CHECK(boat_net_checkpoint_decode(bytes, length, &state, decoded) != BOAT_NET_OK);
        bytes[35] ^= 1; // Live count no longer matches the records.
        CHECK(boat_net_checkpoint_decode(bytes, size, &state, decoded) != BOAT_NET_OK);
        printf("checkpoint: %s water, %zu bytes, %u live records, resumed 600 ticks "
               "bit-identically\n",
            waves ? "wave" : "calm", size, fleet.state.wakes.live);
    }
    return 0;
}

// A virtual room: one authority, clients driven by bots from their own mirrors.
#define ROOM_CLIENTS 6
#define ROOM_HISTORY 1024
typedef struct {
    BoatNetMemory *network;
    BoatNetServer server;
    BoatNetClient clients[ROOM_CLIENTS];
    unsigned link[ROOM_CLIENTS];
    bool live[ROOM_CLIENTS]; // Serviced each step.
    bool inactive[ROOM_CLIENTS];
    BoatFleetBots bots[ROOM_CLIENTS];
    unsigned count;
    double now;
    uint64_t history_tick[ROOM_HISTORY], history_hash[ROOM_HISTORY];
    uint64_t checked[ROOM_CLIENTS];
    unsigned checks, matches, mismatches;
} Room;

static bool room_open(Room *room, uint64_t seed, BoatNetServerConfig config)
{
    *room = (Room) { .network = boat_net_memory_create(seed) };
    if (!config.epoch)
        config.epoch = 0x5eed;
    if (!config.max_players)
        config.max_players = BOAT_FLEET_SLOTS;
    if (!config.token)
        config.token = "room";
    return room->network
        && boat_net_server_init(&room->server, boat_net_memory_server(room->network), &config);
}

static void room_close(Room *room) { boat_net_memory_destroy(room->network); }

static unsigned room_join(Room *room, const char *name, const char *token, uint32_t sequence)
{
    unsigned i = room->count++;
    BoatNetTransport transport;
    boat_net_memory_client(room->network, &room->link[i], &transport);
    BoatNetClientConfig config = { name, token ? token : "room", "validate", sequence };
    boat_net_client_init(&room->clients[i], transport, &config, room->now);
    room->live[i] = true;
    return i;
}

static void room_step(Room *room, double dt)
{
    room->now += dt;
    boat_net_memory_set_time(room->network, room->now);
    boat_net_server_service(&room->server, room->now);
    uint64_t tick = room->server.fleet.state.tick;
    room->history_tick[tick % ROOM_HISTORY] = tick;
    room->history_hash[tick % ROOM_HISTORY] = boat_fleet_hash(&room->server.fleet.state);
    for (unsigned i = 0; i < room->count; ++i) {
        BoatNetClient *c = &room->clients[i];
        if (!room->live[i])
            continue;
        BoatInput input = { 0 };
        if (boat_net_client_own(c))
            input = boat_fleet_bot_input(&room->bots[i], &c->fleet, c->slot);
        room->live[i] = boat_net_client_service(c, room->now, input, room->inactive[i]);
        // At birth-batch ticks a lossless link has delivered every birth the snapshot names.
        uint64_t mirror = c->fleet.state.tick;
        if (c->phase == BOAT_NET_CLIENT_PLAYING && mirror != room->checked[i]
            && mirror % BOAT_NET_BIRTH_TICKS == 0
            && room->history_tick[mirror % ROOM_HISTORY] == mirror) {
            room->checked[i] = mirror;
            ++room->checks;
            if (boat_fleet_hash(&c->fleet.state) == room->history_hash[mirror % ROOM_HISTORY])
                ++room->matches;
            else
                ++room->mismatches;
        }
    }
}

static void room_run(Room *room, double seconds)
{
    // Uneven service steps exercise the accumulator and catch-up.
    for (double end = room->now + seconds; room->now < end;)
        room_step(room, (1 + (unsigned)(room->now * 977) % 3) / 240.0);
}

static unsigned active_boats(const BoatNetServer *server)
{
    unsigned n = 0;
    for (unsigned i = 0; i < BOAT_FLEET_SLOTS; ++i)
        n += server->fleet.state.slots[i].active;
    return n;
}

static int lossless(void)
{
    static Room room;
    CHECK(room_open(&room, 1, (BoatNetServerConfig) { 0 }));
    unsigned a = room_join(&room, "alpha", NULL, 0);
    room_run(&room, 8);
    unsigned b = room_join(&room, "beta", NULL, 0); // Late join with a populated pool.
    room_run(&room, 12);
    const BoatNetServer *s = &room.server;
    for (unsigned i = 0; i < 2; ++i) {
        const BoatNetClient *c = &room.clients[i == 0 ? a : b];
        CHECK(c->phase == BOAT_NET_CLIENT_PLAYING);
        CHECK(boat_net_client_own(c) != NULL);
        CHECK(c->stats.decode_errors == 0 && c->stats.resyncs == 0);
        CHECK(c->roster.slots[0].state == BOAT_NET_ROSTER_PLAYING);
        CHECK(c->roster.slots[1].state == BOAT_NET_ROSTER_PLAYING);
        CHECK(strcmp(c->roster.slots[1].name, "beta") == 0);
    }
    CHECK(room.clients[a].slot == 0 && room.clients[b].slot == 1);
    CHECK(active_boats(s) == 2);
    CHECK(s->fleet.state.wakes.live > 10);
    CHECK(room.checks > 100 && room.mismatches == 0);
    // The authority integrated the clients' own commands almost every tick.
    uint64_t used = 0;
    for (unsigned k = 0; k < BOAT_NET_INPUT_STATES; ++k)
        used += s->stats.input_states[k];
    CHECK(s->stats.input_states[BOAT_NET_INPUT_RECEIVED] > used * 95 / 100);
    CHECK(s->stats.output_faults == 0 && s->stats.emission_failures == 0);
    CHECK(s->stats.malformed == 0 && s->stats.forbidden == 0 && s->stats.late_commands < 20);
    CHECK(s->stats.max_backlog <= 2);
    // Both boats took their bots' throttle settings (0.65 and 0.75) from the commands.
    for (unsigned i = 0; i < 2; ++i)
        CHECK(fabsf(s->fleet.state.slots[i].boat.throttle - (.65f + .1f * (float)i)) < .02f
            || s->fleet.state.slots[i].boat.motion_state == BOAT_CAPSIZED);
    BoatNetTransportStats net = s->transport.stats(s->transport.context);
    CHECK(net.max_message <= BOAT_NET_MAX_MESSAGE && net.max_queued < BOAT_NET_MEMORY_QUEUE / 2);
    printf("lossless: 2 clients (one late) matched the authority hash at %u/%u checks; "
           "%.1f%% received commands, max message %zu bytes, bootstrap %" PRIu64 " bytes\n",
        room.matches, room.checks,
        100.0 * (double)s->stats.input_states[BOAT_NET_INPUT_RECEIVED] / (double)used,
        net.max_message, s->stats.bootstrap_bytes / s->stats.bootstraps);
    // A one-second local stall (a window opening) leaves pongs and snapshots queued; the
    // client must not then aim its commands beyond the authority's future window.
    uint64_t future = s->stats.future_commands;
    BoatNetImpairment lan = { .delay = .01 };
    boat_net_memory_impair(room.network, room.link[a], lan, lan);
    room_run(&room, .5);
    room.clients[a].ping_at = -1e9; // Leave a ping in flight when the stall begins.
    room_step(&room, 1 / 240.0);
    room.live[a] = false;
    room_run(&room, 1);
    room.live[a] = true;
    room_run(&room, 2);
    CHECK(s->stats.future_commands == future && room.clients[a].lead <= 6);
    CHECK(room.clients[a].rtt < .05);
    boat_net_memory_impair(
        room.network, room.link[a], (BoatNetImpairment) { 0 }, (BoatNetImpairment) { 0 });
    // Graceful leave: the hull goes, its wakes stay.
    unsigned live = s->fleet.state.wakes.live;
    boat_net_client_leave(&room.clients[b]);
    room.live[b] = false;
    room_run(&room, .1);
    CHECK(active_boats(s) == 1 && s->slot_peer[1] < 0 && s->stats.left == 1);
    CHECK(s->fleet.state.wakes.live + 4 >= live);
    CHECK(room.clients[a].roster.slots[1].state == BOAT_NET_ROSTER_EMPTY);
    room_close(&room);
    return 0;
}

// Send raw bytes from a fresh virtual client and return the server's first reply type.
static uint8_t raw_hello(Room *room, const uint8_t *bytes, size_t size, BoatNetMessage *reply)
{
    unsigned link;
    BoatNetTransport t;
    boat_net_memory_client(room->network, &link, &t);
    room_step(room, 1 / 240.0);
    BoatNetEvent event;
    while (boat_net_poll(&t, &event)) { }
    boat_net_send(&t, 0, BOAT_NET_CONTROL, bytes, size);
    for (int i = 0; i < 4; ++i)
        room_step(room, 1 / 240.0);
    while (boat_net_poll(&t, &event))
        if (event.kind == BOAT_NET_EVENT_MESSAGE
            && boat_net_decode(event.data, event.size, reply) == BOAT_NET_OK)
            return reply->header.type;
    return 0;
}

static int rejections(void)
{
    static Room room;
    CHECK(room_open(&room, 2, (BoatNetServerConfig) { .max_players = 1 }));
    unsigned first = room_join(&room, "first", NULL, 0);
    room_run(&room, 1);
    CHECK(room.clients[first].phase == BOAT_NET_CLIENT_PLAYING);
    uint64_t hash = boat_fleet_hash(&room.server.fleet.state);
    (void)hash;
    unsigned full = room_join(&room, "second", NULL, 0);
    unsigned token = room_join(&room, "third", "wrong", 0);
    room_run(&room, .5);
    CHECK(room.clients[full].phase == BOAT_NET_CLIENT_REJECTED);
    CHECK(room.clients[full].reject.reason == BOAT_NET_REJECT_FULL);
    CHECK(room.clients[token].phase == BOAT_NET_CLIENT_REJECTED);
    CHECK(room.clients[token].reject.reason == BOAT_NET_REJECT_TOKEN);
    CHECK(strcmp(boat_net_client_failure(&room.clients[token]), "wrong room token") == 0);
    // Crafted Hellos: another protocol version, handling revision and content.
    static BoatNetMessage reply;
    uint8_t bytes[BOAT_NET_MAX_MESSAGE];
    BoatNetMessage hello = { .header = { .type = BOAT_NET_HELLO },
        .hello = { BOAT_PHYSICS_REVISION, boat_net_content_digest(basin()), "x", "room", "" } };
    size_t size = boat_net_encode(&hello, bytes);
    bytes[5] = 2; // Protocol version 2.
    CHECK(raw_hello(&room, bytes, size, &reply) == BOAT_NET_REJECT);
    CHECK(reply.reject.reason == BOAT_NET_REJECT_VERSION && reply.reject.version == 1);
    hello.hello.physics_revision = BOAT_PHYSICS_REVISION + 1;
    size = boat_net_encode(&hello, bytes);
    CHECK(raw_hello(&room, bytes, size, &reply) == BOAT_NET_REJECT);
    CHECK(reply.reject.reason == BOAT_NET_REJECT_PHYSICS);
    CHECK(reply.reject.physics_revision == BOAT_PHYSICS_REVISION);
    hello.hello.physics_revision = BOAT_PHYSICS_REVISION;
    hello.hello.content_digest ^= 1;
    size = boat_net_encode(&hello, bytes);
    CHECK(raw_hello(&room, bytes, size, &reply) == BOAT_NET_REJECT);
    CHECK(reply.reject.reason == BOAT_NET_REJECT_CONTENT);
    uint8_t garbage[40] = { 0x50, 0x42, 0x4e, 0x54, 0, 1, BOAT_NET_HELLO };
    CHECK(raw_hello(&room, garbage, sizeof(garbage), &reply) == BOAT_NET_REJECT);
    CHECK(reply.reject.reason == BOAT_NET_REJECT_MALFORMED);
    const BoatNetServerStats *s = &room.server.stats;
    CHECK(s->rejected[BOAT_NET_REJECT_FULL] == 1 && s->rejected[BOAT_NET_REJECT_TOKEN] == 1);
    CHECK(s->rejected[BOAT_NET_REJECT_VERSION] == 1 && s->rejected[BOAT_NET_REJECT_PHYSICS] == 1);
    CHECK(s->rejected[BOAT_NET_REJECT_CONTENT] == 1);
    CHECK(s->rejected[BOAT_NET_REJECT_MALFORMED] == 1);
    // No rejected peer ever held a slot or a boat.
    CHECK(s->accepted == 1 && active_boats(&room.server) == 1);
    CHECK(room.clients[first].phase == BOAT_NET_CLIENT_PLAYING);
    for (unsigned peer = 0; peer < BOAT_NET_MAX_PEERS; ++peer)
        CHECK(peer == room.link[first] || room.server.peers[peer].phase == BOAT_NET_PEER_FREE);
    printf("rejections: full, token, version, handling, content and malformed Hellos refused "
           "before any slot\n");
    room_close(&room);
    return 0;
}

// Send a crafted message as client I, using its epoch and the next sequence on its channel.
static void inject(Room *room, unsigned i, BoatNetMessage m)
{
    BoatNetClient *c = &room->clients[i];
    BoatNetChannel channel = boat_net_channel(m.header.type);
    m.header.epoch = c->epoch;
    m.header.sequence = c->sequence[channel]++;
    uint8_t bytes[BOAT_NET_MAX_MESSAGE];
    size_t size = boat_net_encode(&m, bytes);
    if (size)
        boat_net_send(&c->transport, 0, channel, bytes, size);
}

static int authority_rules(void)
{
    static Room room;
    CHECK(room_open(&room, 3, (BoatNetServerConfig) { 0 }));
    unsigned i = room_join(&room, "driver", NULL, 0);
    room_run(&room, 3);
    BoatNetServer *s = &room.server;
    BoatNetClient *c = &room.clients[i];
    CHECK(boat_net_client_own(c));
    unsigned slot = c->slot;
    // Silence: repeat for six ticks, then zero, then neutral from 250 ms.
    room.live[i] = false;
    float throttle = s->fleet.state.slots[slot].boat.throttle;
    CHECK(throttle > .5f);
    uint8_t seen[160];
    unsigned ticks = 0;
    while (ticks < 160) {
        uint64_t before = s->fleet.state.tick;
        room_step(&room, 1 / 240.0);
        if (s->fleet.state.tick != before)
            seen[ticks++] = s->inputs[slot].state;
    }
    unsigned first_repeat = 0, first_zero = 0, first_neutral = 0;
    for (unsigned k = 0; k < ticks; ++k) {
        if (!first_repeat && seen[k] == BOAT_NET_INPUT_REPEATED)
            first_repeat = k + 1;
        if (!first_zero && seen[k] == BOAT_NET_INPUT_ZERO)
            first_zero = k + 1;
        if (!first_neutral && seen[k] == BOAT_NET_INPUT_NEUTRAL)
            first_neutral = k + 1;
    }
    CHECK(first_repeat && first_zero && first_neutral);
    CHECK(first_zero - first_repeat == BOAT_NET_REPEAT_TICKS);
    CHECK(first_neutral - first_repeat == BOAT_NET_NEUTRAL_TICKS - 1);
    CHECK(seen[ticks - 1] == BOAT_NET_INPUT_NEUTRAL);
    CHECK(s->fleet.state.slots[slot].boat.throttle < throttle - .3f);
    CHECK(s->fleet.state.slots[slot].active); // Still physical, never removed for silence.
    printf("input loss: repeat %u ticks, zero until tick %u, then neutral (throttle %.2f -> "
           "%.2f)\n",
        first_zero - first_repeat, first_neutral - first_repeat, throttle,
        s->fleet.state.slots[slot].boat.throttle);

    // Immutable commands, late/future/stale-life/stale-sequence rejection.
    uint32_t life = s->fleet.state.slots[slot].life;
    uint64_t target = s->fleet.state.tick + 10;
    BoatNetMessage batch = { .header = { .type = BOAT_NET_INPUT_BATCH, .tick = target },
        .input = { .life = life, .count = 1 } };
    batch.input.commands[0] = boat_net_command((BoatInput) { 1, 0 }, 0);
    inject(&room, i, batch);
    batch.input.commands[0] = boat_net_command((BoatInput) { -1, 0 }, 0);
    inject(&room, i, batch); // Same tick, other command: ignored.
    BoatNetServerStats before = s->stats;
    room_step(&room, 1 / 480.0);
    CHECK(s->stats.duplicate_commands == before.duplicate_commands + 1);
    CHECK(s->control[slot].ring[target % BOAT_NET_INPUT_RING].command.steering == 127);
    batch.header.tick = s->fleet.state.tick; // Already finalized.
    inject(&room, i, batch);
    batch.header.tick = s->fleet.state.tick + BOAT_NET_MAX_FUTURE_TICKS + 1;
    inject(&room, i, batch);
    batch.header.tick = target + 1;
    batch.input.life = life + 1;
    inject(&room, i, batch);
    room_step(&room, 1 / 480.0);
    CHECK(s->stats.late_commands >= before.late_commands + 1);
    CHECK(s->stats.future_commands == before.future_commands + 1);
    CHECK(s->stats.stale_life_commands == before.stale_life_commands + 1);
    // An older sequence on the command channel is discarded as stale.
    batch.input.life = life;
    BoatNetMessage old = batch;
    old.header.epoch = c->epoch;
    old.header.sequence = c->sequence[BOAT_NET_STATE] - 10;
    uint8_t bytes[BOAT_NET_MAX_MESSAGE];
    size_t size = boat_net_encode(&old, bytes);
    boat_net_send(&c->transport, 0, BOAT_NET_STATE, bytes, size);
    // Another session's epoch never reaches the room.
    old.header.epoch = c->epoch + 1;
    old.header.sequence = c->sequence[BOAT_NET_STATE] + 100;
    size = boat_net_encode(&old, bytes);
    boat_net_send(&c->transport, 0, BOAT_NET_STATE, bytes, size);
    room_step(&room, 1 / 480.0);
    CHECK(s->stats.stale_packets >= before.stale_packets + 2);
    CHECK(s->control[slot].ring[(target + 1) % BOAT_NET_INPUT_RING].tick != target + 1);
    room.live[i] = true;
    room_run(&room, 1);
    CHECK(s->control[slot].ring[target % BOAT_NET_INPUT_RING].tick != target
        || s->control[slot].ring[target % BOAT_NET_INPUT_RING].command.steering == 127);

    // Inactive clients are neutralized at once, not after 250 ms.
    room.inactive[i] = true;
    room_run(&room, .3);
    CHECK(s->inputs[slot].state == BOAT_NET_INPUT_NEUTRAL);
    CHECK(s->inputs[slot].used.flags & BOAT_NET_COMMAND_INACTIVE);
    room.inactive[i] = false;
    room_run(&room, .5);
    CHECK(s->inputs[slot].state == BOAT_NET_INPUT_RECEIVED);

    // Reset: one request is one new life; a repeat naming the old life does nothing more.
    life = s->fleet.state.slots[slot].life;
    boat_net_client_request_reset(c);
    room_run(&room, 2);
    CHECK(s->fleet.state.slots[slot].life == life + 1);
    inject(&room, i,
        (BoatNetMessage) { .header = { .type = BOAT_NET_SPAWN_REQUEST }, .spawn = { 99, life } });
    room_run(&room, 1);
    CHECK(s->fleet.state.slots[slot].life == life + 1 && !s->fleet.state.slots[slot].pending);
    CHECK(boat_net_client_own(c)->life == life + 1);

    // No client transform is accepted: world-state messages from a client are forbidden and
    // change nothing; persistent offenders are disconnected.
    BoatNetMessage forged
        = { .header = { .type = BOAT_NET_SNAPSHOT, .tick = c->fleet.state.tick } };
    BoatNetSlotInput records[BOAT_FLEET_SLOTS] = { 0 };
    BoatFleetState fake = c->fleet.state;
    fake.slots[slot].boat.position = (Vector2) { 99, 66 };
    boat_net_snapshot_of(&fake, records, &forged.snapshot);
    inject(&room, i, forged);
    room_step(&room, 1 / 240.0);
    CHECK(s->stats.forbidden == 1);
    CHECK(Vector2Distance(s->fleet.state.slots[slot].boat.position, (Vector2) { 99, 66 }) > 5);
    for (unsigned k = 0; k < BOAT_NET_MISBEHAVIOUR_LIMIT; ++k)
        inject(&room, i,
            (BoatNetMessage) { .header = { .type = BOAT_NET_WORLD_END }, .checkpoint = 1 });
    room_run(&room, .5);
    CHECK(s->stats.misbehaving == 1 && s->slot_peer[slot] < 0 && active_boats(s) == 0);
    CHECK(c->phase == BOAT_NET_CLIENT_CLOSED && c->leave == BOAT_NET_LEAVE_MISBEHAVING);
    printf("authority: immutable, late, future, stale-life, stale-sequence and foreign-epoch "
           "commands handled; inactive neutral; idempotent reset; forged state refused\n");
    room_close(&room);
    return 0;
}

static int wrap_and_flood(void)
{
    static Room room;
    uint32_t start = UINT32_MAX - 100;
    CHECK(room_open(&room, 4, (BoatNetServerConfig) { .first_sequence = start }));
    unsigned i = room_join(&room, "wrap", NULL, start);
    room_run(&room, 6);
    BoatNetClient *c = &room.clients[i];
    CHECK(c->phase == BOAT_NET_CLIENT_PLAYING);
    CHECK(c->sequence[BOAT_NET_STATE] < start && c->received[BOAT_NET_STATE] < start);
    CHECK(room.server.peers[room.link[i]].input_sequence < start);
    CHECK(c->stats.stale_packets == 0 && room.server.stats.stale_packets == 0);
    CHECK(room.checks > 30 && room.mismatches == 0);
    printf("sequence wrap: both directions wrapped with no stale discards; %u/%u hash "
           "matches\n",
        room.matches, room.checks);
    // Flood: far more packets than the bounded queue and rate allow, in one instant.
    uint64_t tick = room.server.fleet.state.tick;
    BoatNetMessage batch = { .header = { .type = BOAT_NET_INPUT_BATCH, .tick = tick + 5 },
        .input = { .life = room.server.fleet.state.slots[c->slot].life, .count = 1 } };
    for (unsigned k = 0; k < 2000; ++k)
        inject(&room, i, batch);
    BoatNetTransportStats sent = c->transport.stats(c->transport.context);
    CHECK(sent.dropped > 1000 && sent.max_queued <= BOAT_NET_MEMORY_QUEUE);
    room_run(&room, 1);
    const BoatNetServerStats *s = &room.server.stats;
    CHECK(s->rate_limited > 100 && s->max_messages_per_service <= BOAT_NET_MESSAGES_PER_SERVICE);
    CHECK(room.server.fleet.state.tick >= tick + 115 && s->max_backlog <= 2);
    CHECK(c->phase == BOAT_NET_CLIENT_PLAYING);
    printf("flood: %" PRIu64 " sends dropped at the %d-message queue, %" PRIu64
           " packets rate-limited, authority stayed on time\n",
        sent.dropped, BOAT_NET_MEMORY_QUEUE, s->rate_limited);
    room_close(&room);
    return 0;
}

static int impaired(void)
{
    static Room room;
    CHECK(room_open(&room, 5, (BoatNetServerConfig) { .waves = true }));
    // Profile F one way: 40 ms +-10, 1% loss, 0.2% duplicates, 1% reordered by 20 ms.
    BoatNetImpairment f = { .04, .01, .01, .002, .01, .02 };
    unsigned a = room_join(&room, "f-one", NULL, 0);
    unsigned b = room_join(&room, "f-two", NULL, 0);
    boat_net_memory_impair(room.network, room.link[a], f, f);
    boat_net_memory_impair(room.network, room.link[b], f, f);
    room_run(&room, 60);
    const BoatNetServerStats *s = &room.server.stats;
    for (unsigned i = 0; i < 2; ++i) {
        const BoatNetClient *c = &room.clients[i];
        CHECK(c->phase == BOAT_NET_CLIENT_PLAYING && c->stats.decode_errors == 0);
        CHECK(c->stats.resyncs <= 3);
        CHECK(room.server.fleet.state.tick - c->fleet.state.tick < 24);
        CHECK(c->rtt > .06 && c->rtt < .12);
        CHECK(c->lead >= 4 && c->lead <= 16);
        printf("impaired F: client %u rtt %.0f ms lead %u, %" PRIu64 " snapshots, %" PRIu64
               " repaired mismatches (max %.0f ms), %" PRIu64 " resyncs\n",
            i, c->rtt * 1000, c->lead, c->stats.snapshots, c->stats.repaired,
            c->stats.max_mismatch * 1000, c->stats.resyncs);
    }
    uint64_t used = 0;
    for (unsigned k = 0; k < BOAT_NET_INPUT_STATES; ++k)
        used += s->input_states[k];
    double late = 1 - (double)s->input_states[BOAT_NET_INPUT_RECEIVED] / (double)used;
    CHECK(late < .02);
    CHECK(room.matches > room.checks * 8 / 10);
    CHECK(s->output_faults == 0 && s->emission_failures == 0 && s->malformed == 0);
    BoatNetTransportStats net = room.server.transport.stats(room.server.transport.context);
    CHECK(net.max_queued < BOAT_NET_MEMORY_QUEUE);
    printf("impaired F: %.2f%% of integrated ticks lacked a command; %u/%u hash matches; "
           "queue high-water %zu\n",
        late * 100, room.matches, room.checks, net.max_queued);

    // A one-second blackout recovers; six seconds times the slot out on both ends.
    unsigned slot = room.clients[b].slot;
    boat_net_memory_blackout(room.network, room.link[b], true);
    room_run(&room, 1);
    boat_net_memory_blackout(room.network, room.link[b], false);
    room_run(&room, 3);
    CHECK(room.clients[b].phase == BOAT_NET_CLIENT_PLAYING && room.server.slot_peer[slot] >= 0);
    CHECK(!boat_net_client_stale(&room.clients[b], room.now));
    boat_net_memory_blackout(room.network, room.link[b], true);
    room_run(&room, 6);
    CHECK(room.server.slot_peer[slot] < 0 && s->timeouts == 1);
    CHECK(room.clients[b].phase == BOAT_NET_CLIENT_CLOSED);
    CHECK(room.clients[a].phase == BOAT_NET_CLIENT_PLAYING);
    printf("outages: 1 s blackout recovered; 6 s blackout timed out on both ends\n");
    room_close(&room);
    return 0;
}

static int resync(void)
{
    static Room room;
    CHECK(room_open(&room, 6, (BoatNetServerConfig) { 0 }));
    unsigned i = room_join(&room, "resync", NULL, 0);
    room_run(&room, 6);
    BoatNetClient *c = &room.clients[i];
    CHECK(c->fleet.state.wakes.live > 3);
    // Damage the mirror: a live record the authority never had.
    BoatWakeObject *o = &c->fleet.state.wakes.objects[127];
    o->born = c->fleet.state.tick;
    o->amplitude = .5f;
    room_run(&room, .5);
    CHECK(!c->wake_consistent && c->stats.resyncs == 0);
    room_run(&room, 1.5);
    CHECK(c->stats.resyncs == 1 && c->stats.bootstraps == 2 && room.server.stats.resyncs == 1);
    CHECK(c->wake_consistent);
    CHECK(boat_net_client_own(c) != NULL && room.server.stats.spawn_requests == 0);
    unsigned matches = room.matches;
    room_run(&room, 2);
    CHECK(room.matches > matches);
    printf("resync: a damaged mirror asked for one fresh checkpoint after 1 s and matched "
           "again\n");
    room_close(&room);
    return 0;
}

int main(void)
{
    RUN(golden());
    RUN(codec());
    RUN(checkpoints());
    RUN(lossless());
    RUN(rejections());
    RUN(authority_rules());
    RUN(wrap_and_flood());
    RUN(impaired());
    RUN(resync());
    printf("boat_net=ok\n");
    return 0;
}
