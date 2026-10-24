#include "boat_hud.h"
#include "raylib.h"

#include <math.h>
#include <stdlib.h>

static const Color face = { 20, 34, 39, 255 };
static const Color ink = { 218, 223, 202, 255 };
static const Color muted = { 133, 158, 153, 255 };
static const Color ahead = { 118, 196, 178, 255 };
static const Color amber = { 233, 180, 104, 255 };

typedef struct {
    int x, y, scale;
} Panel;

static float bounded(float value, float low, float high)
{
    return isfinite(value) ? fmaxf(low, fminf(high, value)) : 0;
}

static void box(Panel p, int x, int y, int w, int h, Color color)
{
    DrawRectangle(p.x + x * p.scale, p.y + y * p.scale, w * p.scale, h * p.scale, color);
}

static void line(Panel p, int x, int y, int end_x, int end_y, Color color)
{
    DrawLineEx((Vector2) { (float)(p.x + x * p.scale), (float)(p.y + y * p.scale) },
        (Vector2) { (float)(p.x + end_x * p.scale), (float)(p.y + end_y * p.scale) },
        (float)p.scale, color);
}

static void circle(Panel p, int x, int y, int radius, Color color)
{
    DrawCircle(p.x + x * p.scale, p.y + y * p.scale, (float)(radius * p.scale), color);
}

static void label(Panel p, const char *text, int x, int y, Color color)
{
    int size = 10 * p.scale;
    DrawText(text, p.x + x * p.scale - MeasureText(text, size) / 2, p.y + y * p.scale, size, color);
}

// Angle increases clockwise from twelve o'clock, matching positive starboard steering.
static void radial(Panel p, int x, int y, float angle, int inner, int outer, Color color)
{
    float radians = angle * .01745329252f;
    line(p, x + (int)roundf(sinf(radians) * inner), y - (int)roundf(cosf(radians) * inner),
        x + (int)roundf(sinf(radians) * outer), y - (int)roundf(cosf(radians) * outer), color);
}

void boat_hud_draw(BoatHudTelemetry telemetry, int width, int height)
{
    if (width < 256 || height < 180)
        return;
    int scale = width >= 1000 && height >= 720 ? 2 : 1;
    bool compact = height < 360;
    int panel_height = compact ? 40 : 76;
    Panel p = { (width - 240 * scale) / 2, height - 28 - (panel_height + 2) * scale, scale };
    float thrust = bounded(telemetry.thrust, -1, 1);
    float steering = bounded(telemetry.steering, -1, 1);
    float knots = bounded(telemetry.speed_mps, 0, 999) * 1.94384449f;
    Color thrust_color = thrust < -.005f ? amber : ahead;

    // A shallow, sealed instrument case: shadow, metal lip, enamel and flush screw heads.
    box(p, 2, 3, 238, panel_height, (Color) { 4, 11, 15, 110 });
    box(p, 0, 0, 240, panel_height, (Color) { 56, 74, 77, 245 });
    box(p, 1, 1, 238, 1, (Color) { 153, 166, 157, 255 });
    box(p, 2, 3, 236, panel_height - 6, face);
    box(p, 2, panel_height - 3, 236, 1, (Color) { 9, 19, 24, 255 });
    for (int x = 6; x <= 234; x += 228) {
        for (int y = 7; y <= panel_height - 7; y += panel_height - 14) {
            circle(p, x, y, 2, muted);
            line(p, x - 1, y + 1, x + 1, y - 1, face);
        }
    }
    line(p, 77, 12, 77, panel_height - 12, (Color) { 43, 61, 65, 255 });
    line(p, 163, 12, 163, panel_height - 12, (Color) { 43, 61, 65, 255 });

    // At the minimum 320x200 window, keep the boat visible above a shallow readout strip.
    if (compact) {
        int percent = (int)roundf(thrust * 100);
        int rudder = (int)roundf(fabsf(steering) * 100);
        label(p, "THRUST", 39, 6, muted);
        label(p, percent == 0 ? "N 0%" : TextFormat("%+d%%", percent), 39, 23,
            percent == 0 ? ink : thrust_color);
        label(p, "SPEED", 120, 6, muted);
        label(p, TextFormat("%.1f kn", knots), 120, 23, ink);
        label(p, "STEERING", 201, 6, muted);
        label(p, rudder == 0 ? "CENTRE" : TextFormat("%s %d%%", steering < 0 ? "P" : "S", rudder),
            201, 23, ink);
        return;
    }

    // Signed throttle telegraph. Neutral stays fixed; the fill grows ahead or astern.
    label(p, "THRUST", 39, 9, muted);
    box(p, 20, 36, 38, 6, (Color) { 8, 20, 25, 255 });
    int travel = (int)roundf(thrust * 18);
    box(p, travel < 0 ? 39 + travel : 39, 38, abs(travel), 2, thrust_color);
    for (int x = 21; x <= 57; x += 9)
        line(p, x, 32, x, x == 39 ? 44 : 34, muted);
    box(p, 38 + travel, 36, 3, 6, ink);
    label(p, "R", 15, 35, amber);
    label(p, "F", 65, 35, ahead);
    int percent = (int)roundf(thrust * 100);
    label(p, percent == 0 ? "N  0%" : TextFormat("%+d%%", percent), 39, 54,
        percent == 0 ? ink : thrust_color);

    // Ground-speed log: 0..60 kn sweep with a precise numeric inset below the spindle.
    circle(p, 120, 37, 34, (Color) { 108, 128, 126, 255 });
    circle(p, 120, 37, 32, (Color) { 8, 21, 26, 255 });
    circle(p, 120, 37, 30, face);
    for (int tick = 0; tick <= 12; ++tick)
        radial(p, 120, 37, -130 + tick * (260.0f / 12), tick % 2 ? 27 : 24, 29, muted);
    label(p, "0", 99, 51, muted);
    label(p, "30", 120, 11, muted);
    label(p, "60", 141, 51, muted);
    label(p, "SPEED", 120, 25, muted);
    radial(p, 120, 37, -130 + bounded(knots / 60, 0, 1) * 260, -5, 23, amber);
    circle(p, 120, 37, 3, ink);
    box(p, 105, 45, 30, 12, (Color) { 8, 21, 26, 255 });
    label(p, TextFormat("%.1f", knots), 120, 46, ink);
    label(p, "kn", 120, 59, muted);

    // Rudder indicator reads the spring-centred wheel, even when stationary or in reverse.
    label(p, "STEERING", 201, 9, muted);
    for (int tick = -2; tick <= 2; ++tick)
        radial(p, 201, 51, tick * 32.0f, 19, 23, muted);
    radial(p, 201, 51, steering * 64, 0, 21, ahead);
    circle(p, 201, 51, 3, ink);
    label(p, "P", 175, 37, muted);
    label(p, "S", 227, 37, muted);
    int rudder = (int)roundf(fabsf(steering) * 100);
    label(p, rudder == 0 ? "CENTRE" : TextFormat("%s %d%%", steering < 0 ? "P" : "S", rudder), 201,
        59, ink);
}
