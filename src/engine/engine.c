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

    // printf("key len %d %d\n", inpCmdConfig.keyByteLen, totalKeyLen);

    for(int i = 0; i < INPCMD_MAX_SIZE; i++)
    {
        inpCmdList->inpCmdArr[i].key = ptr;
        ptr += inpCmdConfig.keyByteLen;
        // inpCmdList->inpCmdArr[i].key = (byte *)zidmalloc(GENERALZONE, inpCmdConfig.keyByteLen);
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
//			normal.set(-1,0);
			staticNormalAngle=180;
		}
		else if(pos[0] > wall[0] + wall[2])
		{
            vec2xy(stat_p[0], wall[0] + wall[2], wall[1]);
            vec2xy(stat_p[1], 0, wall[3]);

            calc = ray_intersect(pos,dir,stat_p[0],stat_p[1]);
			u = MIN(calc, u);
//			normal.set(1,0);.
			staticNormalAngle=0;
		}
		if(pos[1] < wall[1])
		{
            vec2xy(stat_p[0], wall[0], wall[1]);
            vec2xy(stat_p[1], wall[2], 0);

			// stat_p[0].set(wall.x(),wall.y());
			// stat_p[1].set(wall.w(),0);
            calc = ray_intersect(pos,dir,stat_p[0],stat_p[1]);
			u = MIN(calc, u);
//			normal.set(0,-1);.
			staticNormalAngle=-90;
		}
		else if (pos[1] > wall[1] + wall[3])
		{
            vec2xy(stat_p[0], wall[0], wall[1] + wall[3]);
            vec2xy(stat_p[1], wall[2], 0);


            calc = ray_intersect(pos,dir,stat_p[0],stat_p[1]);
			u = MIN(calc, u);
//			normal.set(0,1);.
			staticNormalAngle=90;
		}
		return u;	
	}


void handle_ray(int i)
{
	float rpos[2];
	float rdir[2];


    float u = 1, temp;
    
    rpos[0] = vecget(rayList.xList, i);
    rpos[1] = vecget(rayList.yList, i);
    rdir[0] = vecget(rayList.xDirList, i);
    rdir[1] = vecget(rayList.yDirList, i);


    rect2_t wall;
    for(int i = 0; i < world.worldWallSize; i++)
    {
        rect2set(wall, world.worldWallArray[i].rect);

        temp = check_intersection(rpos, rdir, wall);

        if(u > temp)
        {
            u = temp;
        }
    }

    

    vec3mult(rdir, u);

    printf("u val %f  %f %f,%f \n", u, temp, rdir[0], rdir[1]);

    vecset(rayList.xDirList, i, rdir[0]);
    vecset(rayList.yDirList, i, rdir[1]);
}


void handle_ray_list()
{
    for(int i = 0; i < vecsize(rayList.xList); i++)
    {
        handle_ray(i);
    }
}


void add_ray_to_render()
{
    float pos[2];
    float dir[2];
    for(int i = 0; i < vecsize(rayList.xList); i++)
    {
        pos[0] = vecget(rayList.xList, i);
        pos[1] = vecget(rayList.yList, i);

        dir[0] = vecget(rayList.xDirList, i);
        dir[1] = vecget(rayList.yDirList, i);

        vecpush(renderRayList.xList, float,pos[0]);
        vecpush(renderRayList.yList, float, pos[1]);
        vecpush(renderRayList.xDirList, float, dir[0]);
        vecpush(renderRayList.yDirList, float, dir[1]);
    }

}

