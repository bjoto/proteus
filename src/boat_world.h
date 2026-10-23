#ifndef PROTEUS_BOAT_WORLD_H
#define PROTEUS_BOAT_WORLD_H

#include "boat_location.h"
#include "boat_water.h"

// One physical environment. Assignment clones it: the water history is held by value and the
// definition, arcade wake objects and wake field are borrowed, never owned or freed here.
typedef struct {
    const BoatWorldDefinition *definition;
    BoatWater water; // Settings plus the legacy D3 trail history that boats publish into.
    const BoatWake *wake_field; // Optional legacy CPU field endpoint (basin review only).
} BoatWorld;

void boat_world_init(BoatWorld *world, const BoatWorldDefinition *definition, BoatWater water);
// True when the surface moves: ambient/scripted/D3/object sources or a borrowed wake field.
bool boat_world_displaced(const BoatWorld *world);
// Study water with the definition's current and crossing-source placement.
BoatWater boat_world_test_water(
    const BoatWorldDefinition *definition, bool waves, bool current, bool crossing);

#endif
