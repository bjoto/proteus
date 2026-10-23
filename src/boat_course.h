#ifndef PROTEUS_BOAT_COURSE_H
#define PROTEUS_BOAT_COURSE_H

#include "raylib.h"

#define BOAT_COURSE_GATES 4
// Boat position at the start of each attempt, facing the first gate.
#define BOAT_COURSE_START ((Vector2) { 0, 50 })

typedef struct {
    Vector2 centre;
    float half_width;
} BoatCourseGate;

typedef struct {
    bool enabled;
    int next_gate; // 0: waiting at start, 1..3: running, 4: finished.
    double elapsed;
    double splits[BOAT_COURSE_GATES];
    // Crossings on the most recent simulation tick, -1 when absent.
    double crossing[BOAT_COURSE_GATES];
} BoatCourse;

extern const BoatCourseGate boat_course_gates[BOAT_COURSE_GATES];
void boat_course_step(BoatCourse *course, Vector2 previous, Vector2 position, double dt);

#endif