void input_func_common(inputCommandList_t *inpCmdList, movableEntity_t *moveEnt)
{
    inputCommand_t *inpCmd;
    int entID, inpLen;
    byte up, down, left, right;
    float vec3[3];
    float temp;
    float speed = VECENT_SPEED;
    float mouseXY[2];
    float angle;

    inpLen = inpCmd_getLen(inpCmdList);

    for(int i = 0; i < inpLen; i++)
    {
        inpCmd = inpCmd_get(inpCmdList, i);

        up = bm_getBitVal(inpCmd->key, 0);
        down = bm_getBitVal(inpCmd->key, 1);
        left = bm_getBitVal(inpCmd->key, 2);
        right = bm_getBitVal(inpCmd->key, 3);


        vec3xyz(vec3,
            right - left
            ,up - down
            ,0);

    
        vec3unitvec(vec3, temp);
        vec3mult(vec3, speed);
        vec3add(moveEnt->move.dir, moveEnt->move.dir, vec3);

    
        // if(moveEnt->move.dir[0] != 0 || moveEnt->move.dir[1] != 0)
            // printf("setting some movement %p\n", moveEnt->move.dir);
        

        // vec3add(moveEnt->move.pos, moveEnt->pos, moveEnt->move.dir);
        // inpCmd->inpX = moveEnt->move.pos[0];
        // inpCmd->inpY = moveEnt->move.pos[1];

 
        // printf("pos for rec %d: %f, %f   %f,%f  %p\n",
        //     inpCmd->recordID,
        //     moveEnt->move.pos[0],
        //     moveEnt->move.pos[1],
        //     inpCmd->inpX,
        //     inpCmd->inpY,
        //     inpCmd
        // );
        
        moveEnt->move.speed = 0.2f;
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
        movableEntity_t *moveEnt = &vecget(vectorEntityList.movableList, entID);
        input_func_common(inpCmdList, moveEnt);
    }
}


void input_func_client()
{
    inputCommandList_t *inpCmdList;
    inpCmdList = &vecget(cl_inputList.list, 0);
    
    movableEntity_t *moveEnt = &vecget(vectorEntityList.movableList, MAIN_ENT_ID);
    input_func_common(inpCmdList, moveEnt);
}


void set_physics_movement()
{
    for(int e = 0; e < vecsize(vectorEntityList.movableList); e++)
    {
        if(!bm_getBitVal(vectorEntityList.bitmap.arr, e))
            continue;


        movableEntity_t *moveEnt = &vecget(vectorEntityList.movableList, e);

        // printf("checking movement %p\n", moveEnt->move.dir);
        // if(moveEnt->move.dir[0] != 0 || moveEnt->move.dir[1] != 0)
            // printf("setting some movement %p %f %f\n", moveEnt->move.dir, moveEnt->move.dir[0], moveEnt->move.dir[1]);
        

        int moveID = vecget(vectorEntityList.moveIDList, e);
        physics_setBody(&moveEnt->move, moveID);
    } 
}

