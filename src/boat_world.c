#include "boat_world.h"

#include <assert.h>

void boat_world_init(BoatWorld *world, const BoatWorldDefinition *definition, BoatWater water)
{
    assert(definition);
    *world = (BoatWorld) { .definition = definition, .water = water };
}

bool boat_world_displaced(const BoatWorld *world)
{
    return world->wake_field || boat_water_displaced(world->water);
}

BoatWater boat_world_test_water(
    const BoatWorldDefinition *definition, bool waves, bool current, bool crossing)
{
    BoatWater water = boat_water_test(waves, current, crossing);
    water.current_center = definition->current_center;
    water.wake_offset = definition->wake_offset;
    return water;
}
