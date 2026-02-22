// Add to movement.c

#include "../basic/cJSON.h"
#include "../basic/basic.h"
#include "../basic/world_def.h"
#include "movement.h"
#include "../engine/entity.h"
#include <math.h>

// ... existing code ...

// Helper: Get closest point on line segment to a point
void closestPointOnSegment(cpVect p, cpVect a, cpVect b, cpVect *closest, float *t)
{
    cpVect ab = cpvsub(b, a);
    cpVect ap = cpvsub(p, a);
    
    float abLen = cpvlength(ab);
    if(abLen < 0.0001f) {
        *closest = a;
        *t = 0;
        return;
    }
    
    float dot = cpvdot(ap, ab);
    *t = dot / (abLen * abLen);
    
    if(*t <= 0) {
        *closest = a;
        *t = 0;
    } else if(*t >= 1) {
        *closest = b;
        *t = 1;
    } else {
        *closest = cpvadd(a, cpvmult(ab, *t));
    }
}

// Check if circle intersects line segment
bool circleSegmentIntersect(cpVect center, float radius, cpVect a, cpVect b, 
                           cpVect *hitPoint, cpVect *normal)
{
    cpVect closest;
    float t;
    closestPointOnSegment(center, a, b, &closest, &t);
    
    cpVect toCenter = cpvsub(center, closest);
    float dist = cpvlength(toCenter);
    
    if(dist < radius) {
        *hitPoint = closest;
        if(dist > 0.0001f) {
            *normal = cpvmult(toCenter, 1.0f / dist);
        } else {
            // Circle center is exactly on segment, use perpendicular
            cpVect ab = cpvsub(b, a);
            *normal = cpvnormalize(cpvperp(ab));
        }
        return true;
    }
    
    return false;
}

// Sweep circle from start to end, check collision with segment
bool sweepCircleSegment(cpVect start, cpVect end, float radius,
                       cpVect segA, cpVect segB,
                       float *outT, cpVect *outNormal)
{
    cpVect movement = cpvsub(end, start);
    float moveLen = cpvlength(movement);
    
    if(moveLen < 0.0001f) {
        // Not moving, just check if already intersecting
        cpVect hitPoint, normal;
        if(circleSegmentIntersect(start, radius, segA, segB, &hitPoint, &normal)) {
            *outT = 0;
            *outNormal = normal;
            return true;
        }
        return false;
    }
    
    cpVect moveDir = cpvmult(movement, 1.0f / moveLen);
    
    // Expand segment by radius (Minkowski sum)
    cpVect segDir = cpvsub(segB, segA);
    cpVect segPerp = cpvnormalize(cpvperp(segDir));
    
    // Check multiple samples along the segment to find collision
    float bestT = 2.0f;  // > 1 means no hit
    cpVect bestNormal = cpvzero;
    
    // Test along the path
    int steps = (int)(moveLen / radius) + 2;
    if(steps > 20) steps = 20;  // Cap for performance
    
    for(int i = 0; i <= steps; i++) {
        float t = (float)i / (float)steps;
        cpVect testPos = cpvadd(start, cpvmult(movement, t));
        
        cpVect hitPoint, normal;
        if(circleSegmentIntersect(testPos, radius, segA, segB, &hitPoint, &normal)) {
            // Binary search to find exact collision point
            float tMin = (i > 0) ? ((float)(i-1) / (float)steps) : 0;
            float tMax = t;
            
            for(int j = 0; j < 8; j++) {  // 8 iterations = good precision
                float tMid = (tMin + tMax) * 0.5f;
                cpVect midPos = cpvadd(start, cpvmult(movement, tMid));
                
                if(circleSegmentIntersect(midPos, radius, segA, segB, &hitPoint, &normal)) {
                    tMax = tMid;
                } else {
                    tMin = tMid;
                }
            }
            
            if(tMax < bestT) {
                bestT = tMax;
                // Get normal at collision point
                cpVect collisionPos = cpvadd(start, cpvmult(movement, bestT));
                circleSegmentIntersect(collisionPos, radius, segA, segB, &hitPoint, &bestNormal);
            }
            break;  // Found first collision
        }
    }
    
    if(bestT <= 1.0f) {
        *outT = bestT;
        *outNormal = bestNormal;
        return true;
    }
    
    return false;
}

// Main collision function: move circle through world
cpVect moveCircleWithCollision(cpVect start, cpVect delta, float radius, int maxBounces)
{
    if(maxBounces <= 0 || cpvlength(delta) < 0.01f) {
        return start;
    }
    
    cpVect end = cpvadd(start, delta);
    
    // Find closest collision across all wall segments
    float closestT = 2.0f;
    cpVect closestNormal = cpvzero;
    
    for(int i = 0; i < world.worldWallSize; i++) {
        worldRect_t *wall = &world.worldWallArray[i];
        
        // Convert wall rect to 4 line segments
        cpVect corners[4] = {
            cpv(wall->rect[0], wall->rect[1]),                          // Bottom-left
            cpv(wall->rect[0] + wall->rect[2], wall->rect[1]),          // Bottom-right
            cpv(wall->rect[0] + wall->rect[2], wall->rect[1] + wall->rect[3]), // Top-right
            cpv(wall->rect[0], wall->rect[1] + wall->rect[3])           // Top-left
        };
        
        for(int j = 0; j < 4; j++) {
            cpVect segA = corners[j];
            cpVect segB = corners[(j + 1) % 4];
            
            float t;
            cpVect normal;
            if(sweepCircleSegment(start, end, radius, segA, segB, &t, &normal)) {
                if(t < closestT) {
                    closestT = t;
                    closestNormal = normal;
                }
            }
        }
    }
    
    if(closestT <= 1.0f) {
        // Hit something - move to collision point
        float safeT = closestT - 0.01f;  // Back off slightly
        if(safeT < 0) safeT = 0;
        
        cpVect safePos = cpvadd(start, cpvmult(delta, safeT));
        
        // Calculate remaining movement and slide
        cpVect remaining = cpvmult(delta, 1.0f - safeT);
        
        // Remove component along normal (slide along surface)
        float dot = cpvdot(remaining, closestNormal);
        if(dot < 0) {
            remaining = cpvsub(remaining, cpvmult(closestNormal, dot));
        }
        
        // Recursively try to slide
        if(cpvlength(remaining) > 0.1f) {
            return moveCircleWithCollision(safePos, remaining, radius, maxBounces - 1);
        }
        
        return safePos;
    }
    
    // No collision - move freely
    return end;
}

// Public API: Move entity with collision
void moveEntityWithCollision(VectorEntity *vecEnt, float deltaTime)
{
    float moveX = vecEnt->dir.x * deltaTime;
    float moveY = vecEnt->dir.y * deltaTime;
    
    cpVect start = cpv(vecEnt->pos.x, vecEnt->pos.y);
    cpVect delta = cpv(moveX, moveY);
    
    cpVect newPos = moveCircleWithCollision(start, delta, 5.0f, 3);  // 5.0f radius, max 3 bounces
    
    vecEnt->pos.x = newPos.x;
    vecEnt->pos.y = newPos.y;
}