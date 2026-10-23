#ifndef PROTEUS_BOAT_NET_ENET_H
#define PROTEUS_BOAT_NET_ENET_H

#include "boat_net_transport.h"

// ENet adapter for the transport boundary: IPv4 only, five channels mapped from the delivery
// classes, a 1200-byte UDP payload MTU and bounded packet/waiting-data allocations. ENet types
// stay inside boat_net_enet.c.
#define BOAT_NET_ENET_MTU 1200
#define BOAT_NET_ENET_WAITING_BYTES (64 * 1024) // Per peer, as ENet counts it.

typedef struct BoatNetEnet BoatNetEnet;

// Once per process before any host, and after the last one.
bool boat_net_enet_startup(void);
void boat_net_enet_cleanup(void);
// Listen on a numeric IPv4 ADDRESS for BOAT_NET_MAX_PEERS connections. NULL with ERROR set.
BoatNetEnet *boat_net_enet_listen(const char *address, uint16_t port, char *error, size_t size);
// Start connecting to HOST (numeric or a name resolved to IPv4); the transport reports the
// connection as an event. NULL with ERROR set.
BoatNetEnet *boat_net_enet_connect(const char *host, uint16_t port, char *error, size_t size);
BoatNetTransport boat_net_enet_transport(BoatNetEnet *enet);
// Send queued packets now.
void boat_net_enet_flush(BoatNetEnet *enet);
// Block up to SECONDS for network activity; the event (if any) is returned by the next poll.
void boat_net_enet_wait(BoatNetEnet *enet, double seconds);
// Disconnect every peer gracefully, waiting up to SECONDS, then destroy the host.
void boat_net_enet_close(BoatNetEnet *enet, double seconds);
// Negotiated UDP payload MTU of transport peer PEER, or zero when not connected.
unsigned boat_net_enet_peer_mtu(BoatNetEnet *enet, unsigned peer);

// Monotonic seconds for scheduling (not wall-clock time).
double boat_net_clock(void);
void boat_net_sleep(double seconds);
// Parse "HOST:PORT" (PORT 1..65535). HOST is copied into HOST_OUT.
bool boat_net_parse_endpoint(const char *text, char *host_out, size_t host_size, uint16_t *port);
bool boat_net_parse_port(const char *text, uint16_t *port);

#endif
