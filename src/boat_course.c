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

static Color gate_color(const BoatCourse *course, int gate)
{
    if (gate < course->next_gate)
        return (Color) { 86, 157, 150, 255 };
    if (gate == course->next_gate)
        return (Color) { 255, 216, 93, 255 };
    return gate == BOAT_COURSE_GATES - 1 ? RAYWHITE : ORANGE;
}

void boat_course_visit(const BoatCourse *course, BoatCourseBoxDraw draw, void *context)
{
    if (!course->enabled)
        return;
    for (int i = 0; i < BOAT_COURSE_GATES; ++i) {
        BoatCourseGate gate = boat_course_gates[i];
        Color color = gate_color(course, i);
        for (int side = -1; side <= 1; side += 2) {
            float x = -(gate.centre.x + side * gate.half_width);
            draw(context,
                (Transform3D) {
                    .position = { x, .6f, gate.centre.y }, .scale = { 1.2f, 1.4f, 1.2f } },
                color);
            draw(context,
                (Transform3D) { .position = { x, 1.6f, gate.centre.y }, .scale = { .2f, 1, .2f } },
                RAYWHITE);
        }
        // Floating dashes mark the crossing span; the finish has a chequered stripe.
        for (float offset = -gate.half_width; offset < gate.half_width; offset += 2) {
            Color stripe = i == BOAT_COURSE_GATES - 1 && ((int)offset % 4 == 0) ? DARKGRAY : color;
            draw(context,
                (Transform3D) {
                    .position = { -(gate.centre.x + offset + .5f), .02f, gate.centre.y },
                    .scale = { 1, .08f, .5f } },
                stripe);
        }
    }
}

static void draw_cpu_box(void *context, Transform3D transform, Color color)
{
    renderer3d_draw_box(context, transform, color);
}

void boat_course_draw(const BoatCourse *course, Renderer3D *renderer)
{
    boat_course_visit(course, draw_cpu_box, renderer);
}

void boat_course_hud(const BoatCourse *course, Vector2 position, int width)
{
    if (!course->enabled)
        return;
    int centiseconds = (int)(course->elapsed * 100);
    DrawRectangle(8, 8, 224, 62, (Color) { 14, 23, 32, 245 });
    DrawText(TextFormat("S COURSE  %02d:%02d.%02d", centiseconds / 6000, centiseconds / 100 % 60,
                 centiseconds % 100),
        16, 14, 20, RAYWHITE);
    const char *status = course->next_gate == 0  ? "Cross START to begin"
        : course->next_gate == BOAT_COURSE_GATES ? "FINISHED!"
        : course->next_gate == BOAT_COURSE_GATES - 1
        ? "Next: FINISH"
        : TextFormat("Next: buoy gate %d / 2", course->next_gate);
    DrawText(status, 16, 42, 16, (Color) { 255, 216, 93, 255 });
    int left = width - 128;
    if (left < 240)
        return;
    DrawRectangle(left, 8, 120, 148, (Color) { 14, 23, 32, 245 });
    DrawText("S COURSE", left + 8, 14, 12, RAYWHITE);
    for (int i = 0; i < BOAT_COURSE_GATES; ++i) {
        BoatCourseGate gate = boat_course_gates[i];
        int x = left + 60 + (int)gate.centre.x;
        int y = 38 + (int)(gate.centre.y + 60);
        if (i > 0) {
            Vector2 last = boat_course_gates[i - 1].centre;
            DrawLine(left + 60 + (int)last.x, 38 + (int)(last.y + 60), x, y, DARKGRAY);
        }
        DrawLine(x - (int)gate.half_width, y, x + (int)gate.half_width, y, gate_color(course, i));
        DrawText(i == 0  ? "S"
                : i == 3 ? "F"
                         : TextFormat("%d", i),
            x + 15, y - 4, 10, gate_color(course, i));
    }
    int x = left + 60 + (int)fmaxf(-54, fminf(54, position.x));
    int y = 38 + (int)fmaxf(0, fminf(112, position.y + 60));
    DrawCircle(x, y, 3, RAYWHITE);
}
