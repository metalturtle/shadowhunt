#include "raycast.h"
#include <math.h>
#include <stdio.h>

static int failures;

static void expect_near(const char *name, float actual, float expected)
{
    if(fabsf(actual - expected) > 0.00001f) {
        fprintf(stderr, "%s: expected %g, got %g\n", name, expected, actual);
        failures++;
    }
}

int main(void)
{
    const float wall[4] = {10, 10, 10, 10};
    const float left[2] = {0, 15}, right[2] = {30, 15};
    const float towardRight[2] = {40, 0}, towardLeft[2] = {-40, 0};
    const float miss[2] = {0, 5};

    expect_near("left entry", ray_intersect_rect(left, towardRight, wall), 0.25f);
    expect_near("right entry", ray_intersect_rect(right, towardLeft, wall), 0.25f);
    expect_near("wall occlusion miss", ray_intersect_rect(miss, towardRight, wall), RAY_NO_HIT);

    const float nearWall[4] = {10, 10, 5, 10};
    const float farTarget[4] = {30, 10, 5, 10};
    float wallHit = ray_intersect_rect(left, towardRight, nearWall);
    float targetHit = ray_intersect_rect(left, towardRight, farTarget);
    if(!(wallHit < targetHit)) {
        fprintf(stderr, "wall should occlude the farther target\n");
        failures++;
    }

    if(failures)
        return 1;
    puts("Ray geometry checks passed.");
    return 0;
}
