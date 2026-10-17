#ifndef PROTEUS_BOAT_COURSE_H
#define PROTEUS_BOAT_COURSE_H

#include "renderer3d.h"

#define BOAT_COURSE_GATES 4

typedef struct {
    Vector2 centre;
    float half_width;
} BoatCourseGate;

typedef struct {
    bool enabled;
    int next_gate; // 0: waiting at start, 1..3: running, 4: finished.
    double elapsed;
} BoatCourse;

extern const BoatCourseGate boat_course_gates[BOAT_COURSE_GATES];
void boat_course_step(BoatCourse *course, Vector2 previous, Vector2 position, double dt);
typedef void (*BoatCourseBoxDraw)(void *context, Transform3D transform, Color color);
void boat_course_visit(const BoatCourse *course, BoatCourseBoxDraw draw, void *context);
void boat_course_draw(const BoatCourse *course, Renderer3D *renderer);
void boat_course_hud(const BoatCourse *course, Vector2 position, int width);

#endif
