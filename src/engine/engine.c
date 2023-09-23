#include "../basic/basic.h"
#include "../basic/world_def.h"
#include "engine.h"
#include "entity.h"
#include "../movement/movement.h"

camera_t worldCamera;
cl_inputList_t cl_inputList;

vectorEntity_t mainEntity;
float P_X = 300,P_Y = 300;

byte keyMap[256];

struct inputCmdConfig_st inpCmdConfig;
pickupList_t healthPickupList;

#define ENTCHILD_SIZE 3
#define ENTCHILD_SERIALIZE 0
#define ENTCHILD_SPRITE 1
#define ENTCHILD_MOVE 2

#define VECTOR_SERIALIZER 0

#define VECENT_SPEED 1


int MAIN_ENT_ID = -1;

void inpConfig_storeUsedKeys(char *usedKeys, int usedKeyLen)
{
    memset(inpCmdConfig.usedKeyMap, -1, 256);

    for(int i = 0; i < usedKeyLen; i++)
    {
        inpCmdConfig.usedKeyMap[usedKeys[i]] = i;
    }

    inpCmdConfig.keyBitLen = usedKeyLen;
    inpCmdConfig.keyByteLen = (byte) CEIL(((float) usedKeyLen)/8.0);
    printf("calculating total key bytes %d \n", inpCmdConfig.keyByteLen);
}


void inpCmd_init(inputCommandList_t *inpCmdList)
{
    int totalKeyLen;
    void *ptr;

    totalKeyLen = inpCmdConfig.keyByteLen * INPCMD_MAX_SIZE;

    ptr =  zidmalloc(GENERALZONE, totalKeyLen);


    for(int i = 0; i < INPCMD_MAX_SIZE; i++)
    {
        inpCmdList->inpCmdArr[i].key = ptr;
        ptr += inpCmdConfig.keyByteLen;
    }

    inpCmdList->start = 0;
    inpCmdList->end = 0;
    inpCmdList->lastRecordID = 0;

}


void inpCmd_clearPressed()
{
    inpCmdConfig.pressedLen = 0;
}


void inpCmd_moveMouse(float x, float y)
{
    inpCmdConfig.mouseX = x;
    inpCmdConfig.mouseY = y;
}

void inpCmd_pressKey(char key)
{

    if(inpCmdConfig.pressedLen == 255)
        return;

    
    if(key >= 'A' && key <= 'Z')
        key = (key - 'A') + 'a';

    if(inpCmdConfig.usedKeyMap[key] < 0)
        return;

    inpCmdConfig.keysPressed[inpCmdConfig.pressedLen] = key;
    inpCmdConfig.pressedLen++;
}

qbool inpCmd_isEmpty(inputCommandList_t *inpCmdList)
{
    if(inpCmdList->start == inpCmdList->end)
        return qtrue;
    return qfalse;
}

int inpCmd_getLen(inputCommandList_t *inpCmdList)
{
    return inpCmdList->end - inpCmdList->start;
}

qbool inpCmd_isFull(inputCommandList_t *inpCmdList)
{
    if((inpCmdList->end - inpCmdList->start) == INPCMD_MAX_SIZE)
    {
        return qtrue;
    }
    return qfalse;
}

inputCommand_t *inpCmd_getLast(inputCommandList_t *inpCmdList)
{
    int lastid = (INPCMD_MAX_SIZE + inpCmdList->end - 1) & (INPCMD_MAX_SIZE - 1);
    return &inpCmdList->inpCmdArr[lastid];
}

qbool inpCmd_isPressed(inputCommand_t *inpCmd, char key)
{
    if(bm_getBitVal(inpCmd->key, inpCmdConfig.usedKeyMap[key])) {
        return qtrue;
    }
    return qfalse;
}

void inpCmd_addFromInput(inputCommandList_t *inpCmdList, int sequence)
{
    int ind = (inpCmdList->end & (INPCMD_MAX_SIZE - 1));

    inputCommand_t *inpCmd = &inpCmdList->inpCmdArr[ind];

    for(int i = 0; i < inpCmdConfig.keyByteLen; i++)
    {
        inpCmd->key[i] = 0;
    }

    for(int i = 0; i <inpCmdConfig.pressedLen; i++)
    {
        char key = inpCmdConfig.keysPressed[i];
        bm_setBitVal(inpCmd->key, inpCmdConfig.usedKeyMap[key], 1);
    }

    inpCmd->mouseX = inpCmdConfig.mouseX;
    inpCmd->mouseY = inpCmdConfig.mouseY;


    inpCmd->sequence = sequence;
    inpCmd->recordID = ++inpCmdList->lastRecordID;

    inpCmdList->end++;
}

void inpCmd_removeFirst(inputCommandList_t *inpCmd)
{
    if(inpCmd_isEmpty(inpCmd))
        return;
    
    inpCmd->start++;
}

