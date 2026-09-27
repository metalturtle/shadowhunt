#include "raycast.h"

static float intersect_segment(const float origin[2], const float direction[2],
                               const float point[2], const float segment[2])
{
    float cross = segment[0] * direction[1] - segment[1] * direction[0];
    if(cross != 0) {
        float u = ((origin[0] - point[0]) * segment[1]
                 - (origin[1] - point[1]) * segment[0]) / cross;
        float t = ((origin[0] - point[0]) * direction[1]
                 - (origin[1] - point[1]) * direction[0]) / cross;
        if(u >= 0 && u <= 1 && t >= 0 && t <= 1)
            return u;
    }
    return RAY_NO_HIT;
}

float ray_intersect_rect(const float origin[2], const float direction[2],
                         const float rect[4])
{
    float result = RAY_NO_HIT;
    float point[2] = {rect[0], rect[1]};
    float segment[2] = {0, rect[3]};

    if(origin[0] < rect[0] || origin[0] > rect[0] + rect[2]) {
        if(origin[0] > rect[0] + rect[2])
            point[0] += rect[2];
        result = intersect_segment(origin, direction, point, segment);
    }
    if(origin[1] < rect[1] || origin[1] > rect[1] + rect[3]) {
        point[0] = rect[0];
        point[1] = rect[1];
        if(origin[1] > rect[1] + rect[3])
            point[1] += rect[3];
        segment[0] = rect[2];
        segment[1] = 0;
        float candidate = intersect_segment(origin, direction, point, segment);
        if(candidate < result)
            result = candidate;
    }
    return result;
}
