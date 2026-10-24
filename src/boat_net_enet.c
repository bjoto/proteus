#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L // clock_gettime and nanosleep under strict C17.
#endif
#include "boat_net_enet.h"

#ifdef _WIN32
// ENet includes windows.h after raylib; exclude the GDI/user names raylib also declares.
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#endif
#include <enet/enet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <time.h>
#endif

struct BoatNetEnet {
    ENetHost *host;
    ENetPeer *server; // Client side only.
    bool client, stashed;
    ENetEvent stash;
    BoatNetTransportStats stats;
};

// Largest UDP datagram any host in this process received (the intercept has no context).
static size_t max_datagram;

static int intercept(ENetHost *host, ENetEvent *event)
{
    (void)event;
    if (host->receivedDataLength > max_datagram)
        max_datagram = host->receivedDataLength;
    return 0; // Continue with ENet's own processing.
}

bool boat_net_enet_startup(void) { return enet_initialize() == 0; }

void boat_net_enet_cleanup(void) { enet_deinitialize(); }

static BoatNetEnet *create(const ENetAddress *address, size_t peers, char *error, size_t size)
{
    BoatNetEnet *e = calloc(1, sizeof(*e));
    if (!e) {
        snprintf(error, size, "out of memory");
        return NULL;
    }
    e->host = enet_host_create(address, peers, BOAT_NET_CHANNELS, 0, 0);
    if (!e->host) {
        free(e);
        return NULL;
    }
    e->host->mtu = BOAT_NET_ENET_MTU;
    e->host->maximumPacketSize = BOAT_NET_MAX_MESSAGE;
    e->host->maximumWaitingData = BOAT_NET_ENET_WAITING_BYTES;
    e->host->intercept = intercept;
    return e;
}

BoatNetEnet *boat_net_enet_listen(const char *address, uint16_t port, char *error, size_t size)
{
    ENetAddress bind = { .port = port };
    if (enet_address_set_host_ip(&bind, address) != 0) {
        snprintf(error, size, "invalid IPv4 bind address '%s'", address);
        return NULL;
    }
    BoatNetEnet *e = create(&bind, BOAT_NET_MAX_PEERS, error, size);
    if (!e)
        snprintf(error, size, "cannot listen on %s:%u (address in use or not available)", address,
            (unsigned)port);
    return e;
}

BoatNetEnet *boat_net_enet_connect(const char *host, uint16_t port, char *error, size_t size)
{
    ENetAddress address = { .port = port };
    if (enet_address_set_host(&address, host) != 0) {
        snprintf(error, size, "cannot resolve '%s' to an IPv4 address", host);
        return NULL;
    }
    BoatNetEnet *e = create(NULL, 1, error, size);
    if (!e) {
        snprintf(error, size, "cannot create a UDP socket");
        return NULL;
    }
    e->client = true;
    e->server = enet_host_connect(e->host, &address, BOAT_NET_CHANNELS, 0);
    if (!e->server) {
        snprintf(error, size, "cannot start connecting to %s:%u", host, (unsigned)port);
        enet_host_destroy(e->host);
        free(e);
        return NULL;
    }
    return e;
}

static ENetPeer *peer_of(BoatNetEnet *e, unsigned peer)
{
    if (e->client)
        return peer == 0 ? e->server : NULL;
    return peer < e->host->peerCount ? &e->host->peers[peer] : NULL;
}

static unsigned index_of(BoatNetEnet *e, const ENetPeer *peer)
{
    return e->client ? 0 : (unsigned)(peer - e->host->peers);
}

static bool poll_event(void *context, BoatNetEvent *event)
{
    BoatNetEnet *e = context;
    for (;;) {
        ENetEvent in;
        if (e->stashed) {
            in = e->stash;
            e->stashed = false;
        } else if (enet_host_service(e->host, &in, 0) <= 0) {
            return false;
        }
        event->peer = index_of(e, in.peer);
        event->size = 0;
        switch (in.type) {
        case ENET_EVENT_TYPE_CONNECT:
            event->kind = BOAT_NET_EVENT_CONNECT;
            return true;
        case ENET_EVENT_TYPE_DISCONNECT:
            event->kind = BOAT_NET_EVENT_DISCONNECT;
            return true;
        case ENET_EVENT_TYPE_RECEIVE: {
            ENetPacket *packet = in.packet;
            bool valid = packet->dataLength > 0 && packet->dataLength <= BOAT_NET_MAX_MESSAGE
                && in.channelID < BOAT_NET_CHANNELS;
            if (valid) {
                event->kind = BOAT_NET_EVENT_MESSAGE;
                event->channel = (BoatNetChannel)in.channelID;
                event->size = packet->dataLength;
                memcpy(event->data, packet->data, packet->dataLength);
                ++e->stats.received;
                e->stats.received_bytes += packet->dataLength;
                if (packet->dataLength > e->stats.max_message)
                    e->stats.max_message = packet->dataLength;
            } else {
                ++e->stats.dropped;
            }
            enet_packet_destroy(packet);
            if (valid)
                return true;
            break;
        }
        default:
            break;
        }
    }
}