inputCommand_t *inpCmd_get(inputCommandList_t *inpCmd, int i)
{
    return &inpCmd->inpCmdArr[(inpCmd->start + i) & (INPCMD_MAX_SIZE - 1)];
}

inputCommand_t *inpCmd_add(inputCommandList_t *inpCmdList, int sequence)
{
    int ind = (inpCmdList->end & (INPCMD_MAX_SIZE - 1));

    inputCommand_t *inpCmd = &inpCmdList->inpCmdArr[ind];

    inpCmd->sequence = sequence;
    inpCmd->recordID = ++inpCmdList->lastRecordID;

    inpCmdList->end++; 

    return inpCmd;
}

void inpCmd_clear(inputCommandList_t *inpCmdList)
{
    inpCmdList->start = inpCmdList->end = 0;
}


void inpCmd_free(inputCommandList_t *inpCmdList)
{

    zidfree(inpCmdList->inpCmdArr[0].key);

    for(int i = 0; i < INPCMD_MAX_SIZE; i++)
    {
        inpCmdList->inpCmdArr[i].key = NULL;
    }

    inpCmdList->start = 0;
    inpCmdList->end = 0;
    inpCmdList->lastRecordID = 0;
}





///////////////////////////////////////////////////////////////////////

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
        float staticNormalAngle = 0;
        float calc;

        if(pos[0] < wall[0])
		{
            vec2set(stat_p[0], wall);
            vec2xy(stat_p[1], 0, wall[3]);

            calc = ray_intersect(pos,dir,stat_p[0],stat_p[1]);
			u = MIN(calc, u);
			staticNormalAngle=180;
		}
		else if(pos[0] > wall[0] + wall[2])
		{
            vec2xy(stat_p[0], wall[0] + wall[2], wall[1]);
            vec2xy(stat_p[1], 0, wall[3]);

            calc = ray_intersect(pos,dir,stat_p[0],stat_p[1]);
			u = MIN(calc, u);
			staticNormalAngle=0;
		}
		if(pos[1] < wall[1])
		{
            vec2xy(stat_p[0], wall[0], wall[1]);
            vec2xy(stat_p[1], wall[2], 0);


            calc = ray_intersect(pos,dir,stat_p[0],stat_p[1]);
			u = MIN(calc, u);
			staticNormalAngle=-90;
		}
		else if (pos[1] > wall[1] + wall[3])
		{
            vec2xy(stat_p[0], wall[0], wall[1] + wall[3]);
            vec2xy(stat_p[1], wall[2], 0);


            calc = ray_intersect(pos,dir,stat_p[0],stat_p[1]);
			u = MIN(calc, u);
			staticNormalAngle=90;
		}
		return u;	
	}


