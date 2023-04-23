#include "chipmunk/chipmunk_private.h"
#include "../lib/cJSON/cJSON.h"
#include "../basic/basic.h"
#include "movement.h"

#define PLAYER_AIR_ACCEL 0.1f
cpBody *ballBody;
cpSpace *space;
float timeStep = 1.0/60.0;

world_t world;

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

        rect2xywh(world.worldWallArray[i].rect,x, y, w, h);
        i++;
    }
    world.worldWallSize = arrSize;

    cJSON_free(levelJSON);
    zidfree(fbuf);
}

void updateVelocity(cpBody *body, cpVect gravity, cpFloat damping, cpFloat dt)
{
    entityMove_t *entMove;

    entMove = (entityMove_t *) body->userData;

    entMove->pos[0] = body->p.x + entMove->rect[0];
    entMove->pos[1] = body->p.y + entMove->rect[1];
    cpFloat target_vx = entMove->dir[0] * entMove->speed;
    cpFloat target_vy = entMove->dir[1] * entMove->speed;

    body->v.x = target_vx;
    body->v.y = target_vy;
	// body->v.x = cpflerpconst(body->v.x, target_vx, PLAYER_AIR_ACCEL*dt*100);
	// body->v.y = cpflerpconst(body->v.y, target_vy, PLAYER_AIR_ACCEL*dt*100);

    // if(body->v.x > 0)
    //     printf("updateVel dir (%f,%f) speed %f\n", entMove->dir[0], entMove->dir[1], entMove->speed);

}


void physics_addBody(entityMove_t *entMove)
{
    cpBody *body;
    cpShape *shape;

    body = cpSpaceAddBody(space, cpBodyNew(1.0f, INFINITY));
    body->p = cpv(entMove->pos[0], entMove->pos[1]);
    body->velocity_func = updateVelocity;
    body->userData = (void *) entMove;

    shape = cpSpaceAddShape(space, cpBoxShapeNew(body, 10, 10, 0.0));
    shape->e = 0.0f; shape->u = 0.0f;
    shape->type = 1;
}


void physics_run()
{
    cpSpaceStep(space, timeStep);
}

void physics_init()
{
    cpVect gravity = cpv(0, -10);
    rect2_t rect;
  
    world_load();

    space = cpSpaceNew();
	space->iterations = 10;

	cpBody *body, *staticBody = cpSpaceGetStaticBody(space);
	cpShape *shape;
	
    for(int i = 0; i < world.worldWallSize; i++)
    {
        rect2set(rect, world.worldWallArray[i].rect);

        shape = cpSpaceAddShape(space, cpSegmentShapeNew(staticBody, cpv(rect[0], rect[1]), cpv(rect[0] + rect[2], rect[1]), 0.0f));
        shape->e = 1.0f; shape->u = 1.0f;
	    shape->e = 1.0f; shape->u = 1.0f;

        shape = cpSpaceAddShape(space, cpSegmentShapeNew(staticBody, cpv(rect[0], rect[1]), cpv(rect[0], rect[1] + rect[3]), 0.0f));
        shape->e = 1.0f; shape->u = 1.0f;
	    shape->e = 1.0f; shape->u = 1.0f;
        
        shape = cpSpaceAddShape(space, cpSegmentShapeNew(staticBody, cpv(rect[0], rect[1] + rect[3]), cpv(rect[0] + rect[2], rect[1] + rect[3]), 0.0f));
        shape->e = 1.0f; shape->u = 1.0f;
	    shape->e = 1.0f; shape->u = 1.0f;

        shape = cpSpaceAddShape(space, cpSegmentShapeNew(staticBody, cpv(rect[0] + rect[2], rect[1]), cpv(rect[0] + rect[2], rect[1] + rect[3]), 0.0f));
        shape->e = 1.0f; shape->u = 1.0f;
	    shape->e = 1.0f; shape->u = 1.0f;

    }
}