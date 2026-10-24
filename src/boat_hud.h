#ifndef PROTEUS_BOAT_HUD_H
#define PROTEUS_BOAT_HUD_H

// Presentation-only telemetry, independent of session, physics and multiplayer ownership.
typedef struct {
    float speed_mps; // Ground-speed magnitude, including sideways/reverse motion.
    float thrust; // Held command: -1 reverse, 0 neutral, +1 ahead (not engine response).
    float steering; // Wheel setting: -1 port, +1 starboard (not yaw or hull heading).
} BoatHudTelemetry;

// Draw at scene resolution (320x200) in a render texture, then present with the scene
// destination and nearest filtering. Composite over the scene before scaling so case
// transparency blends correctly. Anchored to the bottom edge. No retained state.
void boat_hud_draw(BoatHudTelemetry telemetry, int width, int height);

#endif
