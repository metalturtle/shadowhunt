#include "../basic/basic.h"
#include "../basic/world_def.h"
#include "movement.h"
#include "../engine/entity.h"

void ent_initRayRenderList()
{
    int initSize = ENT_INITSIZE;
    vecinit(GENERALZONE, renderRayList.xList, float, initSize);
    vecinit(GENERALZONE, renderRayList.yList, float, initSize);
    vecinit(GENERALZONE, renderRayList.xDirList, float, initSize);
    vecinit(GENERALZONE, renderRayList.yDirList, float, initSize);
    vecinit(GENERALZONE, renderRayList.endTimeList, unsigned long int, initSize);
}

void ent_initRayHandleList(rayHandleList_t *rayHandleList)
{
    int initSize = ENT_INITSIZE;
    
    emittedRayList_t *emittedRayList = &rayHandleList->emittedRayList;
    rayEntityList_t *rayEntityList = &rayHandleList->rayEntityList;
    rayHitList_t *rayHitList = &rayHandleList->rayHitList;

    vecinit(GENERALZONE, emittedRayList->entIDList, int, initSize);
    vecinit(GENERALZONE, emittedRayList->xList, float, initSize);
    vecinit(GENERALZONE, emittedRayList->yList, float, initSize);
    vecinit(GENERALZONE, emittedRayList->xDirList, float, initSize);
    vecinit(GENERALZONE, emittedRayList->yDirList, float, initSize);


    vecinit(GENERALZONE, rayEntityList->entIDList, int, initSize);
    vecinit(GENERALZONE, rayEntityList->entList, int, initSize);
    vecinit(GENERALZONE, rayEntityList->entBitmap, byte, initSize/8);


    vecinit(GENERALZONE, rayHitList->fromList, int, initSize);
    vecinit(GENERALZONE, rayHitList->toList, int, initSize);
    vecinit(GENERALZONE, rayHitList->uList, float, initSize);
}


int ent_addRayEntity(rayHandleList_t *rayHandleList, int entID )
{
    int id = bm_findEmpty(rayHandleList->rayEntityList.entBitmap.arr,
         vecsize(rayHandleList->rayEntityList.entBitmap));
    if(id < 0)
    {
        id = vecsize(rayHandleList->rayEntityList.entList);
        vecpushempty(rayHandleList->rayEntityList.entList, int);
        vecpushempty(rayHandleList->rayEntityList.entIDList, int);
    }

    int rayEntID = vecget(rayHandleList->rayEntityList.entList, id);
    // vec3set(entMove->pos, setMove->pos);
    // rect2set(entMove->rect, setMove->rect);
    // vec3xyz(entMove->dir, 0, 0, 0);
    vecset(rayHandleList->rayEntityList.entIDList, id, rayEntID);

    bm_setBitVal(rayHandleList->rayEntityList.entBitmap.arr, id, 1);
    
    return id;
}

void ent_removeRayEntity(rayHandleList_t *rayHandleList, int entID)
{
    bm_setBitVal(rayHandleList->rayEntityList.entBitmap.arr, entID, 0);
}

int ent_emitRay(rayHandleList_t *rayHandleList, int entID, float pos[2], float dir[2])
{
    int id = vecsize(rayHandleList->emittedRayList.xList);
    vecpush(rayHandleList->emittedRayList.xList, float, pos[0]);
    vecpush(rayHandleList->emittedRayList.yList, float, pos[1]);
    vecpush(rayHandleList->emittedRayList.xDirList, float, dir[0]);
    vecpush(rayHandleList->emittedRayList.yDirList, float, dir[1]);
    vecpush(rayHandleList->emittedRayList.entIDList, int, entID);

    return id;
}

void ent_resetRayList(rayHandleList_t *rayHandleList)
{
    vecreset(rayHandleList->emittedRayList.xList);
    vecreset(rayHandleList->emittedRayList.yList);
    vecreset(rayHandleList->emittedRayList.xDirList);
    vecreset(rayHandleList->emittedRayList.yDirList);
    vecreset(rayHandleList->emittedRayList.entIDList);
}

void ent_setHitEntity(rayHandleList_t *rayHandleList, int rayID, int fromID, int toID)
{
    vecpush(rayHandleList->rayHitList.fromList, int, fromID);
    vecpush(rayHandleList->rayHitList.toList, int, toID);
}

void ent_resetHitEntityList(rayHandleList_t *rayHandleList)
{
    vecreset(rayHandleList->rayHitList.fromList);
    vecreset(rayHandleList->rayHitList.toList);   
}

float ray_intersect(float pos[2],float dir[2],float p1[2],float p2[2]) {
		
    float x = 0,y = 0,u=999,t = 999;
    float p[2][2];

    vec2set(p[0], p1);
    vec2set(p[1], p2);


    float r_cr_x =p[1][0] * dir[1] - p[1][1] * dir[0];
    if(r_cr_x != 0)
    {
        u = ( pos[0] - p[0][0] )*p[1][1] - ( pos[1] - p[0][1] )*p[1][0];
        t = ( pos[0] - p[0][0] )*dir[1] - ( pos[1] - p[0][1] )*dir[0];
        u = u/r_cr_x;
        t = t/r_cr_x;
        if ( (0 <= u && u <= 1) && (0 <= t && t <= 1) )
        {
        }
        else {
            u = 999;
        }
    
    }
    return u;
}


