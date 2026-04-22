/*
 * rotation_helper.h
 *
 *  Created on: Apr 22, 2026
 *      Author: jayakrishnan
 */

#ifndef INC_ROTATION_HELPER_H_
#define INC_ROTATION_HELPER_H_
/*
 * rotation_helper.h
 *
 * Handles coordinate transformation for independently rotated LED matrices.
 * Each matrix can be rotated 0°, 90°, 180°, or 270° independently.
 */

#include <stdint.h>

typedef struct {
    uint8_t x;
    uint8_t y;
} Coord_t;

typedef enum {
    ROTATION_0   = 0,
    ROTATION_90  = 90,
    ROTATION_180 = 180,
    ROTATION_270 = 270
} MatrixRotation_t;

/*
 * Transform logical coordinates (x, y) to physical coordinates
 * based on matrix rotation.
 *
 * Logical coords:  (0,0) = top-left, (7,7) = bottom-right
 *
 * Physical transformations:
 *   0°  :  no change
 *   90° :  (x,y) → (7-y, x)      [clockwise rotation]
 *   180°:  (x,y) → (7-x, 7-y)    [upside down]
 *   270°:  (x,y) → (y, 7-x)      [counter-clockwise]
 */
static inline Coord_t rotate_logical_to_physical(uint8_t x, uint8_t y, MatrixRotation_t rot)
{
    Coord_t phys = {x, y};

    switch (rot) {
        case ROTATION_0:
            phys.x = x;
            phys.y = y;
            break;
        case ROTATION_90:
            phys.x = (uint8_t)(7 - y);
            phys.y = x;
            break;
        case ROTATION_180:
            phys.x = (uint8_t)(7 - x);
            phys.y = (uint8_t)(7 - y);
            break;
        case ROTATION_270:
            phys.x = y;
            phys.y = (uint8_t)(7 - x);
            break;
    }
    return phys;
}

/*
 * Transform physical coordinates to logical coordinates.
 * (Inverse of above)
 */
static inline Coord_t rotate_physical_to_logical(uint8_t px, uint8_t py, MatrixRotation_t rot)
{
    Coord_t logical = {px, py};

    switch (rot) {
        case ROTATION_0:
            logical.x = px;
            logical.y = py;
            break;
        case ROTATION_90:
            logical.x = py;
            logical.y = (uint8_t)(7 - px);
            break;
        case ROTATION_180:
            logical.x = (uint8_t)(7 - px);
            logical.y = (uint8_t)(7 - py);
            break;
        case ROTATION_270:
            logical.x = (uint8_t)(7 - py);
            logical.y = px;
            break;
    }
    return logical;
}



#endif /* INC_ROTATION_HELPER_H_ */
