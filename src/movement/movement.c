// #include "chipmunk/chipmunk_private.h"
#include "../basic/cJSON.h"
#include "../basic/basic.h"
#include "movement.h"

#define PLAYER_AIR_ACCEL 0.1f
// cpBody *ballBody;
// cpSpace *space;
float timeStep = 1.0/60.0;

world_t world;
moveList_t moveList;

float MINSTEP = 0.01;


void world_load()
{
    char *fbuf;
    cJSON *levelJSON;
    cJSON *worldJSON;
    cJSON *wallListJSON;
    cJSON *wallJSON;
    int arrSize;
    rect2_t *wallRect;
    float x, y, w, h;
    int i;

    fbuf = getFileString("levels//level.json", TEMPORARYZONE);
    levelJSON = cJSON_Parse(fbuf);

    worldJSON = cJSON_GetObjectItemCaseSensitive(levelJSON, "collision");

    arrSize = (int)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(worldJSON, "size"));
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
        i++;
    }
    world.worldWallSize = arrSize;

    cJSON_free(levelJSON);
    zidfree(fbuf);
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


void physics_init()
{
    int initSize = 8;
    vecinit(GENERALZONE, moveList.list, entityMove_t, initSize);
    vecinit(GENERALZONE, moveList.bitmap, byte, initSize/8);

    world_load();
}


int physics_addBody(entityMove_t *moveObj)
{

    int id = bm_findEmpty(moveList.bitmap.arr, vecsize(moveList.bitmap));
    if(id < 0)
    {
        id = vecsize(moveList.list);
        vecpushempty(moveList.list, entityMove_t);
    }

    entityMove_t *moveEnt = &vecget(moveList.list, id);
    zmemcpy(moveEnt, moveObj, sizeof(entityMove_t));
    bm_setBitVal(moveList.bitmap.arr, id, 1);
    return id;
}

void physics_removeBody(int moveID)
{
    bm_setBitVal(moveList.bitmap.arr, moveID, 0);
}

void physics_setBody(entityMove_t *moveObj, int moveID)
{
    entityMove_t *moveEnt = &vecget(moveList.list, moveID);
    zmemcpy(moveEnt, moveObj, sizeof(entityMove_t));
    
}


entityMove_t* physics_get(int moveID)
{
    entityMove_t *moveEnt = &vecget(moveList.list, moveID);
    return moveEnt;
}


void move_collision(entityMove_t *moveObj, float deltaTime)
{
    int xMoveAxisCheck = 0, yMoveAxisCheck = 0;
    float tempVec[3];
    float totalMoveVec[3];
    float entRect[4];
    float moveRect[4];
    float unionRect[4];
    float wallRect[4];
    float minstepVec[] = {0, 0};
    float finalPos[2];


    vec3set(tempVec, moveObj->dir);
    vec3mult(tempVec, deltaTime);


    vec3set(totalMoveVec, tempVec);


    if(ABS(tempVec[0]) > 0)
        xMoveAxisCheck = 1;
    if(ABS(tempVec[1]) > 0)
        yMoveAxisCheck = 1;


    if((xMoveAxisCheck + yMoveAxisCheck) == 0)
        return;

    rect2set(entRect, moveObj->rect);
    vec2add(entRect, entRect, moveObj->pos);

    // printf("checking move rect %f %f %f %f \n", moveObj->rect[0], moveObj->rect[1], moveObj->rect[2], moveObj->rect[3]);


    rect2set(moveRect, entRect);
    vec2add(moveRect, moveRect, totalMoveVec);



    getUnionRect(unionRect, moveRect, entRect);
    rect2set(moveRect, unionRect);


    float minstep, epsilon = 0.0001;
    while(yMoveAxisCheck)
    {
        minstep = SIGNUM(totalMoveVec[1]) * MIN(ABS(totalMoveVec[1]), MINSTEP);
        entRect[1] += minstep;


        for(int i = 0; i < world.worldWallSize; i++)
        {
            rect2set(wallRect, world.worldWallArray[i].rect);

            if(checkRectIntersect(wallRect, entRect))
            {
                if(minstep > 0) {
                    int tempm = (minstep - (entRect[1] + entRect[3]) + wallRect[1] - epsilon);
                    minstep = MAX(tempm, 0);
                }
                if(minstep < 0) {
                    int tempm = minstep + wallRect[1] + wallRect[3] - entRect[1] + epsilon;
                    minstep = MIN(tempm, 0);
                }

                yMoveAxisCheck = 0;
            }
        }


        totalMoveVec[1] -= minstep;
        if(ABS(totalMoveVec[1]) < epsilon)
            yMoveAxisCheck = 0;

        minstepVec[1] += minstep;
    }



    rect2set(entRect, moveObj->rect);
    vec2add(entRect, entRect, moveObj->pos);
    while(xMoveAxisCheck)
    {
        minstep = SIGNUM(totalMoveVec[0]) * MIN(ABS(totalMoveVec[0]), MINSTEP);
        entRect[0] += minstep;
        for(int i = 0; i < world.worldWallSize; i++)
        {
            rect2set(wallRect, world.worldWallArray[i].rect);
            if(checkRectIntersect(wallRect, entRect))
            {
                if(minstep > 0)
                {
                    int tempm = minstep - (entRect[0] + entRect[2]) - (wallRect[0]) - epsilon;
                    minstep = MAX(tempm, 0);
                }
                if(minstep < 0)
                {
                    int tempm = minstep + wallRect[0] + wallRect[2] - entRect[0] + epsilon;
                    minstep = MIN(tempm, 0);
                }

                xMoveAxisCheck = 0;
            }
        }
    

        totalMoveVec[0] -= minstep;
        if(ABS(totalMoveVec[0]) < epsilon)
            xMoveAxisCheck = 0;

        minstepVec[0] += minstep;
    }


    vec2set(finalPos, moveObj->pos);
    vec2add(finalPos, finalPos, minstepVec);

    vec2set(moveObj->pos, finalPos);
}


void physics_run()
{
    for(int i = 0; i < vecsize(moveList.list); i++)
    {
        entityMove_t *moveObj = &vecget(moveList.list, i);
        if(!bm_getBitVal(moveList.bitmap.arr, i))
            continue;

        move_collision(moveObj, 1);
    }
}

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