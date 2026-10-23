#include "boat_course.h"

#include <math.h>

// Northbound gates; offset centres encourage two broad turns across the basin.
const BoatCourseGate boat_course_gates[BOAT_COURSE_GATES]
    = { { { 0, 35 }, 12 }, { { 22, 7 }, 10 }, { { -22, -23 }, 10 }, { { 0, -53 }, 12 } };

void boat_course_step(BoatCourse *course, Vector2 previous, Vector2 position, double dt)
{
    for (int i = 0; i < BOAT_COURSE_GATES; ++i)
        course->crossing[i] = -1;
    if (!course->enabled || course->next_gate == BOAT_COURSE_GATES)
        return;
    // Swept crossings avoid missed gates at speed. Fractions account for the portions of the
    // start/finish ticks actually spent on course, independent of the frame rate.
    double running_from = course->next_gate > 0 ? 0 : 1;
    double running_until = 1;
    while (course->next_gate < BOAT_COURSE_GATES) {
        BoatCourseGate gate = boat_course_gates[course->next_gate];
        if (previous.y <= gate.centre.y || position.y > gate.centre.y)
            break;
        double fraction = (previous.y - gate.centre.y) / (previous.y - position.y);
        float x = previous.x + (position.x - previous.x) * (float)fraction;
        if (fabsf(x - gate.centre.x) > gate.half_width)
            break;
        if (course->next_gate == 0)
            running_from = fraction;
        course->crossing[course->next_gate] = fraction;
        course->splits[course->next_gate] = course->elapsed + (fraction - running_from) * dt;
        if (++course->next_gate == BOAT_COURSE_GATES)
            running_until = fraction;
    }
    course->elapsed += fmax(0, running_until - running_from) * dt;
}
