#ifndef PROTEUS_WATERFRONT_CORNER_H
#define PROTEUS_WATERFRONT_CORNER_H

#include "renderer3d.h"

// Static, authored dressing at the northwest corner of OSM way 282574767.
// Corners use the same ordering as the renderer's unit cube; arrays are scene-owned.
typedef struct {
    Vector3 vertices[8];
    Color color;
    bool emissive;
} WaterfrontCornerBox;

const WaterfrontCornerBox *waterfront_corner_boxes(size_t *count);
Vector3 waterfront_corner_position(float u, float height, float v);
bool waterfront_corner_clear(Vector3 position, float clearance);

#endif