float check_intersection(float pos[2], float dir[2], float wall[4]) 
{ 
    float u = 999;
    float stat_p[2][2];
    // float staticNormalAngle = 0;
    float calc;

    if(pos[0] < wall[0])
    {
        vec2set(stat_p[0], wall);
        vec2xy(stat_p[1], 0, wall[3]);

        calc = ray_intersect(pos,dir,stat_p[0],stat_p[1]);
        u = MIN(calc, u);
        // staticNormalAngle=180;
    } else if(pos[0] > wall[0] + wall[2])
    {
        vec2xy(stat_p[0], wall[0] + wall[2], wall[1]);
        vec2xy(stat_p[1], 0, wall[3]);

        calc = ray_intersect(pos,dir,stat_p[0],stat_p[1]);
        u = MIN(calc, u);
        // staticNormalAngle=0;
    }
    if(pos[1] < wall[1])
    {
        vec2xy(stat_p[0], wall[0], wall[1]);
        vec2xy(stat_p[1], wall[2], 0);


        calc = ray_intersect(pos,dir,stat_p[0],stat_p[1]);
        u = MIN(calc, u);
        // staticNormalAngle=-90;
    }
    else if (pos[1] > wall[1] + wall[3])
    {
        vec2xy(stat_p[0], wall[0], wall[1] + wall[3]);
        vec2xy(stat_p[1], wall[2], 0);


        calc = ray_intersect(pos,dir,stat_p[0],stat_p[1]);
        u = MIN(calc, u);
        // staticNormalAngle=90;
    }
    return u;	
}


void handle_ray(rayHandleList_t *rayHandleList, int rayID)
{
float rpos[2];
float rdir[3];


float u = 1, temp;

rpos[0] = vecget(rayHandleList->emittedRayList.xList, rayID);
rpos[1] = vecget(rayHandleList->emittedRayList.yList, rayID);
rdir[0] = vecget(rayHandleList->emittedRayList.xDirList, rayID);
rdir[1] = vecget(rayHandleList->emittedRayList.yDirList, rayID);
rdir[2] = 0;

int fromID = vecget(rayHandleList->emittedRayList.entIDList, rayID);

rect2_t wall;
for(int i = 0; i < world.worldWallSize; i++)
{
    rect2set(wall, world.worldWallArray[i].rect);

    temp = check_intersection(rpos, rdir, wall);
    if(u > temp) {
        u = temp;
    }
}


for(int i = 0; i < vecsize(rayHandleList->rayEntityList.entList); i++)
{
    if(!bm_getBitVal(rayHandleList->rayEntityList.entBitmap.arr, i))
        continue;


    int toID = vecget(rayHandleList->rayEntityList.entIDList, i);

    if(fromID == toID)
        continue;

    int rayEntID = vecget(rayHandleList->rayEntityList.entList, i);
    VectorEntity *vecEnt = &vectorEntityList[rayEntID];
    // rect2set(wall, vecEnt->rect);
    wall[0] = vecEnt->rect.x; wall[1] = vecEnt->rect.y; wall[2] = vecEnt->rect.w; wall[3] = vecEnt->rect.h;
    // vec2add(wall, wall, vecEnt->pos);
    wall[0] += vecEnt->pos.x;
    wall[1] += vecEnt->pos.y;


    temp = check_intersection(rpos, rdir, wall);
    if(u > temp) {
        u = temp;

        ent_setHitEntity(rayHandleList, rayID, fromID, toID);
    }
}

vec3mult(rdir, u);

printf("u val %f  %f %f,%f \n", u, temp, rdir[0], rdir[1]);

vecset(rayHandleList->emittedRayList.xDirList, rayID, rdir[0]);
vecset(rayHandleList->emittedRayList.yDirList, rayID, rdir[1]);
}


void handle_ray_hits()
{
rayHandleList_t *rayHandleList = &rayWeaponHandle.rayHandleList;
int isServer = cvar_getInt("isServer");
if(!isServer)
    return;

for(int i = 0; i < vecsize(rayHandleList->rayHitList.fromList); i++)
{
    int fromID = vecget(rayHandleList->rayHitList.fromList, i);
    int toID = vecget(rayHandleList->rayHitList.toList, i);
    

    // int health = vecget(vectorEntityList.healthList, toID);
    int health = vectorEntityList[toID].health;

    if(health > 0) {
        health -= 30;
        // vecset(vectorEntityList.healthList, toID, health);
        vectorEntityList[toID].health = health;
        // ent_setStateFlags(VECTOR_SERIALIZER, toID, 3, 1);
        printf("setting health state flag \n");
    }
    

    // animatedSprite_t *sprite = &vecget(vectorEntityList.animSpriteList, toID);
    // sprite->rect[2] /= 2;
    // sprite->rect[3] /= 2;
    printf("hit entities: %d %d %d \n", fromID, toID, health);
}
}


void handle_ray_list(rayHandleList_t *rayHandleList)
{
for(int i = 0; i < vecsize(rayHandleList->emittedRayList.xList); i++)
{
    handle_ray(rayHandleList, i);
}
}


void add_ray_to_render()
{
float pos[2];
float dir[2];
for(int i = 0; i < vecsize(rayWeaponHandle.rayHandleList.emittedRayList.xList); i++)
{
    pos[0] = vecget(rayWeaponHandle.rayHandleList.emittedRayList.xList, i);
    pos[1] = vecget(rayWeaponHandle.rayHandleList.emittedRayList.yList, i);

    dir[0] = vecget(rayWeaponHandle.rayHandleList.emittedRayList.xDirList, i);
    dir[1] = vecget(rayWeaponHandle.rayHandleList.emittedRayList.yDirList, i);

    vecpush(renderRayList.xList, float,pos[0]);
    vecpush(renderRayList.yList, float, pos[1]);
    vecpush(renderRayList.xDirList, float, dir[0]);
    vecpush(renderRayList.yDirList, float, dir[1]);
}
}