static bool send_message(
    void *context, unsigned peer, BoatNetChannel channel, const void *data, size_t size)
{
    BoatNetEnet *e = context;
    ENetPeer *p = peer_of(e, peer);
    if (!p || p->state != ENET_PEER_STATE_CONNECTED || size == 0 || size > BOAT_NET_MAX_MESSAGE
        || channel >= BOAT_NET_CHANNELS) {
        ++e->stats.dropped;
        return false;
    }
    // Messages fit one datagram, so unreliable ones never take ENet's reliable fragment path.
    enet_uint32 flags = channel == BOAT_NET_CONTROL || channel == BOAT_NET_BOOTSTRAP
        ? ENET_PACKET_FLAG_RELIABLE
        : channel == BOAT_NET_REGISTRY ? ENET_PACKET_FLAG_UNSEQUENCED
                                       : 0;
    ENetPacket *packet = enet_packet_create(data, size, flags);
    if (!packet || enet_peer_send(p, (enet_uint8)channel, packet) < 0) {
        if (packet)
            enet_packet_destroy(packet);
        ++e->stats.dropped;
        return false;
    }
    ++e->stats.sent;
    e->stats.sent_bytes += size;
    if (size > e->stats.max_message)
        e->stats.max_message = size;
    return true;
}

static void disconnect(void *context, unsigned peer)
{
    BoatNetEnet *e = context;
    ENetPeer *p = peer_of(e, peer);
    if (p && p->state != ENET_PEER_STATE_DISCONNECTED)
        enet_peer_disconnect_later(p, 0);
}

static BoatNetTransportStats stats(void *context)
{
    BoatNetEnet *e = context;
    BoatNetTransportStats s = e->stats;
    s.max_datagram = max_datagram;
    return s;
}

BoatNetTransport boat_net_enet_transport(BoatNetEnet *enet)
{
    return (BoatNetTransport) { enet, poll_event, send_message, disconnect, stats };
}

void boat_net_enet_flush(BoatNetEnet *enet) { enet_host_flush(enet->host); }

void boat_net_enet_wait(BoatNetEnet *enet, double seconds)
{
    if (enet->stashed)
        return;
    enet_uint32 ms = seconds <= 0 ? 0 : seconds > 1 ? 1000 : (enet_uint32)(seconds * 1000);
    enet->stashed = enet_host_service(enet->host, &enet->stash, ms) > 0;
}

void boat_net_enet_close(BoatNetEnet *enet, double seconds)
{
    if (!enet)
        return;
    for (size_t i = 0; i < enet->host->peerCount; ++i) {
        ENetPeer *p = &enet->host->peers[i];
        if (p->state != ENET_PEER_STATE_DISCONNECTED)
            enet_peer_disconnect_later(p, 0);
    }
    double deadline = boat_net_clock() + seconds;
    while (enet->host->connectedPeers > 0 && boat_net_clock() < deadline) {
        ENetEvent event;
        if (enet_host_service(enet->host, &event, 10) > 0 && event.type == ENET_EVENT_TYPE_RECEIVE)
            enet_packet_destroy(event.packet);
    }
    enet_host_destroy(enet->host);
    free(enet);
}

unsigned boat_net_enet_peer_mtu(BoatNetEnet *enet, unsigned peer)
{
    ENetPeer *p = peer_of(enet, peer);
    return p && p->state == ENET_PEER_STATE_CONNECTED ? p->mtu : 0;
}

double boat_net_clock(void)
{
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)frequency.QuadPart;
#else
    struct timespec stamp;
    clock_gettime(CLOCK_MONOTONIC, &stamp);
    return (double)stamp.tv_sec + (double)stamp.tv_nsec * 1e-9;
#endif
}

void boat_net_sleep(double seconds)
{
    if (seconds <= 0)
        return;
#ifdef _WIN32
    Sleep((DWORD)(seconds * 1000));
#else
    struct timespec wait = { (time_t)seconds, (long)((seconds - (double)(time_t)seconds) * 1e9) };
    nanosleep(&wait, NULL);
#endif
}

bool boat_net_parse_port(const char *text, uint16_t *port)
{
    char *end;
    unsigned long value = strtoul(text, &end, 10);
    if (!*text || *end || text[0] == '-' || text[0] == '+' || value < 1 || value > 65535)
        return false;
    *port = (uint16_t)value;
    return true;
}

bool boat_net_parse_endpoint(const char *text, char *host_out, size_t host_size, uint16_t *port)
{
    const char *colon = strrchr(text, ':');
    if (!colon || colon == text || (size_t)(colon - text) >= host_size
        || !boat_net_parse_port(colon + 1, port))
        return false;
    memcpy(host_out, text, (size_t)(colon - text));
    host_out[colon - text] = 0;
    return strchr(host_out, ':') == NULL; // IPv4 or a name; no IPv6 literals in v1.
}
