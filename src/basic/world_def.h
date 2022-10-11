#ifndef WORLDDEF_H
#define WORLDDEF_H
#include <math.h>
#include "basic.h"

// typedef struct vector2d_st
// {
//     float x, y;
// } vector2d_t;

// typedef struct rectangle_st
// {
//     float x, y, w, h;
// } rectangle_t;

typedef float vec_t;
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];

#define rad2deg(radians) ((radians) * (180.0 / M_PI))
#define deg2rad(deg) ((deg) * (M_PI/180.0))

#define vec3set(v1, v2) {(v1)[0] = (v2)[0]; (v1)[1] = (v2)[1]; (v1)[2] = (v2)[2];}
#define vec3xyz(v, x, y, z) {(v)[0] = (x); (v)[1] = (y); (v)[2] = (z);}
#define vec3dot(v1, v2) ((v1)[0]*(v2)[0] + (v1)[1]*(v2)[1] + (v1)[2]*(v2)[2])
#define vec3squared(v) vec3dot(v, v)
#define vec3length(v) sqrt(vec3dot(v, v))
#define vec3add(v3, v1, v2) {(v3)[0] = (v1)[0]+(v2)[0]; (v3)[1] = (v1)[1]+(v2)[1]; (v3)[2] = (v1)[2]+(v2)[2];}
#define vec3sub(v3, v1, v2) {(v3)[0] = (v1)[0]-(v2)[0]; (v3)[1] = (v1)[1]-(v2)[1];(v3)[2] = (v1)[2]-(v2)[2];}
#define vec3mult(v, m) {(v)[0] *= (m); (v)[1] *= (m); (v)[2] *= (m);}
#define vec3unitvec(v, temp) {temp = 1/vec3length(v); vec3mult(v, temp);}
#define vec3dist(v1, v2) sqrt( ( ( (v1)[0] - (v2)[0] ) * ( (v1)[0] -(v2)[0] ) ) \
     + ( ( (v1)[1] - (v2)[1] ) * ( (v1)[1] -(v2)[1] ) ) \
     + ( ( (v1)[2] - (v2)[2] ) * ( (v1)[2] -(v2)[2]) ) )
#define vec3setang3(v, a, b) {vec3xyz(v, sin(a)*cos(b), sin(a)*sin(b), cos(a));}
#define vec3setang2(v, a) {vec3xyz(v, cos(a), sin(a), 0);}
#define vec3getang2(v) atan2(v[1], v[0])


typedef vec_t rect2_t[4];

#define rect2set(r1, r2) {(r1)[0] = (r2)[0]; (r1)[1] = (r2)[1]; (r1)[2] = (r2)[2]; (r1)[3] = (r2)[3];}
#define rect2xy(r1, x, y) {(r1)[0] = (x); (r1)[1] = (y);}
#define rect2xywh(r1, x, y, w, h) {(r1)[0] = MIN((x), (x) + (w)); (r1)[1] = MIN((y), (y) + (h)); (r1)[2] = abs(w); (r1)[3] = abs(h); }
#define rect2centx(r) ((r)[2]/2) + (r)[0]
#define rect2centy(r) ((r)[3]/2) + (r)[1]
#define checkRectIntersect(r1, r2) ( ( ((r1[0] + r1[2]/2) - (r2[0] + r2[2]/2)) < ((r1[2] + r2[2])/2) ) \
 && ( ((r1[1] + r1[3]/2) - (r2[1] + r2[3]/2)) < ((r1[3] + r2[3])/2) ) )
#define checkVec2Intersect(r, v) ( (v[0] >= r[0]) && (v[0] <= r[0] + r[2]) ) && ( (v[1] >= r[1]) && (v[1] <= r[1] + r[3]) )

#define getIntersectRect(r3, r1, r2) { \
    if(!checkRectIntersect(r1, r2)) { \
        r3[0] = r3[1] = r3[2] = r3[3] = 0; \
    } else { \
        r3[0] = MAX(r1[0], r2[0]); \
        r3[1] = MAX(r1[1], r2[1]); \
        r3[2] = MAX(r1[0] + r1[2], r2[0] + r2[2]) = r3[0]; \
        r3[3] = MAX(r1[1] + r1[3], r2[1] + r2[3]) = r3[1]; \
     } \
}

#define getUnionRect(r3, r1, r2) { \
    r3[0] = MIN(r1[0], r2[0]); \
    r3[1] = MIN(r1[1], r2[1]); \
    r3[2] = MAX(r1[0] + r1[2], r2[0] + r2[2]) - r3[0]; \
    r3[3] = MAX(r1[1] + r1[3], r2[1] + r2[3]) - r3[1]; \
}

/********************CLIENT********************/

typedef struct camera_st
{
    rect2_t window;
} camera_t;

#endif