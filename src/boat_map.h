#ifndef PROTEUS_BOAT_MAP_H
#define PROTEUS_BOAT_MAP_H

#include "raylib.h"

// Simulation X is reflected relative to the bundled map's east coordinate.
bool boat_map_water(Vector2 position, float clearance);
// If the point is too close to shore, return the direction back toward open water.
bool boat_map_water_contact(Vector2 position, float clearance, Vector2 *water_normal);
void boat_map_destroy(void);

#endif
