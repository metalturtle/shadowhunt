#ifndef SHADOWHUNT_RAYCAST_H
#define SHADOWHUNT_RAYCAST_H

#define RAY_NO_HIT 999.0f

/* Return the fraction along a finite direction vector where it first enters
 * an axis-aligned rectangle, or RAY_NO_HIT. */
float ray_intersect_rect(const float origin[2], const float direction[2],
                         const float rect[4]);

#endif
