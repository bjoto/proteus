/* Compare the frozen 2994e0d app driver against the extracted production helper. */
#include "boat_drive_baseline.h"
#include "boat_study_state.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x);                                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
int main(void)
{
    Boat a, b;
    ObjectStudy wa, wb;
    boat_reset(&a);
    boat_reset(&b);
    a.water_settings = b.water_settings = boat_water_test(false, false, false);
    baseline_reset(&wa, &a, false, BOAT_OBJECT_LIFE_TICKS, 1.8f);
    object_study_reset(&wb, &b, false, BOAT_OBJECT_LIFE_TICKS, 1.8f);
    Renderer3D ra = { 0 }, rb = { 0 };
    Color pa[320 * 200], pb[320 * 200];
    for (int tick = 0; tick < 3840; ++tick) {
        baseline_demo(&a, tick, NULL, NULL, &wa);
        demo_step(&b, tick, NULL, NULL, &wb);
        CHECK(boat_study_state_hash(BOAT_STUDY_HASH_SEED, &a, &wa)
            == boat_study_state_hash(BOAT_STUDY_HASH_SEED, &b, &wb));
        if (tick == 1079 || tick == 2399) {
            CHECK(boat_draw(&ra, pa, 320, 200, &a, BOAT_CAMERA_DEFAULT));
            CHECK(boat_draw(&rb, pb, 320, 200, &b, BOAT_CAMERA_DEFAULT));
            CHECK(memcmp(pa, pb, sizeof(pa)) == 0);
        }
    }
    renderer3d_destroy(&ra);
    renderer3d_destroy(&rb);
    puts("3840 tick physical/wake hashes and 1080/2400 tick CPU pixels match old driver");
    return 0;
}
