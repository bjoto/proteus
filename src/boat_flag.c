#include "boat_flag.h"
#include "raymath.h"

#include <math.h>

// Cross boundaries are actual mesh edges: 5:2:9 horizontally, 4:2:4 vertically.
static const float columns[BOAT_FLAG_COLS]
    = { 0, .15625f, .3125f, .4375f, .55f, .6625f, .775f, .8875f, 1 };
static const float width = BOAT_FLAG_WIDTH, height = BOAT_FLAG_HEIGHT;

Vector3 boat_flag_wind(Vector3 p, float t)
{
    float gust = 1 + .18f * sinf(.73f * t + .013f * p.z) + .10f * sinf(1.61f * t - .021f * p.x);
    // Small advected eddies give a sheet aligned with the mean wind a varying angle of attack.
    float eddy = 8.3f * t - 2.1f * p.x - .7f * p.z;
    return (Vector3) { .35f * gust + .035f * sinf(eddy), .015f * sinf(eddy * 1.37f),
        .10f * gust + .04f * sinf(eddy * .83f + p.y * 2) };
}

Vector3 boat_flag_attachment(Transform3D hull, int row)
{
    // Eleven-degree aft rake; hoist spacing follows distance along the staff.
    float y = BOAT_FLAG_HOIST_TOP - .9805807f * height * row / (BOAT_FLAG_ROWS - 1);
    Vector3 local = { .69f, y, 1.30f + .2f * (y - BOAT_FLAG_STAFF_BASE_Y) };
    return Vector3Add(hull.position, Vector3Transform(local, MatrixRotateXYZ(hull.rotation)));
}

void boat_flag_reset(BoatFlag *flag, Transform3D hull, Vector3 velocity, float time)
{
    Vector3 air = Vector3Subtract(boat_flag_wind(hull.position, time), velocity);
    // Begin hanging at low speed, including the initial frame at spawn/reset.
    float speed = Vector3Length(air);
    Vector3 direction = Vector3Normalize((Vector3) { air.x, -9.81f / fmaxf(1, speed), air.z });
    if (Vector3LengthSqr(direction) < .1f)
        direction = (Vector3) { 1, 0, 0 };
    for (int y = 0; y < BOAT_FLAG_ROWS; ++y)
        for (int x = 0; x < BOAT_FLAG_COLS; ++x) {
            int i = y * BOAT_FLAG_COLS + x;
            Vector3 p = Vector3Add(
                boat_flag_attachment(hull, y), Vector3Scale(direction, columns[x] * width));
            flag->surface.points[i] = p;
            flag->previous[i] = Vector3Subtract(p, Vector3Scale(velocity, BOAT_FLAG_STEP));
        }
    flag->flutter_phase = 0;
    flag->initialized = true;
}

static void constrain(BoatFlag *flag, int a, int b, float rest, float stiffness)
{
    float wa = a % BOAT_FLAG_COLS != 0, wb = b % BOAT_FLAG_COLS != 0;
    if (wa + wb == 0)
        return;
    Vector3 delta = Vector3Subtract(flag->surface.points[b], flag->surface.points[a]);
    float length = Vector3Length(delta);
    if (length < .00001f)
        return;
    delta = Vector3Scale(delta, stiffness * (length - rest) / (length * (wa + wb)));
    flag->surface.points[a] = Vector3Add(flag->surface.points[a], Vector3Scale(delta, wa));
    flag->surface.points[b] = Vector3Subtract(flag->surface.points[b], Vector3Scale(delta, wb));
}

