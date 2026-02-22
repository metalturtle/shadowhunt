#include "../basic/cJSON.h"
#include "../basic/basic.h"
#include "../basic/world_def.h"
#include "movement.h"
#include "../engine/entity.h"
#include <math.h>

#define PLAYER_AIR_ACCEL 0.1f
// cpBody *ballBody;
// cpSpace *space;
float timeStep = 1.0/60.0;

world_t world;
// moveList_t moveList;

float MINSTEP = 0.01;


void world_setup()
{
    char *fbuf;
    cJSON *levelJSON;
    cJSON *worldJSON;
    cJSON *wallListJSON;
    cJSON *wallJSON;
    int arrSize;
    // rect2_t *wallRect;
    float x, y, w, h;
    int i;
    char *loadPath;

    SDL_asprintf(&loadPath, "%s%s", SDL_GetBasePath(), "levels//level.json");
    fbuf = getFileString("levels//level.json", TEMPORARYZONE);
    if(fbuf == NULL) {
        printf("couldnt open file levels//level.json\n");
        return;
    }
    levelJSON = cJSON_Parse(fbuf);

    worldJSON = cJSON_GetObjectItemCaseSensitive(levelJSON, "collision");

    arrSize = (int)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(worldJSON, "size"));
    printf("arrSize: %d \n", arrSize);
    world.worldWallArray = (worldRect_t *) zidmalloc(PERMANENTZONE, arrSize * sizeof(worldRect_t));

    wallListJSON = cJSON_GetObjectItemCaseSensitive(worldJSON, "object");
    
    i=0;
    cJSON_ArrayForEach(wallJSON, wallListJSON)
    {
        x = cJSON_GetNumberValue(cJSON_GetArrayItem(wallJSON, 0));
        y = cJSON_GetNumberValue(cJSON_GetArrayItem(wallJSON, 1));
        w = cJSON_GetNumberValue(cJSON_GetArrayItem(wallJSON, 2));
        h = cJSON_GetNumberValue(cJSON_GetArrayItem(wallJSON, 3));

        rect2xywh(&world.worldWallArray[i].rect,x, y, w, h);
        printf("x y w h %f %f %f %f \n", x, y, w, h);
        if(w != 0 && h != 0)
            createEdgeBoundary(x, y, w, h);
        i++;
    }
    world.worldWallSize = arrSize;

    cJSON_free(levelJSON);
    zidfree(fbuf);
}

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
                           cpVect *hitPoint, cpVect *normal, bool track)
{
    cpVect closest;
    float t;
    closestPointOnSegment(center, a, b, &closest, &t);
    
    cpVect toCenter = cpvsub(center, closest);
    float dist = cpvlength(toCenter);
    
    // if(track)
    //     printf("checking dist %f %f \n", dist, radius);
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
        if(circleSegmentIntersect(start, radius, segA, segB, &hitPoint, &normal, false)) {
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
        if(circleSegmentIntersect(testPos, radius, segA, segB, &hitPoint, &normal, false)) {
            // Binary search to find exact collision point
            float tMin = (i > 0) ? ((float)(i-1) / (float)steps) : 0;
            float tMax = t;
            
            for(int j = 0; j < 8; j++) {  // 8 iterations = good precision
                float tMid = (tMin + tMax) * 0.5f;
                cpVect midPos = cpvadd(start, cpvmult(movement, tMid));
                
                if(circleSegmentIntersect(midPos, radius, segA, segB, &hitPoint, &normal, false)) {
                    tMax = tMid;
                } else {
                    tMin = tMid;
                }
            }
            
            if(tMax < bestT) {
                bestT = tMax;
                // Get normal at collision point
                cpVect collisionPos = cpvadd(start, cpvmult(movement, bestT));
                circleSegmentIntersect(collisionPos, radius, segA, segB, &hitPoint, &bestNormal, true);
                // printf("bestT %f %f,%f\n", bestT, tMin, tMax);
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
// Main collision function: move circle through world
cpVect moveCircleWithCollision(cpVect start, cpVect delta, float radius, int maxBounces)
{
    if(maxBounces <= 0) {
        return start;  // Hit recursion limit
    }
    
    float moveLen = cpvlength(delta);
    if(moveLen < 0.01f) {
        return start;  // Movement too small
    }
    
    cpVect end = cpvadd(start, delta);
    
    // Find closest collision across all wall segments
    float closestT = 2.0f;
    cpVect closestNormal = cpvzero;
    
    for(int i = 0; i < world.worldWallSize; i++) {
        worldRect_t *wall = &world.worldWallArray[i];
        
        // Skip zero-size walls
        if(wall->rect[2] <= 0.001f || wall->rect[3] <= 0.001f) {
            continue;
        }
        
        // Convert wall rect to 4 line segments
        cpVect corners[4] = {
            cpv(wall->rect[0], wall->rect[1]),                          
            cpv(wall->rect[0] + wall->rect[2], wall->rect[1]),          
            cpv(wall->rect[0] + wall->rect[2], wall->rect[1] + wall->rect[3]), 
            cpv(wall->rect[0], wall->rect[1] + wall->rect[3])           
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
        // printf("closestT %f \n", closestT);
        // CRITICAL FIX: If we're hitting at t≈0, we're stuck - don't slide
        // if(closestT < 0.05f) {  // Hit almost immediately
        //     // Move a tiny bit to avoid getting stuck
        //     // float safeT = closestT * 0.5f;
        //     float safeT = closestT * 0.1f;
        //     if(safeT < 0) safeT = 0;
        //     printf("hit almost immediately %f \n", closestT);
        //     return cpvadd(start, cpvmult(delta, safeT));
        // }
        
        // Hit something - move most of the way
        float safeT = closestT - 0.05f;  // Back off 5% of movement
        if(safeT < 0) safeT = 0;

        float epsilon = 0.01;
        cpVect counterVec = cpvmult(closestNormal, epsilon);

        // printf("safeT %f \n", safeT);
        
        cpVect safePos = cpvadd(start, cpvmult(delta, safeT));
        safePos = cpvadd(safePos, counterVec);
        
        // // Calculate remaining movement and slide
        // cpVect remaining = cpvmult(delta, 1.0f - closestT);  // Use closestT, not safeT!
        // float remainingLen = cpvlength(remaining);
        
        // // Only slide if there's meaningful movement left
        // if(remainingLen < 0.1f) {
        //     return safePos;  // Too small to matter
        // }
        
        return safePos;
        // Project remaining movement onto surface (slide)
        // float dot = cpvdot(remaining, closestNormal);
        // if(dot < 0) {
        //     remaining = cpvsub(remaining, cpvmult(closestNormal, dot));
        // }
        
        // Recursively try to slide (with reduced maxBounces)
        // return moveCircleWithCollision(safePos, remaining, radius, maxBounces - 1);
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
    
    cpVect newPos = moveCircleWithCollision(start, delta, 5.0f, 1);  // 5.0f radius, max 3 bounces
    
    vecEnt->pos.x = newPos.x;
    vecEnt->pos.y = newPos.y;
}


float signum(float x) {
    return x > 0 ? 1 : x < 0 ? -1 : 0;
}

bool checkIntersection(SDL_FRect r1, SDL_FRect r2) {
    return (fabs(r1.x + r1.w/2 - r2.x - r2.w/2) < (r1.w + r2.w)/2) && (fabs(r1.y + r1.h/2 - r2.y - r2.h/2) < (r1.h + r2.h)/2);
}

void moveCollision(VectorEntity *vecEnt, float deltaTime) {
    float minstepX=0,minstepY=0;
    bool moveAxisCheck[2] = {false, false};

    SDL_FPoint totalMoveVecArray = vecEnt->dir;
    if(totalMoveVecArray.x != 0) moveAxisCheck[0] = true;
    if(totalMoveVecArray.y != 0) moveAxisCheck[1] = true;
    float minstep,epsilon = 1;
    SDL_FRect box = vecEnt->rect;
    box.x += vecEnt->pos.x;
    box.y += vecEnt->pos.y;

    // while(moveAxisCheck[1]) {
    //     minstep = signum(totalMoveVecArray.y)*fmin(fabs(totalMoveVecArray.y),MINSTEP);
    //     box.y += minstep;
    //     for(int i = 0; i < world.worldWallSize; i++) {
    //         worldRect_t *wallrect = &world.worldWallArray[i];
    //         SDL_FRect wall;
    //         wall.x = wallrect->rect[0];
    //         wall.y = wallrect->rect[1];
    //         wall.w = wallrect->rect[2];
    //         wall.h = wallrect->rect[3];
    //         if(checkIntersection(box, wall)) {
    //             if(minstep > 0) {
    //                 minstep = fmax(minstep - ((box.y + box.h) - (wall.y)), 0);
    //             }
    //         }
    //         if(minstep < 0) {
    //             minstep = fmin(minstep + ((wall.y + wall.h) - box.y), 0);
    //         }
    //         moveAxisCheck[1] = false;
    //     }
    //     totalMoveVecArray.y -= minstep;
    //     if(fabs(totalMoveVecArray.y) < epsilon) {
    //         moveAxisCheck[1] = false;
    //     }
    //     minstepY += minstep;
    // }

    box.x = vecEnt->pos.x + box.x;
    box.y = vecEnt->pos.y + box.y;

    int counter = 0;
    while(moveAxisCheck[0]) {
        if(counter > 100) {
            com_error(ERR_FATAL, "infinite loop\n");
        }
        counter++;
        minstep = signum(totalMoveVecArray.x)*fmin(fabs(totalMoveVecArray.x),MINSTEP);
        printf("minstep: %f \n", minstep);
        box.x += minstep;

        for(int i = 0; i < world.worldWallSize; i++) {
            worldRect_t *wallrect = &world.worldWallArray[i];
            SDL_FRect wall;
            wall.x = wallrect->rect[0];
            wall.y = wallrect->rect[1];
            wall.w = wallrect->rect[2];
            wall.h = wallrect->rect[3];
            if(checkIntersection(box, wall)) {
                printf("intersection with wall %f %f %f %f \n", wall.x, wall.y, wall.w, wall.h);
                if(minstep > 0) {
                    minstep = fmax(minstep - ((box.x + box.w) - (wall.x)), 0);
                }
            }
            if(minstep < 0) {
                minstep = fmin(minstep + ((wall.x + wall.w) - box.x), 0);
            }
            moveAxisCheck[1] = false;
        }

        totalMoveVecArray.x -= minstep;
        if(fabs(totalMoveVecArray.x) < epsilon) {
            moveAxisCheck[0] = false;
        }
        minstepX += minstep;
    }
    vecEnt->pos.x += minstepX;
    vecEnt->pos.y += minstepY;
    // box.set(vecEnt->pos.x+box.x,vecEnt->pos.y+box.y,box.w,box.h);
    // ent_rect.set(ent.pos.x() + box.x() ,ent.pos.y() + box.y(),box.w(),box.h());

}

// float check_intersection(float pos[2], float dir[2], float wall[4])  {

// }

void handleMove(SDL_FPoint *pos, SDL_FPoint *dir)
{
    if(dir->x == 0 && dir->y == 0) return;
	float rpos[2];
	float rdir[3];
    

    float u = 1, temp;
    
    rpos[0] = pos->x;
    rpos[1] = pos->y;
    rdir[0] = dir->x;
    rdir[1] = dir->y;
    rdir[2] = 0;

    // int fromID = vecget(rayHandleList->emittedRayList.entIDList, rayID);

    rect2_t wall;
    for(int i = 0; i < world.worldWallSize; i++)
    {
        rect2set(wall, world.worldWallArray[i].rect);

        temp = check_intersection(rpos, rdir, wall);
        if(u > temp) {
            u = temp;
        }
    }

    vec3mult(rdir, u *1.01);

    SDL_FPoint vdist;
    vdist.x = rdir[0]; vdist.y = rdir[1];
    float dist = vec_length(&vdist);

    // printf("u val %f  %f %f,%f %f\n", u, temp, rdir[0], rdir[1], dist);


    // if(u < 1) {
    //     rdir[0] = 0;
    //     rdir[1] = 0;
    // }

    pos->x = pos->x + rdir[0];
    pos->y = pos->y + rdir[1];
    dir->x = rdir[0];
    dir->y = rdir[1];
    // vecset(rayHandleList->emittedRayList.xDirList, rayID, rdir[0]);
    // vecset(rayHandleList->emittedRayList.yDirList, rayID, rdir[1]);
}
// void updateVelocity(cpBody *body, cpVect gravity, cpFloat damping, cpFloat dt)
// {
//     // entityMove_t *entMove;

//     // entMove = (entityMove_t *) body->userData;

//     // entMove->pos[0] = body->p.x + entMove->rect[0];
//     // entMove->pos[1] = body->p.y + entMove->rect[1];
//     // cpFloat target_vx = entMove->dir[0] * entMove->speed;
//     // cpFloat target_vy = entMove->dir[1] * entMove->speed;

//     // body->v.x = target_vx;
//     // body->v.y = target_vy;
// 	// body->v.x = cpflerpconst(body->v.x, target_vx, PLAYER_AIR_ACCEL*dt*100);
// 	// body->v.y = cpflerpconst(body->v.y, target_vy, PLAYER_AIR_ACCEL*dt*100);

//     // if(body->v.x > 0)
//     //     printf("updateVel dir (%f,%f) speed %f\n", entMove->dir[0], entMove->dir[1], entMove->speed);

// }


// void physics_init()
// {
//     int initSize = 8;
//     vecinit(GENERALZONE, moveList.list, entityMove_t, initSize);
//     vecinit(GENERALZONE, moveList.bitmap, byte, initSize/8);

//     world_load();
// }


// int physics_addBody(entityMove_t *moveObj)
// {

//     int id = bm_findEmpty(moveList.bitmap.arr, vecsize(moveList.bitmap));
//     if(id < 0)
//     {
//         id = vecsize(moveList.list);
//         vecpushempty(moveList.list, entityMove_t);
//     }

//     entityMove_t *moveEnt = &vecget(moveList.list, id);
//     zmemcpy(moveEnt, moveObj, sizeof(entityMove_t));
//     bm_setBitVal(moveList.bitmap.arr, id, 1);
//     return id;
// }

// void physics_removeBody(int moveID)
// {
//     bm_setBitVal(moveList.bitmap.arr, moveID, 0);
// }

// void physics_setBody(entityMove_t *moveObj, int moveID)
// {
//     entityMove_t *moveEnt = &vecget(moveList.list, moveID);
//     zmemcpy(moveEnt, moveObj, sizeof(entityMove_t));
    
// }


// entityMove_t* physics_get(int moveID)
// {
//     entityMove_t *moveEnt = &vecget(moveList.list, moveID);
//     return moveEnt;
// }


// void move_collision(entityMove_t *moveObj, float deltaTime)
// {
//     int xMoveAxisCheck = 0, yMoveAxisCheck = 0;
//     float tempVec[3];
//     float totalMoveVec[3];
//     float entRect[4];
//     float moveRect[4];
//     float unionRect[4];
//     float wallRect[4];
//     float minstepVec[] = {0, 0};
//     float finalPos[2];


//     vec3set(tempVec, moveObj->dir);
//     vec3mult(tempVec, deltaTime);


//     vec3set(totalMoveVec, tempVec);


//     if(ABS(tempVec[0]) > 0)
//         xMoveAxisCheck = 1;
//     if(ABS(tempVec[1]) > 0)
//         yMoveAxisCheck = 1;


//     if((xMoveAxisCheck + yMoveAxisCheck) == 0)
//         return;

//     rect2set(entRect, moveObj->rect);
//     vec2add(entRect, entRect, moveObj->pos);

//     // printf("checking move rect %f %f %f %f \n", moveObj->rect[0], moveObj->rect[1], moveObj->rect[2], moveObj->rect[3]);


//     rect2set(moveRect, entRect);
//     vec2add(moveRect, moveRect, totalMoveVec);



//     getUnionRect(unionRect, moveRect, entRect);
//     rect2set(moveRect, unionRect);


//     float minstep, epsilon = 0.0001;
//     while(yMoveAxisCheck)
//     {
//         minstep = SIGNUM(totalMoveVec[1]) * MIN(ABS(totalMoveVec[1]), MINSTEP);
//         entRect[1] += minstep;


//         for(int i = 0; i < world.worldWallSize; i++)
//         {
//             rect2set(wallRect, world.worldWallArray[i].rect);

//             if(checkRectIntersect(wallRect, entRect))
//             {
//                 if(minstep > 0) {
//                     int tempm = (minstep - (entRect[1] + entRect[3]) + wallRect[1] - epsilon);
//                     minstep = MAX(tempm, 0);
//                 }
//                 if(minstep < 0) {
//                     int tempm = minstep + wallRect[1] + wallRect[3] - entRect[1] + epsilon;
//                     minstep = MIN(tempm, 0);
//                 }

//                 yMoveAxisCheck = 0;
//             }
//         }


//         totalMoveVec[1] -= minstep;
//         if(ABS(totalMoveVec[1]) < epsilon)
//             yMoveAxisCheck = 0;

//         minstepVec[1] += minstep;
//     }



//     rect2set(entRect, moveObj->rect);
//     vec2add(entRect, entRect, moveObj->pos);
//     while(xMoveAxisCheck)
//     {
//         minstep = SIGNUM(totalMoveVec[0]) * MIN(ABS(totalMoveVec[0]), MINSTEP);
//         entRect[0] += minstep;
//         for(int i = 0; i < world.worldWallSize; i++)
//         {
//             rect2set(wallRect, world.worldWallArray[i].rect);
//             if(checkRectIntersect(wallRect, entRect))
//             {
//                 if(minstep > 0)
//                 {
//                     int tempm = minstep - (entRect[0] + entRect[2]) - (wallRect[0]) - epsilon;
//                     minstep = MAX(tempm, 0);
//                 }
//                 if(minstep < 0)
//                 {
//                     int tempm = minstep + wallRect[0] + wallRect[2] - entRect[0] + epsilon;
//                     minstep = MIN(tempm, 0);
//                 }

//                 xMoveAxisCheck = 0;
//             }
//         }
    

//         totalMoveVec[0] -= minstep;
//         if(ABS(totalMoveVec[0]) < epsilon)
//             xMoveAxisCheck = 0;

//         minstepVec[0] += minstep;
//     }


//     vec2set(finalPos, moveObj->pos);
//     vec2add(finalPos, finalPos, minstepVec);

//     vec2set(moveObj->pos, finalPos);
// }


// void physics_run()
// {
//     for(int i = 0; i < vecsize(moveList.list); i++)
//     {
//         entityMove_t *moveObj = &vecget(moveList.list, i);
//         if(!bm_getBitVal(moveList.bitmap.arr, i))
//             continue;

//         move_collision(moveObj, 1);
//     }
// }

	// public Vector move_collision(EntityMovementObject moveobj,Entity ent,float delta_time) {
		// moveobj.toPoint.set(ent.vdir);
		// moveobj.set_speed(ent.get_speed());
		


// void physics_addBody(entityMove_t *entMove)
// {
//     cpBody *body;
//     cpShape *shape;

//     body = cpSpaceAddBody(space, cpBodyNew(1.0f, INFINITY));
//     body->p = cpv(entMove->pos[0], entMove->pos[1]);
//     body->velocity_func = updateVelocity;
//     body->userData = (void *) entMove;

//     shape = cpSpaceAddShape(space, cpBoxShapeNew(body, 10, 10, 0.0));
//     shape->e = 0.0f; shape->u = 0.0f;
//     shape->type = 1;
// }


// void physics_run()
// {
//     cpSpaceStep(space, timeStep);
// }

// void physics_init()
// {
//     cpVect gravity = cpv(0, -10);
//     rect2_t rect;
  
//     world_load();

//     space = cpSpaceNew();
// 	space->iterations = 10;

// 	cpBody *body, *staticBody = cpSpaceGetStaticBody(space);
// 	cpShape *shape;
	
//     for(int i = 0; i < world.worldWallSize; i++)
//     {
//         rect2set(rect, world.worldWallArray[i].rect);

//         shape = cpSpaceAddShape(space, cpSegmentShapeNew(staticBody, cpv(rect[0], rect[1]), cpv(rect[0] + rect[2], rect[1]), 0.0f));
//         shape->e = 1.0f; shape->u = 1.0f;
// 	    shape->e = 1.0f; shape->u = 1.0f;

//         shape = cpSpaceAddShape(space, cpSegmentShapeNew(staticBody, cpv(rect[0], rect[1]), cpv(rect[0], rect[1] + rect[3]), 0.0f));
//         shape->e = 1.0f; shape->u = 1.0f;
// 	    shape->e = 1.0f; shape->u = 1.0f;
        
//         shape = cpSpaceAddShape(space, cpSegmentShapeNew(staticBody, cpv(rect[0], rect[1] + rect[3]), cpv(rect[0] + rect[2], rect[1] + rect[3]), 0.0f));
//         shape->e = 1.0f; shape->u = 1.0f;
// 	    shape->e = 1.0f; shape->u = 1.0f;

//         shape = cpSpaceAddShape(space, cpSegmentShapeNew(staticBody, cpv(rect[0] + rect[2], rect[1]), cpv(rect[0] + rect[2], rect[1] + rect[3]), 0.0f));
//         shape->e = 1.0f; shape->u = 1.0f;
// 	    shape->e = 1.0f; shape->u = 1.0f;

//     }
// }

// void input_func_common(inputCommand_t *inpCmd, VectorEntity *vecEnt, int server)
// {
//     // Calculate desired movement
//     float moveX = vecEnt->dir.x * inpCmd->deltaTime;
//     float moveY = vecEnt->dir.y * inpCmd->deltaTime;
    
//     // Raycast to check collision
//     cpVect start = cpv(vecEnt->pos.x, vecEnt->pos.y);
//     cpVect end = cpv(vecEnt->pos.x + moveX, vecEnt->pos.y + moveY);
    
//     cpSegmentQueryInfo info;
//     cpShape *hit = cpSpaceSegmentQueryFirst(worldId, start, end, 5.0f, 
//                                             CP_SHAPE_FILTER_ALL, &info);
    
//     if(hit) {
//         // Hit something - slide along surface
//         cpVect normal = info.normal;
//         cpVect slide = cpv(moveX, moveY);
//         float dot = cpvdot(slide, normal);
//         slide = cpvsub(slide, cpvmult(normal, dot));
        
//         vecEnt->pos.x += slide.x;
//         vecEnt->pos.y += slide.y;
//     } else {
//         // No collision - move freely
//         vecEnt->pos.x += moveX;
//         vecEnt->pos.y += moveY;
//     }
    
//     // Update body to match (for rendering)
//     cpBodySetPosition(vecEnt->collision, cpv(vecEnt->pos.x, vecEnt->pos.y));
    
//     // NO cpSpaceStep() call!
// }

/* Stub implementation for edge boundary creation */
void createEdgeBoundary(float x, float y, float width, float height) {
    // Stub: Create physics boundary edge
    // Would typically create a static collision shape for world boundaries
    printf("createEdgeBoundary: creating boundary at (%.2f, %.2f) size %.2fx%.2f\n", x, y, width, height);
}