#ifndef PROTEUS_WATERFRONT_CORNER_H
#define PROTEUS_WATERFRONT_CORNER_H

#include "renderer3d.h"

// Static city dressing: the authored cafe at OSM way 282574767 and a map-constrained
// kit distributed along streets, exterior facades, and quays. Placements are artistic.
// Corners use the same ordering as the renderer's unit cube; arrays are scene-owned.
typedef struct {
    Vector3 vertices[8];
    Color color;
    bool emissive;
    bool waterline; // Wall fittings also drawn by the software reference.
} WaterfrontCornerBox;

const WaterfrontCornerBox *waterfront_corner_boxes(size_t *count);
Vector3 waterfront_corner_position(float u, float height, float v);
bool waterfront_corner_clear(Vector3 position, float clearance);

#endif
