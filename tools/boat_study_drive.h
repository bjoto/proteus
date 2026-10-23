#ifndef BOAT_STUDY_DRIVE_H
#define BOAT_STUDY_DRIVE_H
#include "boat.h"
#include "boat_profile.h"
typedef struct {
    BoatWakeObjects world;
    Boat other;
    bool crossing;
    unsigned waypoint;
    float recovery_time;
} ObjectStudy;

void object_study_reset(ObjectStudy *, Boat *, bool, unsigned, float);
void timed_boat_step(Boat *, float, float, BoatFrameMetrics *, BoatWakePlayer *, ObjectStudy *);
void demo_step(Boat *, int, BoatFrameMetrics *, BoatWakePlayer *, ObjectStudy *);
#endif