void handle_ray(rayHandleList_t *rayHandleList, int rayID)
{
	float rpos[2];
	float rdir[2];
    

    float u = 1, temp;
    
    rpos[0] = vecget(rayHandleList->emittedRayList.xList, rayID);
    rpos[1] = vecget(rayHandleList->emittedRayList.yList, rayID);
    rdir[0] = vecget(rayHandleList->emittedRayList.xDirList, rayID);
    rdir[1] = vecget(rayHandleList->emittedRayList.yDirList, rayID);

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

        entityMove_t *moveEnt = &vecget(rayHandleList->rayEntityList.entList, i);
        rect2set(wall, moveEnt->rect);
        vec2add(wall, wall, moveEnt->pos);


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
        

        int health = vecget(vectorEntityList.healthList, toID);

        if(health > 0) {
            health -= 30;
            vecset(vectorEntityList.healthList, toID, health);
            ent_setStateFlags(VECTOR_SERIALIZER, toID, 3, 1);
            printf("setting health state flag \n");
        }
        

        // animatedSprite_t *sprite = &vecget(vectorEntityList.animSpriteList, toID);
        // sprite->rect[2] /= 2;
        // sprite->rect[3] /= 2;
        printf("hit entities: %d %d %f \n", fromID, toID, health);
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

void input_func_common(int entID, inputCommandList_t *inpCmdList, entityMove_t *moveEnt)
{
    inputCommand_t *inpCmd;
    int inpLen;
    byte up, down, left, right, shoot;
    float vec3[3];
    float temp;
    float speed = VECENT_SPEED;
    float mouseXY[3];
    float angle;
    float dir[3];


    inpLen = inpCmd_getLen(inpCmdList);


    for(int i = 0; i < inpLen;i++)
    {
        inpCmd = inpCmd_get(inpCmdList, i);


        up = bm_getBitVal(inpCmd->key, 0);
        down = bm_getBitVal(inpCmd->key, 1);
        left = bm_getBitVal(inpCmd->key, 2);
        right = bm_getBitVal(inpCmd->key, 3);
        shoot = bm_getBitVal(inpCmd->key, 4);

        vec3xyz(vec3,
            right - left
            ,up - down
            ,0);

    
        vec3unitvec(vec3, temp);
        vec3mult(vec3, speed);
        vec3add(moveEnt->dir, moveEnt->dir, vec3);

        moveEnt->speed = 0.2f;


        if(shoot)
        {
            endTimer_t *shootTimer = &vecget(vectorEntityList.shootTimerList, entID);

            mouseXY[0] = inpCmd->mouseX - 0.5;
            mouseXY[1] = inpCmd->mouseY - 0.5;
            angle = vec3getang2(mouseXY);


            weaponOnHand_t *weaponOnHand = &vecget(vectorEntityList.weaponOnHandList, entID);
            ent_handleRayWeaponShoot(&rayWeaponHandle, entID, weaponOnHand, moveEnt->pos, angle);
        }
    }
}


void input_func_server()
{
    inputCommandList_t *inpCmdList;

    for(int i = 0; i < vecsize(server.clRepList); i++)
    {
        if(!bm_getBitVal(server.clRepBitMap.arr, i))
            continue;

        
        inpCmdList = &vecget(cl_inputList.list, i);

        int entID = i2imap_get(vectorEntityList.mainEntMap, i);
        entityMove_t *moveEnt = &vecget(vectorEntityList.movableList, entID);
        input_func_common(entID, inpCmdList, moveEnt);
    }
}


void input_func_client()
{
    inputCommandList_t *inpCmdList;
    inpCmdList = &vecget(cl_inputList.list, 0);
    
    entityMove_t *moveEnt = &vecget(vectorEntityList.movableList, MAIN_ENT_ID);
    input_func_common(MAIN_ENT_ID, inpCmdList, moveEnt);
    printf("input func common \n");
}


void set_physics_movement()
{
    for(int e = 0; e < vecsize(vectorEntityList.movableList); e++)
    {
        if(!bm_getBitVal(vectorEntityList.bitmap.arr, e))
            continue;


        entityMove_t *moveEnt = &vecget(vectorEntityList.movableList, e);

        // printf("checking movement %p\n", moveEnt->move.dir);
        // if(moveEnt->move.dir[0] != 0 || moveEnt->move.dir[1] != 0)
            // printf("setting some movement %p %f %f\n", moveEnt->move.dir, moveEnt->move.dir[0], moveEnt->move.dir[1]);
        

        int moveID = vecget(vectorEntityList.moveIDList, e);
        physics_setBody(moveEnt, moveID);
    } 
}

void sprite_func()
{
    for(int e = 0; e < vecsize(vectorEntityList.movableList); e++)
    {
        if(!bm_getBitVal(vectorEntityList.bitmap.arr, e))
            continue;

        entityMove_t *moveEnt = &vecget(vectorEntityList.movableList, e);
        animatedSprite_t *entSprite = &vecget(vectorEntityList.animSpriteList, e);

        vec3set(entSprite->pos, moveEnt->pos);   
    }
}


void set_sprite_angle(int entID, int conID)
{
    inputCommandList_t *inpCmdList;
    int inpLen;
    inputCommand_t *inpCmd;
    float mouseXY[2];
    float angle;
    byte shoot;
    endTimer_t *shootTimer;
 

    animatedSprite_t *entSprite = &vecget(vectorEntityList.animSpriteList, entID);

    inpCmdList = &vecget(cl_inputList.list, conID);
    inpLen = inpCmd_getLen(inpCmdList);

    if(inpLen == 0)
        return;


    inpCmd = inpCmd_get(inpCmdList, inpLen - 1);
    

    mouseXY[0] = inpCmd->mouseX - 0.5;
    mouseXY[1] = inpCmd->mouseY - 0.5;
    angle = vec3getang2(mouseXY);


    entSprite->angle = angle;

    int isServer = cvar_getInt("isServer");
    if(isServer)
    {
        ent_setStateFlags(VECTOR_SERIALIZER, entID, 2, 1);
    }
}


void set_sprite_angle_server()
{
    for(int i = 0; i < vecsize(server.clRepList); i++)
    {
        if(!bm_getBitVal(server.clRepBitMap.arr, i))
            continue;

        int entID = i2imap_get(vectorEntityList.mainEntMap, i);
        set_sprite_angle(entID, i);
    }
}

void check_pickup_collided()
{
    for(int e = 0; e < vecsize(vectorEntityList.movableList); e++)
    {
        if(!bm_getBitVal(vectorEntityList.bitmap.arr, e))
            continue;

        entityMove_t entMove = vecget(vectorEntityList.movableList, e);
        entVec_t entPos = vecget(vectorEntityList.posList, e);
        vec2add(entMove.rect, entMove.rect, entPos.pos);

        for(int p = 0; p < vecsize(healthPickupList.posList); p++)
        {
            if(!bm_getBitVal(healthPickupList.bitmap.arr, p))
                continue;

            entVec_t pickupPos = vecget(healthPickupList.posList, p);
            entRect_t pickupRect = vecget(healthPickupList.rectList, p);
            vec2add(pickupRect.rect, pickupRect.rect, pickupPos.pos);

            if(checkRectIntersect(pickupRect.rect, entMove.rect))
            {
                printf("entity picked up health: %d\n", p);
                vecset(vectorEntityList.healthList, e, 100);
                ent_setStateFlags(VECTOR_SERIALIZER, e, 3, 1);
                ent_removePickup(&healthPickupList,p);
            }
        }
    }
}

void check_move_func()
{
    float zeroVec[] = {0, 0, 0};
    for(int i = 0; i < vecsize(vectorEntityList.movableList); i++)
    {
        if(!bm_getBitVal(vectorEntityList.bitmap.arr, i))
            continue;
    

        entityMove_t *moveEnt = &vecget(vectorEntityList.movableList, i);
        entVec_t entPos = vecget(vectorEntityList.posList, i);

        float xdiff = moveEnt->pos[0] - entPos.pos[0];
        float ydiff = moveEnt->pos[1] - entPos.pos[1];


        if(ABS(xdiff) > 0)
        {
            // printf("entity moved x %d \n", ABS(xdiff));
            ent_setStateFlags(VECTOR_SERIALIZER, i, 1, 1);
        }
        if(ABS(ydiff) > 0)
        {
            // printf("entity moved y %d \n", ABS(ydiff));
            ent_setStateFlags(VECTOR_SERIALIZER, i, 0, 1);
        }
    }
}

void set_physics_to_move()
{
    entityMove_t *phyMove;
    for(int i = 0; i < vecsize(vectorEntityList.movableList); i++)
    {
        if(!bm_getBitVal(vectorEntityList.bitmap.arr, i))
            continue;

        
        entityMove_t *moveEnt = &vecget(vectorEntityList.movableList, i);
        int moveID = vecget(vectorEntityList.moveIDList, i);
        phyMove = physics_get(moveID);

        zmemcpy(moveEnt, phyMove, sizeof(entityMove_t));
    }
}

void set_pos_from_move()
{
    float zeroVec[] = {0, 0, 0};
    for(int i = 0; i < vecsize(vectorEntityList.movableList); i++)
    {
        if(!bm_getBitVal(vectorEntityList.bitmap.arr, i))
            continue;
    

        entVec_t *entPos = &vecget(vectorEntityList.posList, i);
        entityMove_t *moveEnt = &vecget(vectorEntityList.movableList, i);
        // entitySprite_t *entSprite = &vecget(vectorEntityList.spriteList, i);

        animatedSprite_t *entSprite = &vecget(vectorEntityList.animSpriteList, i);

        weaponOnHand_t *weaponOnHand = &vecget(vectorEntityList.weaponOnHandList, i);

        // printf("entSprite: %f,%f \n", entSprite->pos[0], entSprite->pos[1]);

        // int rayEntID = vecget(vectorEntityList.rayEntIDList, i);

        // entityMove_t *rayEnt = &vecget(rayEntityList.entList, rayEntID);


        // if(moveEnt->move.dir[0] == 0 && moveEnt->move.dir[1] == 0 && moveEnt->move.dir[2] == 0)
        //     continue;

        float dist = vec3dist(entPos->pos, moveEnt->pos);

    

        vec3set(entPos->pos, moveEnt->pos);
        vec3set(moveEnt->dir, zeroVec);

        vec3set(entSprite->pos, moveEnt->pos);

        // vec3set(rayEnt->pos, moveEnt->pos);
        ent_setRayWeaponEntity(&rayWeaponHandle, weaponOnHand, moveEnt);
    }
}


void interpolate_pos()
{
    long simTime = getTimeMillis();
    simTime -= 200;
    for(int e = 0; e < vecsize(vectorEntityList.movableList); e++)
    {
        entVec_t *entPos = &vecget(vectorEntityList.posList, e);
        positionInterpolate_t *posIntp = &vecget(vectorEntityList.posInterpolateList, e);
        angleInterpolate_t *angIntp = &vecget(vectorEntityList.angleInterpolateList, e);
        animatedSprite_t *sprite = &vecget(vectorEntityList.animSpriteList, e);


        if(e == MAIN_ENT_ID)
            continue;


        if(posIntp->last < 3)
            continue;
        

        int lastPos = -1;
        int nextPos = -1;
        for(int j = 0; j < 3; j++)
        {
            long curTime = posIntp->timestamp[(posIntp->last + j) % 3];
            if(curTime > simTime)
            {
                if(j > 0) {
                    lastPos = (posIntp->last + j + 3 - 1) % 3;
                    nextPos = (posIntp->last + j) % 3;
                }

                break;
            }
        }


        if(lastPos == -1)
            continue;


        if(posIntp->timestamp[nextPos] == posIntp->timestamp[lastPos])
        {
            printf("zero difference betwen timestamp %d %d %d\n", lastPos, nextPos, posIntp->timestamp[nextPos]);
            continue;
        }

        
        float prevX = posIntp->pos[lastPos][0];
        float prevY = posIntp->pos[lastPos][1];
        float nextX = posIntp->pos[nextPos][0];
        float nextY = posIntp->pos[nextPos][1];

        float a = ((float)(posIntp->timestamp[nextPos] - simTime))
            /func_absFloat((float)(posIntp->timestamp[nextPos] - posIntp->timestamp[lastPos]));


        float intpVec[3];
        intpVec[0] = (prevX * a) + (nextX * (1 - a));
        intpVec[1] = (prevY * a) + (nextY * (1 - a));

        // float correctVec[3];
        // float temp;

        // float dist = vec2dist(moveEnt->pos, intpVec);
        // if(dist > 2)
        // if(1)
        // {
        //     printf("dist great %f x: %f-%f, y: %f-%f\n", dist);
        //     vec3sub(correctVec, intpVec, moveEnt->pos);
        //     vec3unitvec(correctVec, temp);

        //     temp = dist * 0.1667;

        //     vec3mult(correctVec, temp);
        //     vec3add(correctVec, correctVec, moveEnt->pos);
            
        //     moveEnt->pos[0] = correctVec[0];
        //     moveEnt->pos[1] = correctVec[1];
        // }
        // else {
        //     moveEnt->pos[0] = intpVec[0];
        //     moveEnt->pos[1] = intpVec[1];
        // }


        entPos->pos[0] = intpVec[0];
        entPos->pos[1] = intpVec[1];
    }
}


void interpolate_angle()
{
    long simTime = getTimeMillis();
    simTime -= 200;
    for(int e = 0; e < vecsize(vectorEntityList.movableList); e++)
    {
        angleInterpolate_t *angIntp = &vecget(vectorEntityList.angleInterpolateList, e);
        animatedSprite_t *sprite = &vecget(vectorEntityList.animSpriteList, e);


        if(e == MAIN_ENT_ID)
            continue;


        if(angIntp->last < 3)
            continue;
        

        int lastPos = -1;
        int nextPos = -1;
        for(int j = 0; j < 3; j++)
        {
            long curTime = angIntp->timestamp[(angIntp->last + j) % 3];
            if(curTime > simTime)
            {
                if(j > 0) {
                    lastPos = (angIntp->last + j + 3 - 1) % 3;
                    nextPos = (angIntp->last + j) % 3;
                }

                break;
            }
        }


        if(lastPos == -1)
            continue;


        if(angIntp->timestamp[nextPos] == angIntp->timestamp[lastPos])
        {
            printf("zero difference betwen timestamp %d %d %d\n", lastPos, nextPos, angIntp->timestamp[nextPos]);
            continue;
        }


        float nextAng = rad2deg(angIntp->angle[nextPos]);
        float lastAng = rad2deg(angIntp->angle[lastPos]);


        if(nextAng < 0) nextAng = 360 + nextAng;
        if(lastAng < 0) lastAng = 360 + lastAng;


        float a = ((float)(angIntp->timestamp[nextPos] - simTime))
            /func_absFloat((float)(angIntp->timestamp[nextPos] - angIntp->timestamp[lastPos]));

        
        float diff = nextAng - lastAng;
        int sig = SIGNUM(diff);

        
        float diff2 = 360 - ABS(diff);
        if(ABS(diff2) < ABS(diff)) {
            diff = -1 * sig * diff2;
        }
        else {
            diff2 = diff;
        }
        diff = diff2;
        

        float angToSet = lastAng + diff * (1 - a);
        sprite->angle = deg2rad(angToSet);
    }
}


void run_move_func()
{
    float temp;
    float vec3[3];

    for(int e = 0; e < vecsize(vectorEntityList.movableList); e++)
    {
        entityMove_t *moveEnt = &vecget(vectorEntityList.movableList, e);
    
        if(!bm_getBitVal(vectorEntityList.bitmap.arr, e))
            continue;

        
        vec3set(vec3, moveEnt->dir);

        // if(vec3[0] == 0 && vec3[1] == 0 && vec3[2] == 0)
        //     continue;
        
        // vec3unitvec(vec3, temp);
        // vec3mult(vec3, moveEnt->move.speed);
        // // printf("vec3 %f %f %f\n", vec3[0], vec3[1], vec3[2]);
        // vec3add(moveEnt->move.pos, moveEnt->move.pos, vec3);
        // vec3set(moveEnt->move.dir, zeroVec);

        vec3add(moveEnt->pos, moveEnt->pos, moveEnt->dir);


        // printf("move dir %f %f %f \n", moveEnt->move.dir[0], moveEnt->move.dir[1], moveEnt->move.dir[2]);
        // printf("move pos %f %f %f \n", moveEnt->move.pos[0], moveEnt->move.pos[1], moveEnt->move.pos[2]);
    }
}

void add_pickup_sprite()
{
    entitySprite_t pickupSprite;
    pickupSprite.texID = 0;
    for(int i = 0; i < vecsize(healthPickupList.posList); i++)
    {
        if(!bm_getBitVal(healthPickupList.bitmap.arr, i))
            continue;
        
        entVec_t pickupPos = vecget(healthPickupList.posList, i);
        entRect_t pickupRect = vecget(healthPickupList.rectList, i);

        vec3set(pickupSprite.pos, pickupPos.pos);
        rect2set(pickupSprite.rect, pickupRect.rect);
    
        vecpush(entSpriteList.renderList, entitySprite_t, pickupSprite);
    }
}

void add_sprite_for_render()
{
    for(int e = 0; e < vecsize(vectorEntityList.animSpriteList); e++)
    {
        animatedSprite_t *sprite = &vecget(vectorEntityList.animSpriteList, e);
    
        if(!bm_getBitVal(vectorEntityList.bitmap.arr, e))
            continue;
        

        vecpush(animSpriteList.renderList, animatedSprite_t, (*sprite));
    }
}

void set_move_from_pos()
{
    for(int e = 0; e < vecsize(vectorEntityList.movableList); e++)
    {
        if(!bm_getBitVal(vectorEntityList.bitmap.arr, e))
            continue;


        entVec_t *entPos = &vecget(vectorEntityList.posList, e);
        entityMove_t *moveEnt = &vecget(vectorEntityList.movableList, e);

        vec3set(moveEnt->pos, entPos->pos);
    }
}

void scan_killed_entities()
{

    int health;
    for(int e = 0; e < vecsize(vectorEntityList.movableList); e++)
    {
        if(!bm_getBitVal(vectorEntityList.bitmap.arr, e))
            continue;


        health = vecget(vectorEntityList.healthList, e);

        if(health < 0)
        {
            animatedSprite_t *sprite = &vecget(vectorEntityList.animSpriteList, e);
            sprite->rect[2] /= 2;
            sprite->rect[3] /= 2;
            // vecset(vectorEntityList.healthList, e, 100);
            // ent_removeVectorEntity(e);
            // serv_removeSyncedEnt(e, VECTOR_SERIALIZER);

            ent_addKillID(e);

            printf("entity id %d \n", e);
        }
    }  
}

void kill_vector_entities()
{
    for(int i = 0; i < vecsize(killIDList.entIDList); i++)
    {
        int entID = vecget(killIDList.entIDList, i);
        ent_removeVectorEntity(entID);
        serv_removeSyncedEnt(entID, VECTOR_SERIALIZER);
        printf("killed vector entity\n");
    }
}

void set_actual_to_baseline()
{
    if(MAIN_ENT_ID == -1)  return;


    entVec_t *entPos = &vecget(vectorEntityList.posList, MAIN_ENT_ID);
    entityMove_t *moveEnt = &vecget(vectorEntityList.movableList, MAIN_ENT_ID);
    animatedSprite_t *sprite = &vecget(vectorEntityList.animSpriteList, MAIN_ENT_ID);

    vec3set(mainEntity.entPos.pos, entPos->pos);
    // rect2set(mainEntity.movable.rect, moveEnt->rect);


    vec3set(mainEntity.movable.pos, moveEnt->pos);
    rect2set(mainEntity.movable.rect, moveEnt->rect);


    vec3set(mainEntity.sprite.pos, entPos->pos);
    rect2set(mainEntity.sprite.rect, moveEnt->rect);
}

void set_baseline_to_actual()
{
    if(MAIN_ENT_ID == -1) return;


    entVec_t *entPos = &vecget(vectorEntityList.posList, MAIN_ENT_ID);
    entityMove_t *moveEnt = &vecget(vectorEntityList.movableList, MAIN_ENT_ID);
    animatedSprite_t *sprite = &vecget(vectorEntityList.animSpriteList, MAIN_ENT_ID);


    vec3set(entPos->pos, mainEntity.entPos.pos);

    
    vec3set(moveEnt->pos, mainEntity.movable.pos);
    rect2set(moveEnt->rect, mainEntity.movable.rect);
    
    vec3set(sprite->pos, mainEntity.sprite.pos);
}

void eng_processServerEntities()
{
    kill_vector_entities();

    ent_resetKillList();

    check_pickup_collided();

    set_move_from_pos();


    input_func_server();


    set_physics_movement();


    physics_run();


    set_physics_to_move();


    check_move_func();


    set_pos_from_move();
    

    handle_ray_list(&rayWeaponHandle.rayHandleList);

    handle_ray_hits();

    ent_resetHitEntityList(&rayWeaponHandle.rayHandleList);

    scan_killed_entities();

    set_sprite_angle_server();

    // ent_resetRayList();

    ent_resetRayWeapon(&rayWeaponHandle);

    // if(!checkTimer(&server.sendTimer)) {
    //     return;
    // }

    // movableEntity_t *moveEnt = &vecget(vectorEntityList.movableList, 0);
    // printf("entity pos %f %f \n", moveEnt->pos[0], moveEnt->pos[1]);
}


void eng_processClientEntities()
{

    set_actual_to_baseline();

    
    set_move_from_pos();

    printf("before input func client\n");
    input_func_client();
    printf("after input func client \n");


    set_physics_movement();


    physics_run();


    set_physics_to_move();


    set_pos_from_move();


    interpolate_pos();


    interpolate_angle();


    if(MAIN_ENT_ID != -1) {
        float *pos = (vecget(vectorEntityList.movableList, MAIN_ENT_ID)).pos;
        float diff[] = {-30, -30};
        rect2xywh(worldCamera.window, diff[0], diff[1], getScreenWidth(), getScreenHeight());
        vec2add(worldCamera.window, worldCamera.window, pos);

        set_sprite_angle(MAIN_ENT_ID, 0);
    }


    handle_ray_list(&rayWeaponHandle.rayHandleList);


    handle_ray_hits();


    ent_resetHitEntityList(&rayWeaponHandle.rayHandleList);


    add_sprite_for_render();


    add_pickup_sprite();


    add_ray_to_render();


    ent_resetRayWeapon(&rayWeaponHandle);

}


int writeState(int entID, bitstream_t *bs, byte *bm, int conID)
{
    entVec_t *entPos = &vecget(vectorEntityList.posList, entID);
    animatedSprite_t *sprite = &vecget(vectorEntityList.animSpriteList, entID);

    int health = vecget(vectorEntityList.healthList, entID);

    int xval = (int)(entPos->pos[0] * 100);
    int yval = (int)(entPos->pos[1] * 100);
    int ang = (int)(sprite->angle * 100);

    for(int i = 0; i < 4; i++)
    {
        if(bm_getBitVal(bm, i))
        {
            switch(i)
            {
                case 0:
                    stream_writeInt(bs, yval);
                    break;
                case 1:
                    stream_writeInt(bs, xval);
                    break;
                case 2:
                    stream_writeInt(bs, ang);
                    break;
                case 3:
                    printf("writing health %d %d \n", health, conID);
                    stream_writeInt(bs, health);
                    break;
            }
        }
    }

    return 0;
}



int readState(int entID, bitstream_t *bs, byte *bm)
{
    int xval, yval, iang;
    int changePosFlag = 0;
    int changeAngleFlag = 0;
    int health;

    xval = yval = 0;
    iang = 0;
  

    entVec_t *entPos = &vecget(vectorEntityList.posList, entID);
    positionInterpolate_t *posIntp = &vecget(vectorEntityList.posInterpolateList, entID);
    angleInterpolate_t *angIntp = &vecget(vectorEntityList.angleInterpolateList, entID);
    animatedSprite_t *sprite = &vecget(vectorEntityList.animSpriteList, entID);

    float x = entPos->pos[0], y = entPos->pos[1];
    float ang = sprite->angle;


    for(int i = 0; i < 4; i++)
    {
        if(bm_getBitVal(bm, i))
        {
            switch(i)
            {
                case 0:
                    // printf("pressed yval\n");
                    yval = stream_readInt(bs);
                    y = ((float)yval)/100.0;
                    changePosFlag = 1;
                    P_Y = entPos->pos[1];
                    break;
                case 1:
                    // printf("pressed xval\n");
                    xval = stream_readInt(bs);
                    x = ((float)xval)/100.0;
                    changePosFlag = 1;
                    P_X = entPos->pos[0];
                    break;
                case 2:
                    iang = stream_readInt(bs);
                    ang = ((float)iang)/100.0;
                    changeAngleFlag = 1;
                    break;
                case 3:
                    health = stream_readInt(bs);
                    printf("reading health %d\n", health);
                    vecset(vectorEntityList.healthList, entID, health);
                    break;
            }
        }
    }


    if(changePosFlag)
    {
        if(entID != MAIN_ENT_ID)
        {
            int last = posIntp->last % 3;
            posIntp->pos[last][0] = x;
            posIntp->pos[last][1] = y;
            posIntp->timestamp[last] = getTimeMillis();
            posIntp->last++;

            printf("position received: %f %f\n", x, y);
        }
        else {
            entPos->pos[0] = x;
            entPos->pos[1] = y;
        }
    }


    if(changeAngleFlag)
    {
        if(entID != MAIN_ENT_ID)
        {
            int last = angIntp->last % 3;
            angIntp->angle[last] = ang;
            angIntp->timestamp[last] = getTimeMillis();
            angIntp->last++;
        }
    }


    return 0;
}


int isMainEnt;
int readInitParam(bitstream_t *bs)
{
    int testint = stream_readInt(bs);
    isMainEnt = stream_readBit(bs);
    printf("read init param bit %d %d\n", isMainEnt, testint);
    return 0;
}


int applyInitParam()
{
    int entID = ent_addVectorEntity();

    printf("apply init param %d %d\n", isMainEnt, entID);
    if(isMainEnt) {
        MAIN_ENT_ID = entID;
        printf("is main ent \n");
    };

    return entID;
}



int writeInitParam(int entID, int conID, bitstream_t *bs)
{
    int mainEnt = i2imap_get(vectorEntityList.mainEntMap, conID);
    printf("writing main ent %d %d \n", mainEnt, conID);
    stream_writeInt(bs, 123);
    if(mainEnt == -1) {
        stream_writeBit(bs, 0);
        return 0;
    }
    else {
        if(mainEnt == entID)
            stream_writeBit(bs, 1);
        else
            stream_writeBit(bs, 0);
    }

    return 0;
}


intPair_t ent_setupEntityForClient(int conID, netcon_t *con)
{
    int entID;

    intPair_t pair;
    
    entID = ent_addVectorEntity();

    pair.a = entID;
    pair.b = 0;

    i2imap_put(vectorEntityList.mainEntMap, conID, entID);
    return pair;
}

intPair_t ent_removeEntityFromClient(int conID, netcon_t *con)
{
    intPair_t pair;
    int entID = i2imap_get(vectorEntityList.mainEntMap, conID);
    ent_removeVectorEntity(entID);

    i2imap_remove(vectorEntityList.mainEntMap, conID);

    pair.a = entID;
    pair.b = 0;

    return pair;
}

void removeEntity(int entID)
{
    printf("removing entity : %d", entID);
    ent_removeVectorEntity(entID);
}

void initPickupList()
{
    entVec_t posToSet;
    entRect_t rectToSet;

    vec3xyz(posToSet.pos, 380, 300, 0);
    rect2xywh(rectToSet.rect,0,0,10,10);

    ent_initPickupList(&healthPickupList);

    ent_addPickup(&healthPickupList, posToSet, rectToSet);
}

void eng_initSerializerList()
{
    entitySerializer_t *list = (entitySerializer_t *)
        zidmalloc(GENERALZONE, sizeof(entitySerializer_t) * 1);
    
    ent_setSerializer(
        &list[0],
        4,
        readState,
        writeState,
        readInitParam,
        applyInitParam,
        writeInitParam,
        removeEntity
    );

    entSerializerList.list = list;
    entSerializerList.length = 1;
}


void eng_init()
{
    cvar_t *cv_isServer;

    int isServer = cvar_getInt("isServer");

    char keys[] = {'w', 's', 'a', 'd', 't'};

    inpConfig_storeUsedKeys(keys, sizeof(keys));


    if(isServer)
    {
        serv_init();
    }
    else {
        cl_init();
    }

    // world_load();
    physics_init();


    ent_init();

    eng_initSerializerList();

    initPickupList();
}


void eng_afterRender()
{

    set_baseline_to_actual();

    vecreset(entSpriteList.renderList);
    vecreset(animSpriteList.renderList);
}



void eng_handleServerEvents()
{
    sysEvent_t *ev;
    byte *buf;
    int len;


    inpCmd_clearPressed();
    
    int evNum = 0;
    while(!isSysEventEmpty())
    {
        ev = getSysEvent();
        switch(ev->type)
        {
            case SYSEVENT_PACKET:
                buf = (byte *) ev->ptr;
                len = ev->value;
                netaddr_t *fromAddr = (netaddr_t *) buf;
                buf += sizeof(netaddr_t);
                len -= sizeof(netaddr_t);
                serv_packetEvent(fromAddr, buf, len);
                zidfree(ev->ptr);
                break;

            default:
                break;
        }
    }

    
    serv_frame();
}



void eng_handleClientEvents()
{
    sysEvent_t *ev;
    byte *buf;
    int len;


    inpCmd_clearPressed();
    
    int evNum = 0;
    while(!isSysEventEmpty())
    {
        ev = getSysEvent();
        switch(ev->type)
        {
            case SYSEVENT_KEY:
                cl_keyEvent(ev->value);
                break;
            case SYSEVENT_MOUSE:
                float mouseX = ((float)(ev->value))/10000.0;
                float mouseY = ((float)(ev->value2))/10000.0;
                cl_mouseEvent(mouseX, mouseY);
                break;
            case SYSEVENT_PACKET:
                buf = (byte *) ev->ptr;
                len = ev->value;
                netaddr_t *fromAddr = (netaddr_t *) buf;
                buf += sizeof(netaddr_t);
                cl_packetEvent(fromAddr, buf, len);
                zidfree(ev->ptr);
                break;
            default:
                break;
        }
    }

    cl_frame();
}


void eng_runFrame()
{
    int isServer = cvar_getInt("isServer");

    if(isServer)
        eng_handleServerEvents();
    else
        eng_handleClientEvents();
}