#ifndef PROTEUS_BOAT_HUD_H
#define PROTEUS_BOAT_HUD_H

// Presentation-only telemetry, independent of session, physics and multiplayer ownership.
typedef struct {
    float speed_mps; // Ground-speed magnitude, including sideways/reverse motion.
    float thrust; // Held command: -1 reverse, 0 neutral, +1 ahead (not engine response).
    float steering; // Wheel setting: -1 port, +1 starboard (not yaw or hull heading).
} BoatHudTelemetry;

// Draw inside the current raylib drawing pass. Integer-scaled, bottom-centred; reserve
// the bottom 28 screen pixels for connection/review status. No resources or retained state.
void boat_hud_draw(BoatHudTelemetry telemetry, int width, int height);

#endif