void sprite_func()
{
    for(int e = 0; e < vecsize(vectorEntityList.movableList); e++)
    {
        if(!bm_getBitVal(vectorEntityList.bitmap.arr, e))
            continue;

        movableEntity_t *moveEnt = &vecget(vectorEntityList.movableList, e);
        // entitySprite_t *entSprite = &vecget(vectorEntityList.spriteList, e);
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
    float dir[3];


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


    shoot = bm_getBitVal(inpCmd->key, 4);


    if(shoot)
    {
        shootTimer = &vecget(vectorEntityList.shootTimerList, entID);
        if(checkTimer(shootTimer))
        {
            movableEntity_t *moveEnt = &vecget(vectorEntityList.movableList, entID);

            vec3setang2(dir, angle);
            vec3mult(dir, 50);

            vecpush(rayList.xList, float, moveEnt->pos[0]);
            vecpush(rayList.yList, float, moveEnt->pos[1]);
            vecpush(rayList.xDirList, float, dir[0]);
            vecpush(rayList.yDirList, float, dir[1]);


            startTimer(shootTimer, 200);
            printf("shooting \n");
        }
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


void check_move_func()
{
    float zeroVec[] = {0, 0, 0};
    for(int i = 0; i < vecsize(vectorEntityList.movableList); i++)
    {
        if(!bm_getBitVal(vectorEntityList.bitmap.arr, i))
            continue;
    

        movableEntity_t *moveEnt = &vecget(vectorEntityList.movableList, i);

        float xdiff = moveEnt->pos[0] - moveEnt->move.pos[0];
        float ydiff = moveEnt->pos[1] - moveEnt->move.pos[1];


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
    entityMove_t *entMove;
    for(int i = 0; i < vecsize(vectorEntityList.movableList); i++)
    {
        if(!bm_getBitVal(vectorEntityList.bitmap.arr, i))
            continue;

        
        movableEntity_t *moveEnt = &vecget(vectorEntityList.movableList, i);
        int moveID = vecget(vectorEntityList.moveIDList, i);
        entMove = physics_get(moveID);

        zmemcpy(&moveEnt->move, entMove, sizeof(entityMove_t));
    }
}

void set_pos_from_move()
{
    float zeroVec[] = {0, 0, 0};
    for(int i = 0; i < vecsize(vectorEntityList.movableList); i++)
    {
        if(!bm_getBitVal(vectorEntityList.bitmap.arr, i))
            continue;
    

        movableEntity_t *moveEnt = &vecget(vectorEntityList.movableList, i);
        // entitySprite_t *entSprite = &vecget(vectorEntityList.spriteList, i);

        animatedSprite_t *entSprite = &vecget(vectorEntityList.animSpriteList, i);

        // if(moveEnt->move.dir[0] == 0 && moveEnt->move.dir[1] == 0 && moveEnt->move.dir[2] == 0)
        //     continue;

        float dist = vec3dist(moveEnt->pos, moveEnt->move.pos);

    

        vec3set(moveEnt->pos, moveEnt->move.pos);
        vec3set(moveEnt->move.dir, zeroVec);

        vec3set(entSprite->pos, moveEnt->pos);

    }
}


void interpolate_pos()
{
    long simTime = getTimeMillis();
    simTime -= 200;
    for(int e = 0; e < vecsize(vectorEntityList.movableList); e++)
    {
        movableEntity_t *moveEnt = &vecget(vectorEntityList.movableList, e);
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


        moveEnt->pos[0] = intpVec[0];
        moveEnt->pos[1] = intpVec[1];


        
        // printf("angle %f %f\n", sprite->angle, posIntp->angle[nextPos]);


        // moveEnt->pos[0] = (prevX * a) + (nextX * (1 - a));
        // moveEnt->pos[1] = (prevY * a) + (nextY * (1 - a));




        // printf("checking a %f %f %f \n", ((float)(posIntp->timestamp[nextPos] - simTime)), func_absFloat((float)(posIntp->timestamp[nextPos] - posIntp->timestamp[lastPos])),  a);
        // printf("move ent pos %f %f %f\n", prevX, nextX, a);
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



        float degAng = rad2deg(angIntp->angle[nextPos]);
        float curAng = rad2deg(angIntp->angle[lastPos]);

        // degAng = func_absFloat(degAng + 360);
        if(degAng < 0) degAng = 360 + degAng;
        if(curAng < 0) curAng = 360 + curAng;

        int idegAng = (int)(degAng * 100);
        int icurAng = (int)(curAng * 100);


        // int angDiff = idegAng - icurAng;
        float a = ((float)(angIntp->timestamp[nextPos] - simTime))
            /func_absFloat((float)(angIntp->timestamp[nextPos] - angIntp->timestamp[lastPos]));

        
        float diff = degAng - curAng;
        int sig = SIGNUM(diff);

        
        float diff2 = 360 - diff;
        if(diff2 < diff) {
            diff = -1 * sig * diff2;
        }

        float angToSet = curAng + diff * (1 - a);
        sprite->angle = deg2rad(angToSet);
        // sprite->angle = (curAng * a + (1-a) * degAng);
        // float angToSet = (curAng * a + (1-a) * degAng);
        printf("checking angle %f \n", angToSet);
    }
}


void run_move_func()
{
    float temp;
    float vec3[3];

    for(int e = 0; e < vecsize(vectorEntityList.movableList); e++)
    {
        movableEntity_t *moveEnt = &vecget(vectorEntityList.movableList, e);
    
        if(!bm_getBitVal(vectorEntityList.bitmap.arr, e))
            continue;

        
        vec3set(vec3, moveEnt->move.dir);

        // if(vec3[0] == 0 && vec3[1] == 0 && vec3[2] == 0)
        //     continue;
        
        // vec3unitvec(vec3, temp);
        // vec3mult(vec3, moveEnt->move.speed);
        // // printf("vec3 %f %f %f\n", vec3[0], vec3[1], vec3[2]);
        // vec3add(moveEnt->move.pos, moveEnt->move.pos, vec3);
        // vec3set(moveEnt->move.dir, zeroVec);

        vec3add(moveEnt->move.pos, moveEnt->move.pos, moveEnt->move.dir);


        // printf("move dir %f %f %f \n", moveEnt->move.dir[0], moveEnt->move.dir[1], moveEnt->move.dir[2]);
        // printf("move pos %f %f %f \n", moveEnt->move.pos[0], moveEnt->move.pos[1], moveEnt->move.pos[2]);
    }
}


void add_sprite_for_render()
{
    for(int e = 0; e < vecsize(vectorEntityList.animSpriteList); e++)
    {
        // entitySprite_t *sprite = &vecget(vectorEntityList.spriteList, e);
        animatedSprite_t *sprite = &vecget(vectorEntityList.animSpriteList, e);
    
        if(!bm_getBitVal(vectorEntityList.bitmap.arr, e))
            continue;
        

        // vecpushempty(entSpriteList.renderList, entitySprite_t);
        // vecpush(entSpriteList.renderList, entitySprite_t, (*sprite));
        vecpush(animSpriteList.renderList, animatedSprite_t, (*sprite));
    }
}

void set_move_from_pos()
{
    for(int e = 0; e < vecsize(vectorEntityList.movableList); e++)
    {
        if(!bm_getBitVal(vectorEntityList.bitmap.arr, e))
            continue;


        movableEntity_t *moveEnt = &vecget(vectorEntityList.movableList, e);

        vec3set(moveEnt->move.pos, moveEnt->pos);
    }
}

void eng_processServerEntities()
{
    set_move_from_pos();


    input_func_server();

    // run_move_func();
    set_physics_movement();


    physics_run();

    set_physics_to_move();

    check_move_func();

    set_pos_from_move();


    set_sprite_angle_server();

    // if(!checkTimer(&server.sendTimer)) {
    //     return;
    // }

    // movableEntity_t *moveEnt = &vecget(vectorEntityList.movableList, 0);
    // printf("entity pos %f %f \n", moveEnt->pos[0], moveEnt->pos[1]);
}


void set_actual_to_baseline()
{
    if(MAIN_ENT_ID == -1)  return;


    movableEntity_t *moveEnt = &vecget(vectorEntityList.movableList, MAIN_ENT_ID);
    animatedSprite_t *sprite = &vecget(vectorEntityList.animSpriteList, MAIN_ENT_ID);

    vec3set(mainEntity.movable.pos, moveEnt->pos);
    rect2set(mainEntity.movable.bound, moveEnt->bound);


    vec3set(mainEntity.movable.move.pos, moveEnt->move.pos);
    rect2set(mainEntity.movable.move.rect, moveEnt->move.rect);


    vec3set(mainEntity.sprite.pos, moveEnt->pos);
    rect2set(mainEntity.sprite.rect, moveEnt->move.rect);
}


void eng_processClientEntities()
{

    set_actual_to_baseline();

    
    set_move_from_pos();


    input_func_client();


    set_physics_movement();


    physics_run();

    // run_move_func();
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



    handle_ray_list();


    add_sprite_for_render();


    add_ray_to_render();


    // if(!checkTimer(&client.clRep.sendTimer))
    //     return;

    // movableEntity_t *moveEnt = &vecget(vectorEntityList.movableList, 0);
    // printf("entity pos %f %f \n", moveEnt->pos[0], moveEnt->pos[1]);
}


int writeState(int entID, bitstream_t *bs, byte *bm, int conID)
{
    movableEntity_t *movEnt = &vecget(vectorEntityList.movableList, entID);
    animatedSprite_t *sprite = &vecget(vectorEntityList.animSpriteList, entID);

    int xval = (int)(movEnt->pos[0] * 100);
    int yval = (int)(movEnt->pos[1] * 100);
    int ang = (int)(sprite->angle * 100);

    for(int i = 0; i < 3; i++)
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

    xval = yval = 0;
    iang = 0;

    movableEntity_t *movEnt = &vecget(vectorEntityList.movableList, entID);
    positionInterpolate_t *posIntp = &vecget(vectorEntityList.posInterpolateList, entID);
    angleInterpolate_t *angIntp = &vecget(vectorEntityList.angleInterpolateList, entID);
    animatedSprite_t *sprite = &vecget(vectorEntityList.animSpriteList, entID);

    float x = movEnt->pos[0], y = movEnt->pos[1];
    float ang = sprite->angle;


    for(int i = 0; i < 3; i++)
    {
        if(bm_getBitVal(bm, i))
        {
            switch(i)
            {
                case 0:
                    // printf("pressed yval\n");
                    yval = stream_readInt(bs);
                    // movEnt->pos[1] = ((float)yval)/100.0;
                    y = ((float)yval)/100.0;
                    changePosFlag = 1;
                    P_Y = movEnt->pos[1];
                    break;
                case 1:
                    // printf("pressed xval\n");
                    xval = stream_readInt(bs);
                    // movEnt->pos[0] = ((float)xval)/100.0;
                    x = ((float)xval)/100.0;
                    changePosFlag = 1;
                    P_X = movEnt->pos[0];
                    break;
                case 2:
                    iang = stream_readInt(bs);
                    ang = ((float)iang)/100.0;
                    changeAngleFlag = 1;
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
        }
        else {
            movEnt->pos[0] = x;
            movEnt->pos[1] = y;
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


void eng_initSerializerList()
{
    entitySerializer_t *list = (entitySerializer_t *)
        zidmalloc(GENERALZONE, sizeof(entitySerializer_t) * 1);
    
    ent_setSerializer(
        &list[0],
        3,
        readState,
        writeState,
        readInitParam,
        applyInitParam,
        writeInitParam
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
}


void set_baseline_to_actual()
{
    if(MAIN_ENT_ID == -1) return;


    movableEntity_t *moveEnt = &vecget(vectorEntityList.movableList, MAIN_ENT_ID);
    // entitySprite_t *sprite = &vecget(vectorEntityList.spriteList, MAIN_ENT_ID);
    animatedSprite_t *sprite = &vecget(vectorEntityList.animSpriteList, MAIN_ENT_ID);

    vec3set(moveEnt->pos, mainEntity.movable.pos);
    rect2set(moveEnt->bound, mainEntity.movable.bound);

    
    vec3set(moveEnt->move.pos, mainEntity.movable.move.pos);
    rect2set(moveEnt->move.rect, mainEntity.movable.move.rect);
    
    vec3set(sprite->pos, mainEntity.sprite.pos);
}


void eng_afterRender()
{

    set_baseline_to_actual();

    vecreset(entSpriteList.renderList);
    vecreset(animSpriteList.renderList);

    vecreset(rayList.xList);
    vecreset(rayList.yList);
    vecreset(rayList.xDirList);
    vecreset(rayList.yDirList);
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