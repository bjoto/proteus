#ifndef PROTEUS_BOAT_MAP_H
#define PROTEUS_BOAT_MAP_H

#include "raylib.h"

// Simulation X is reflected relative to the bundled map's east coordinate.
bool boat_map_water(Vector2 position, float clearance);
void boat_map_destroy(void);

#endif
