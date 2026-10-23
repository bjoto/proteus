#ifndef BOAT_STUDY_DRIVE_H
#define BOAT_STUDY_DRIVE_H
#include "boat_profile.h"
#include "boat_session.h"
typedef struct {
    BoatWakeObjects wakes;
    // The scripted crossing boat keeps its original calm basin, sharing only the arcade wakes.
    // It steps at the session's clock; the multi-boat schedule belongs to multiplayer M2.
    BoatWorld other_world;
    Boat other;
    BoatPresentation other_presentation; // Drawn as its pose and cloth.
    bool crossing;
    unsigned waypoint;
    float recovery_time;
} ObjectStudy;

void object_study_reset(ObjectStudy *, BoatSession *, bool, unsigned, float);
void timed_boat_step(
    BoatSession *, float, float, BoatFrameMetrics *, BoatWakePlayer *, ObjectStudy *);
void demo_step(BoatSession *, int, BoatFrameMetrics *, BoatWakePlayer *, ObjectStudy *);
#endif
