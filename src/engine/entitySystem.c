#include "../basic/basic.h"
#include "../basic/world_def.h"
#include "engine.h"
#include "entity.h"
#include "../movement/movement.h"

#define VECENT_SPEED 1

void interpolate_pos(VectorEntity *vecEnt)
{
    unsigned long simTime = getTimeMillis();
    simTime -= 200;

    // Puppet *puppet = &puppetList[vecEnt->externalID];



        positionInterpolate_t *posIntp = &vecEnt->posInterpolate;
        animatedSprite_t *sprite = &vecEnt->animSprite;



        if(posIntp->last < 3) {
            return;
        }
            
        

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
            return;


        if(posIntp->timestamp[nextPos] == posIntp->timestamp[lastPos])
        {
            printf("zero difference betwen timestamp %d %d %lu\n", lastPos, nextPos, posIntp->timestamp[nextPos]);
            return;
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


        vecEnt->pos.x = intpVec[0];
        vecEnt->pos.y = intpVec[1];
}

void set_sprite_angle(VectorEntity *vecEnt, inputCommandList_t *inpCmdList)
{
    
    int inpLen;
    inputCommand_t *inpCmd;
    float mouseXY[2];
    float angle;
    byte shoot;
    endTimer_t *shootTimer;
 
    animatedSprite_t *entSprite = &vecEnt->animSprite;

    // inpCmdList = &vecget(cl_inputList.list, conID);
    inpLen = inpCmd_getLen(inpCmdList);

    if(inpLen == 0)
        return;


    inpCmd = inpCmd_get(inpCmdList, inpLen - 1);
    

    mouseXY[0] = inpCmd->mouseX - 0.5;
    mouseXY[1] = inpCmd->mouseY - 0.5;
    angle = vec3getang2(mouseXY);


    entSprite->angle = angle;

}
void setRect(SDL_FRect *rect, float x, float y, float w, float h) {
    rect->x = MIN(x, x + w);;
    rect->y = MIN(y, y + h);
    rect->w = fabsf(w);
    rect->h = fabsf(h);
}

void setCamera(VectorEntity *vecEnt) {
    // cameraRect.x = vecEnt->pos.x - 50;
    // cameraRect.y = vecEnt->pos.y - 50;
    cameraRect.x = 300;
    cameraRect.y = 300;
    cameraRect.w = engineParameters.windowWidth;
    cameraRect.h = engineParameters.windowHeight;
}

void set_camera1(VectorEntity *vecEnt)
{

    float temp;
    float mouseXY[3];
    float mouseDist[3];
    float camPos[3];
    float diff[] = {-50, -50};
    inputCommand_t *inpCmd;

    inputCommandList_t *inpCmdList = &client.clRep.inputCommandList;

    int inpLen = inpCmd_getLen(inpCmdList);

    inpCmd = inpCmd_get(inpCmdList, inpLen);

    // float *mainEntPos = (vecget(vectorEntityList.movableList, MAIN_ENT_ID)).pos;
    float mainEntPos[2];
    mainEntPos[0] = vecEnt->pos.x;
    mainEntPos[1] = vecEnt->pos.y;


    mouseDist[0] = inpCmd->mouseX;
    mouseDist[1] = inpCmd->mouseY;
    mouseDist[2] = 0;
    vec3unitvec(mouseDist, temp);
    vec3mult(mouseDist, 7);

    vec2add(mouseDist, mouseDist, mainEntPos);
    // vec2sub(mouseDist, mouseDist, worldCamera.window);
    mouseDist[0] -= cameraRect.x;
    mouseDist[1] -= cameraRect.y;
    // printf("window pos %f,%f \n", worldCamera.window[0], worldCamera.window[1]);

    float distance = vec2length(mouseDist);
    float curSpeed = 0;

    // printf("distance calc: %f \n", distance);

    if(distance < 1) {
        curSpeed = 0;
    }
    else if (distance < 10) {
        curSpeed = distance/10;
    }
    else if (distance < 100)
    {
        curSpeed = 5;
    }
    else {
        // cameraRect.x = diff[0];
        // cameraRect.y = diff[1];
        setRect(&cameraRect, diff[0], diff[1], engineParameters.windowWidth, engineParameters.windowHeight);
        cameraRect.x = mainEntPos[0];
        cameraRect.y += mainEntPos[1];
        // rect2xywh(&worldCamera.window, diff[0], diff[1], engineParameters.windowWidth, engineParameters.windowHeight);
        // vec2add(worldCamera.window, worldCamera.window, mainEntPos); 
        return;
    }

    vec3mult(mouseDist, curSpeed/distance);

    setRect(&cameraRect, diff[0], diff[1], engineParameters.windowWidth, engineParameters.windowHeight);
    cameraRect.x += mainEntPos[0] + mouseDist[0];
    cameraRect.y += mainEntPos[1] + mouseDist[1];
    // rect2xywh(&worldCamera.window, diff[0], diff[1], engineParameters.windowWidth, engineParameters.windowHeight);
    // vec2add(worldCamera.window, worldCamera.window, mainEntPos);
    // vec2add(worldCamera.window, worldCamera.window, mouseDist);
    // mouseXY[0] = inpCmd->mouseX - 0.5;
    // mouseXY[1] = inpCmd->mouseY - 0.5;
    // mouseXY[2] = 0;
    // float angle = vec3getang2(mouseXY);

    set_sprite_angle(vecEnt, inpCmdList);  
}

void input_func_common(inputCommand_t *inpCmd, VectorEntity *vecEnt, int server)
{
    if(vecEnt->health < 0) {
        vecEnt->active = false;
        return;
    }

    int inpLen;
    byte up = false, down = false, left = false, right = false, shoot = false;
    float vec3[3];
    float temp;
    // float speed = 1;
    float mouseXY[3];
    float angle;
    float dir[3];


    vecEnt->dir.x = 0;
    vecEnt->dir.y = 0;

    up = bm_getBitVal(inpCmd->key, 0);
    down = bm_getBitVal(inpCmd->key, 1);
    left = bm_getBitVal(inpCmd->key, 2);
    right = bm_getBitVal(inpCmd->key, 3);
    shoot = bm_getBitVal(inpCmd->key, 4);


        // vec3add(moveEnt->dir, moveEnt->dir, vec3);
    if(up || down || left || right) {
        vec3xyz(vec3,
            right - left
            ,up - down
            ,0);
    
        float speed = VECENT_SPEED * 20;
        
        vec3unitvec(vec3, temp);
        vec3mult(vec3, speed);

        vecEnt->dir.x = vec3[0];
        vecEnt->dir.y = vec3[1];
    }

    if(server) {
        // ent_setStateFlags(VECTOR_SERIALIZER, vecEnt->entID, 0, true);
        // ent_setStateFlags(VECTOR_SERIALIZER, vecEnt->entID, 1, true);
    }


    float pos[2];
    pos[0] = vecEnt->pos.x;
    pos[1] = vecEnt->pos.y;

        
    moveEntityWithCollision(vecEnt, inpCmd->deltaTime);

    // printf("vector position %f %f \n", vecEnt->pos.x, vecEnt->pos.y);

    vecEnt->animSprite.pos[0] = vecEnt->pos.x + vecEnt->rect.x;vecEnt->animSprite.pos[1] = vecEnt->pos.y + vecEnt->rect.y;



        // printf("OUTPUT %d: pos=(%.10f,%.10f) vel=(%.10f,%.10f)\n",
        //     inpCmd->recordID,
        //     finalPos.x, finalPos.y,
        //     finalVel.x, finalVel.y);

        // printf("checking result %f %f %d %d %f %f,%f\n", vecEnt->pos.x, vecEnt->pos.y, inpCmd->recordID, left, inpCmd->deltaTime, , vecEnt->dir.x, vecEnt->dir.y);
    if(inpCmd->posCheck.x != 0 || inpCmd->posCheck.y != 0) {
        SDL_FPoint posCheck = inpCmd->posCheck;
        vec_subtract(&posCheck, &vecEnt->pos);
        float dist = vec_length(&posCheck);
        if(dist > 0.001) {
            printf("MISMATCH %d: expected=(%.10f,%.10f) got=(%.10f,%.10f) diff=%.10f\n",
                inpCmd->recordID,
                inpCmd->posCheck.x, inpCmd->posCheck.y,
                vecEnt->pos.x, vecEnt->pos.y,
                dist);
                // printf("Difference in result %f %f %d %f,%f %f %f,%f\n", posCheck.x, posCheck.y, inpCmd->recordID, inpCmd->posCheck.x, inpCmd->posCheck.y, inpCmd->deltaTime);
                // com_error(ERR_FATAL, "difference");
        }
    }
    inpCmd->posCheck = vecEnt->pos;

    PlayerData *playerData = &playerDataList[vecEnt->externalID];
    if(shoot)
    {
        int entID = vecEnt->entID;
        printf("called shoot \n");
        endTimer_t *shootTimer = &playerData->shootTimer;

        SDL_FPoint mouseScreenPos;
        mouseScreenPos.x = inpCmd->mouseX - 0.5;
        mouseScreenPos.y = inpCmd->mouseY - 0.5;

        float toAngle = rad2deg(vec_getAngle(&mouseScreenPos));

        weaponOnHand_t *weaponOnHand = &playerData->weaponOnHand;
        ent_handleRayWeaponShoot(&rayWeaponHandle, entID, weaponOnHand, pos, toAngle);

        // if(server)
        // {
        //     ent_setStateFlags(VECTOR_SERIALIZER, entID, 5, 1);
        // }
    }
}

void setupPlayer(VectorEntity *vecEnt, SaveDataHandler* saveHandle) {
    vecEnt->pos.x = 300;
    vecEnt->pos.y = 330;
    vecEnt->rect.x = -5;
    vecEnt->rect.y = -5;
    vecEnt->rect.w = 10;
    vecEnt->rect.h = 10;
    vecEnt->animSprite.rect[0] = vecEnt->animSprite.rect[1] = 0;
    vecEnt->animSprite.rect[2] = 10; vecEnt->animSprite.rect[3] = 10;
    vecEnt->dir.x = 0;
    vecEnt->dir.y = 0;
    vecEnt->health = 100;

    PlayerData *playerData = NULL;
    for(int i = 0; i < 8; i++) {
        playerData = &playerDataList[i];
        if(!playerData->active) {
            vecEnt->externalID = i;
            playerData->active = true;
            break;
        }
    }

    playerData->weaponShot = 0;
    playerData->rayEntID = -1;

    startTimer(&playerData->shootTimer, 200);
    zmemset(&playerData->weaponOnHand, 0, sizeof(weaponOnHand_t));

    ent_setRayWeapon(&rayWeaponHandle, &playerData->weaponOnHand, vecEnt->entID);
}

void updatePlayer(VectorEntity *vecEnt, bool isServer, inputCommand_t *inpCmd) {
    input_func_common(inpCmd, vecEnt, isServer);
    if(!isServer)
        setCamera(vecEnt);
}


void cleanupPlayer(VectorEntity *vecEnt) {
    PlayerData *playerData = &playerDataList[vecEnt->entID];
    playerData->active = false;
    playerData->rayEntID = -1;
    playerData->weaponShot = 0;
    // stopTimer(&playerData->shootTimer);
    ent_removeRayWeapon(&rayWeaponHandle, &playerData->weaponOnHand);
}

void serializePlayer(VectorEntity *ent, NetObj *netObj) {
    PlayerData *playerData = &playerDataList[ent->externalID];
    weaponOnHand_t *weapOnHand = &playerData->weaponOnHand;
    SDL_FPoint pos = ent->pos;
    handleNetEntry(netObj, &ent->pos.x, sizeof(ent->pos.x), ent);
    handleNetEntry(netObj, &ent->pos.y, sizeof(ent->pos.y), ent);
    handleNetEntry(netObj, &ent->animSprite.angle, sizeof(ent->animSprite.angle), ent);
    handleNetEntry(netObj, &ent->health, sizeof(ent->health), ent);
    // handleNetEntry(netObj, &weapOnHand->ammoCount, sizeof(weapOnHand->ammoCount), ent);
    // handleNetEntry(netObj, &ent->animSprite.angle, sizeof(ent->animSprite.angle), ent);

    // if(netObj->curState == WRITE) {
    //     printf("writing cur state %d %f %f \n", ent->entID, pos.x, pos.y);
    // }
    if(netObj->curState == READ) {
        vec_subtract(&pos, &ent->pos);
        float dist = vec_length(&pos);
        if(dist > 0) {
            // killCmd = true;

            // b2Vec2 correctedPos = {
            //     ent->pos.x - ent->rect.x,  // Convert from entity pos to body pos
            //     ent->pos.y - ent->rect.y
            // };
            // b2Rot currentRotation = b2Body_GetRotation(ent->collisionID);
            // b2Body_SetTransform(ent->collisionID, correctedPos, currentRotation);
            // cpSpaceRemoveBody(worldId, ent->collision);
            // ent->collision = createCircle(ent->pos.x, ent->pos.y, 5.0f);
            printf("checking received position %f %f \n", ent->pos.x, ent->pos.y);
        }
        
        if(netObj->curState == READ) {
            // positionInterpolate_t *posIntp = &ent->posInterpolate;
            // Puppet *puppet = &puppetList[ent->externalID];
            positionInterpolate_t *posIntp = &ent->posInterpolate;
            int last = posIntp->last % 3;
            posIntp->pos[last][0] = ent->pos.x;
            posIntp->pos[last][1] = ent->pos.y;
            posIntp->timestamp[last] = getTimeMillis();
            posIntp->last++;
        }

        
        // printf("checking entered values: %f, %f %f %f %d\n", ent->pos.x, ent->pos.y, ent->animSprite.angle, ent->health, weapOnHand->ammoCount);
        // handleNetEntry(netObj, &weapOnHand, weapOnHand->angle);
    }
}

void entSys_init() {

}

void initPlayerState(ESDef *esDef) {
    addESVar(esDef, sizeof(SDL_FPoint), NULL);
    addESVar(esDef, sizeof(SDL_FPoint), NULL);
    addESVar(esDef, sizeof(float), NULL);
    addESVar(esDef, sizeof(float), NULL);
}

void processPlayerState(VectorEntity *vecEnt, ESDef *esDef) {
    handleESVar(esDef, &vecEnt->pos.x);
    handleESVar(esDef, &vecEnt->pos.y);
    handleESVar(esDef, &vecEnt->animSprite.angle);
    handleESVar(esDef, &vecEnt->health);
}

void entSys_setup() {
    createSpriteFactory(setupPlayer, updatePlayer, cleanupPlayer, initPlayerState, processPlayerState);
}

void entSys_updateServer() {
    for(int j = 0; j < 8; j++) {
        VectorEntity *vecEnt = &vectorEntityList[j];
        if(!vecEnt->active) continue;
        NetEntity *netEnt = &netEntityList[vecEnt->entID];
        serv_clrep_t *clRep = &vecget(server.clRepList, netEnt->clientOwner);
        // printf("client owner %d \n", netEnt->clientOwner);
        inputCommandList_t *inpCmdList = &clRep->inputCommandList;
        inputCommand_t *inpCmd;
        int inpLen = inpCmd_getLen(inpCmdList);
        for(int i = 0; i < inpLen;i++) {
            inpCmd = inpCmd_get(inpCmdList, i);
            if(inpCmd->isDone) continue;
            printf("updated input command %d \n", inpCmd->recordID);
            spriteFactoryList[vecEnt->typeID].think(vecEnt, true, inpCmd);

            // bool up, down, left, right, shoot;
            // up = bm_getBitVal(inpCmd->key, 0);
            // down = bm_getBitVal(inpCmd->key, 1);
            // left = bm_getBitVal(inpCmd->key, 2);
            // right = bm_getBitVal(inpCmd->key, 3);
            // shoot = bm_getBitVal(inpCmd->key, 4);
            // printf("up down left right %d %d %d %d \n", up, down, left , right);;
        }
    }

    for(int i = 0; i < VECTOR_ENTITY_COUNT; i++) {
        VectorEntity *vecEnt = &vectorEntityList[i];
        if(!vecEnt->active) continue;

        NetEntity *netEnt = &netEntityList[i];

        inputCommandList_t *inpCmdList = &client.clRep.inputCommandList;

        inputCommand_t *inpCmd;
        int inpLen = inpCmd_getLen(inpCmdList);
        for(int i = 0; i < inpLen;i++)
        {
            inpCmd = inpCmd_get(inpCmdList, i);
            if(inpCmd->isDone) continue;
            inpCmd->isDone = true;
        }
    }
    
}

void updatePuppet(VectorEntity *vecEnt, bool isServer, inputCommand_t *inpCmd) {
    interpolate_pos(vecEnt);
    // interpolate_angle(vecEnt);
    vecEnt->animSprite.pos[0] = vecEnt->pos.x;vecEnt->animSprite.pos[1] = vecEnt->pos.y;
}


void entSys_updateClient() {
    for(int i = 0; i < VECTOR_ENTITY_COUNT; i++) {
        VectorEntity *vecEnt = &vectorEntityList[i];
        if(!vecEnt->active) continue;

        NetEntity *netEnt = &netEntityList[i];

        inputCommandList_t *inpCmdList = &client.clRep.inputCommandList;
        // inpCmdList = &vecget(cl_inputList.list, 0);

        // updateVectorEntity(vecEnt, false, inpCmdList);
        inputCommand_t *inpCmd;
        int inpLen = inpCmd_getLen(inpCmdList);
        for(int i = 0; i < inpLen;i++)
        {
            inpCmd = inpCmd_get(inpCmdList, i);
            if(inpCmd->isDone) continue;
            // inpCmd->isDone = true;

            updatePuppet(vecEnt, false, inpCmd);
            
            // setCamera(vecEnt);

            // if(netEnt->isPuppet) {
            //     updatePuppet(vecEnt, false, inpCmd);
            // }
            // else {
            //     updateVectorEntity(vecEnt, false, inpCmd);
            // }
        }


        // else {
        //     // updateVectorEntity(vecEnt, false, NULL);
        //     updatePuppet(vecEnt);
        // }
    }

    for(int i = 0; i < VECTOR_ENTITY_COUNT; i++) {
        VectorEntity *vecEnt = &vectorEntityList[i];
        if(!vecEnt->active) continue;

        NetEntity *netEnt = &netEntityList[i];

        inputCommandList_t *inpCmdList = &client.clRep.inputCommandList;

        inputCommand_t *inpCmd;
        int inpLen = inpCmd_getLen(inpCmdList);
        for(int i = 0; i < inpLen;i++)
        {
            inpCmd = inpCmd_get(inpCmdList, i);
            if(inpCmd->isDone) continue;
            inpCmd->isDone = true;
        }
    }
}

void entSys_cleanup() {

}

void entSys_close() {

}