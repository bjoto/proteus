#ifndef PROTEUS_BOAT_FLAG_H
#define PROTEUS_BOAT_FLAG_H

#include "renderer3d.h"

#define BOAT_FLAG_COLS 9
#define BOAT_FLAG_ROWS 6
#define BOAT_FLAG_POINTS (BOAT_FLAG_COLS * BOAT_FLAG_ROWS)
#define BOAT_FLAG_WIDTH .64f
#define BOAT_FLAG_HEIGHT .40f
#define BOAT_FLAG_STAFF_BASE_Y .82f
#define BOAT_FLAG_STAFF_HEIGHT .75f
#define BOAT_FLAG_HOIST_TOP (BOAT_FLAG_STAFF_BASE_Y + BOAT_FLAG_STAFF_HEIGHT - .04f)
#define BOAT_FLAG_STEP (1.0f / 120.0f)

typedef struct {
    Vector3 points[BOAT_FLAG_POINTS]; // World coordinates; snapshot shared by all render passes.
} BoatFlagSurface;

typedef struct {
    BoatFlagSurface surface;
    Vector3 previous[BOAT_FLAG_POINTS];
    float flutter_phase; // Integrated shedding phase; depends on apparent air speed.
    bool initialized;
} BoatFlag;

// Gentle world-space breeze in renderer coordinates (metres/second), independent of water flow.
Vector3 boat_flag_wind(Vector3 position, float time);
Vector3 boat_flag_attachment(Transform3D hull, int row);
// Fixed 120 Hz cloth integration. Hull scale is one; attachments are in physical metres.
void boat_flag_reset(BoatFlag *flag, Transform3D hull, Vector3 velocity, float time);
void boat_flag_step(BoatFlag *flag, Transform3D hull, Vector3 velocity, float time);
// Two-sided Swedish cross geometry; returned view borrows the snapshot and static topology.
Mesh3D boat_flag_mesh(const BoatFlagSurface *surface);

#endif