void boat_flag_step(BoatFlag *flag, Transform3D hull, Vector3 velocity, float time)
{
    if (!flag->initialized
        || Vector3Distance(flag->surface.points[0], boat_flag_attachment(hull, 0)) > 2)
        boat_flag_reset(flag, hull, velocity, time);
    float apparent_speed = Vector3Length(
        Vector3Subtract(boat_flag_wind(boat_flag_attachment(hull, 0), time), velocity));
    flag->flutter_phase = remainderf(
        flag->flutter_phase + 2 * PI * (1.7f + .32f * apparent_speed) * BOAT_FLAG_STEP, 2 * PI);
    Vector3 acceleration[BOAT_FLAG_POINTS];
    for (int y = 0; y < BOAT_FLAG_ROWS; ++y)
        for (int x = 0; x < BOAT_FLAG_COLS; ++x) {
            int i = y * BOAT_FLAG_COLS + x;
            Vector3 p = flag->surface.points[i];
            Vector3 v = Vector3Scale(Vector3Subtract(p, flag->previous[i]), 1 / BOAT_FLAG_STEP);
            Vector3 air = Vector3Subtract(boat_flag_wind(p, time), v);
            Vector3 across = Vector3Subtract(
                flag->surface.points[y * BOAT_FLAG_COLS + (x < BOAT_FLAG_COLS - 1 ? x + 1 : x)],
                flag->surface.points[y * BOAT_FLAG_COLS + (x > 0 ? x - 1 : x)]);
            Vector3 down = Vector3Subtract(
                flag->surface.points[(y < BOAT_FLAG_ROWS - 1 ? y + 1 : y) * BOAT_FLAG_COLS + x],
                flag->surface.points[(y > 0 ? y - 1 : y) * BOAT_FLAG_COLS + x]);
            Vector3 normal = Vector3Normalize(Vector3CrossProduct(across, down));
            // Unresolved wake turbulence acts as air pressure, not prescribed vertex motion.
            // Convect from hoist to fly; stronger apparent flow sheds faster, stronger eddies.
            float eddy = .18f * apparent_speed * columns[x]
                * sinf(flag->flutter_phase - 9 * columns[x] + .8f * y);
            float pressure = Vector3DotProduct(air, normal) + eddy;
            // Thin cloth: strong normal pressure, small skin drag, gravity. Acceleration
            // is bounded for hard contacts; constraints carry tension back to the hoist.
            Vector3 force = Vector3Add(Vector3Scale(normal, 2.8f * pressure * fabsf(pressure)),
                Vector3Scale(air, .16f * Vector3Length(air)));
            force.y -= 9.81f;
            float magnitude = Vector3Length(force);
            acceleration[i] = Vector3Scale(force, magnitude > 180 ? 180 / magnitude : 1);
        }
    for (int i = 0; i < BOAT_FLAG_POINTS; ++i) {
        Vector3 p = flag->surface.points[i];
        flag->surface.points[i] = Vector3Add(p,
            Vector3Add(Vector3Scale(Vector3Subtract(p, flag->previous[i]), .997f),
                Vector3Scale(acceleration[i], BOAT_FLAG_STEP * BOAT_FLAG_STEP)));
        flag->previous[i] = p;
    }
    for (int pass = 0; pass < 8; ++pass) {
        for (int y = 0; y < BOAT_FLAG_ROWS; ++y)
            flag->surface.points[y * BOAT_FLAG_COLS] = boat_flag_attachment(hull, y);
        for (int y = 0; y < BOAT_FLAG_ROWS; ++y)
            for (int x = 0; x < BOAT_FLAG_COLS; ++x) {
                int a = y * BOAT_FLAG_COLS + x;
                float dy = height / (BOAT_FLAG_ROWS - 1);
                if (x + 1 < BOAT_FLAG_COLS) {
                    float dx = width * (columns[x + 1] - columns[x]);
                    constrain(flag, a, a + 1, dx, 1);
                    if (y + 1 < BOAT_FLAG_ROWS) {
                        constrain(flag, a, a + BOAT_FLAG_COLS + 1, hypotf(dx, dy), .03f);
                        constrain(flag, a + 1, a + BOAT_FLAG_COLS, hypotf(dx, dy), .03f);
                    }
                }
                if (y + 1 < BOAT_FLAG_ROWS)
                    constrain(flag, a, a + BOAT_FLAG_COLS, dy, 1);
                // Soft bend resistance allows folds without making a hinged metal plate.
                if (x + 2 < BOAT_FLAG_COLS)
                    constrain(flag, a, a + 2, width * (columns[x + 2] - columns[x]), .0005f);
                if (y + 2 < BOAT_FLAG_ROWS)
                    constrain(flag, a, a + 2 * BOAT_FLAG_COLS, 2 * dy, .0005f);
                // Long-range, stretch-only tether prevents accumulated extension at speed.
                Vector3 delta = Vector3Subtract(
                    flag->surface.points[a], flag->surface.points[y * BOAT_FLAG_COLS]);
                float length = Vector3Length(delta), reach = width * columns[x];
                if (length > reach && x > 0)
                    flag->surface.points[a] = Vector3Add(flag->surface.points[y * BOAT_FLAG_COLS],
                        Vector3Scale(delta, reach / length));
            }
    }
}

Mesh3D boat_flag_mesh(const BoatFlagSurface *surface)
{
    static Triangle3D triangles[(BOAT_FLAG_COLS - 1) * (BOAT_FLAG_ROWS - 1) * 4];
    static bool initialized;
    if (!initialized) {
        int count = 0;
        for (int y = 0; y < BOAT_FLAG_ROWS - 1; ++y)
            for (int x = 0; x < BOAT_FLAG_COLS - 1; ++x) {
                unsigned int a = y * BOAT_FLAG_COLS + x, b = a + 1;
                unsigned int c = b + BOAT_FLAG_COLS, d = a + BOAT_FLAG_COLS;
                Color color = x == 2 || y == 2 ? (Color) { 255, 205, 0, 255 }
                                               : (Color) { 0, 106, 167, 255 };
                triangles[count++] = (Triangle3D) { { a, b, c }, color };
                triangles[count++] = (Triangle3D) { { a, c, d }, color };
                triangles[count++] = (Triangle3D) { { c, b, a }, color };
                triangles[count++] = (Triangle3D) { { d, c, a }, color };
            }
        initialized = true;
    }
    return (Mesh3D) { surface->points, triangles, sizeof(triangles) / sizeof(triangles[0]) };
}
