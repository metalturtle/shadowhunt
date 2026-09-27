#include "../basic/basic.h"
#include "../basic/world_def.h"
#include "engine.h"
#include "entity.h"
#include "../movement/movement.h"
#include "stealth.h"

#define VECENT_SPEED 1

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
    PlayerData *playerData = stealth_player(vecEnt);
    if(playerData == NULL || vecEnt->health <= 0 || playerData->spectating)
        return;

    byte up, down, left, right, shoot;
    float vec3[3];
    float temp;
    float mouseXY[3];

    vecEnt->dir.x = 0;
    vecEnt->dir.y = 0;

    up = bm_getBitVal(inpCmd->key, 0);
    down = bm_getBitVal(inpCmd->key, 1);
    left = bm_getBitVal(inpCmd->key, 2);
    right = bm_getBitVal(inpCmd->key, 3);
    shoot = bm_getBitVal(inpCmd->key, 4);

    float speed = stealth_moveSpeed(vecEnt);
    if(speed <= 0)
        return;

    /* The client encodes its world-space aim as a point on a circle around
     * the viewport center, so the angle is independent of the camera. */
    mouseXY[0] = inpCmd->mouseX - 0.5f;
    mouseXY[1] = inpCmd->mouseY - 0.5f;
    mouseXY[2] = 0.0f;
    if(mouseXY[0] != 0.0f || mouseXY[1] != 0.0f)
        vecEnt->animSprite.angle = vec3getang2(mouseXY);

    if(up || down || left || right) {
        vec3xyz(vec3, right - left, down - up, 0);
        vec3unitvec(vec3, temp);
        vec3mult(vec3, speed);
        vecEnt->dir.x = vec3[0];
        vecEnt->dir.y = vec3[1];
    }

    float pos[2] = {vecEnt->pos.x, vecEnt->pos.y};
    float deltaTime = inpCmd->deltaTime;
    if(server)
        stealth_consumeMoveBudget(vecEnt, inpCmd, &deltaTime);

    moveEntityWithCollision(vecEnt, deltaTime);
    playerData->moving = vecEnt->dir.x != 0 || vecEnt->dir.y != 0;

    if(server && shTestLogs)
        printf("vector position %f %f \n", vecEnt->pos.x, vecEnt->pos.y);

    vecEnt->animSprite.pos[0] = vecEnt->pos.x;
    vecEnt->animSprite.pos[1] = vecEnt->pos.y;

    if(server && shoot && stealth_canShoot(vecEnt)) {
        float toAngle = rad2deg(vecEnt->animSprite.angle);
        ent_handleRayWeaponShoot(&rayWeaponHandle, vecEnt->entID,
                                 &playerData->weaponOnHand, pos, toAngle);
    }
}

void setupPlayer(VectorEntity *vecEnt, SaveDataHandler* saveHandle) {
    bool isServer = cvar_getInt("isServer") != 0;

    vecEnt->rect.x = -5;
    vecEnt->rect.y = -5;
    vecEnt->rect.w = 10;
    vecEnt->rect.h = 10;
    vecEnt->animSprite.rect[0] = -9;
    vecEnt->animSprite.rect[1] = -6.5f;
    vecEnt->animSprite.rect[2] = 18;
    vecEnt->animSprite.rect[3] = 13;
    vecEnt->animSprite.entID = vecEnt->entID;
    vecEnt->animSprite.texID = isServer ? 0 :
        sprite_getID("actor_torso_walk_machgun", SPRITE_TYPE_ANIM);
    vecEnt->animSprite.curSprite = 0;
    vecEnt->animSprite.angle = 0;
    vecEnt->dir.x = 0;
    vecEnt->dir.y = 0;
    vecEnt->health = 100;
    vecEnt->pos.x = vecEnt->pos.y = 0;

    PlayerData *playerData = NULL;
    int playerID = -1;
    for(int i = 0; i < 8; i++) {
        if(!playerDataList[i].active) {
            playerID = i;
            playerData = &playerDataList[i];
            break;
        }
    }

    if(playerID < 0)
        com_error(ERR_FATAL, "No free player slots\n");

    zmemset(playerData, 0, sizeof(PlayerData));
    playerData->active = true;
    playerData->rayEntID = -1;
    playerData->lastHealth = 100;
    vecEnt->externalID = playerID;

    startTimer(&playerData->shootTimer, 200);
    ent_setRayWeapon(&rayWeaponHandle, &playerData->weaponOnHand, vecEnt->entID);

    if(isServer)
        stealth_onPlayerJoined(vecEnt);
}

void updatePlayer(VectorEntity *vecEnt, bool isServer, inputCommand_t *inpCmd) {
    input_func_common(inpCmd, vecEnt, isServer);
}


void cleanupPlayer(VectorEntity *vecEnt) {
    PlayerData *playerData = stealth_player(vecEnt);
    if(playerData == NULL)
        return;
    ent_removeRayWeapon(&rayWeaponHandle, &playerData->weaponOnHand);
    playerData->active = false;
    playerData->rayEntID = -1;
    playerData->weaponShot = 0;
    playerData->role = SH_ROLE_NONE;
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
    netEntSys_init();
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
    for(int j = 0; j < VECTOR_ENTITY_COUNT; j++) {
        VectorEntity *vecEnt = &vectorEntityList[j];
        if(!vecEnt->active) continue;
        NetEntity *netEnt = &netEntityList[vecEnt->entID];
        if(netEnt->clientOwner < 0 || netEnt->clientOwner >= server.clRepList.size ||
           !bm_getBitVal(server.clRepBitMap.arr, netEnt->clientOwner))
            continue;
        serv_clrep_t *clRep = &vecget(server.clRepList, netEnt->clientOwner);
        inputCommandList_t *inpCmdList = &clRep->inputCommandList;
        int inpLen = inpCmd_getLen(inpCmdList);
        for(int i = 0; i < inpLen; i++) {
            inputCommand_t *inpCmd = inpCmd_get(inpCmdList, i);
            if(inpCmd->isDone) continue;
            spriteFactoryList[vecEnt->typeID].think(vecEnt, true, inpCmd);
            inpCmd->isDone = true;
        }
    }

    handle_ray_list(&rayWeaponHandle.rayHandleList);
    handle_ray_hits();
    ent_resetRayWeapon(&rayWeaponHandle);
}

#define INTERP_DELAY_MS 100

static void animatePlayer(VectorEntity *vecEnt, float prevX, float prevY)
{
    PlayerData *player = stealth_player(vecEnt);
    if(player == NULL)
        return;
    float dx = vecEnt->pos.x - prevX;
    float dy = vecEnt->pos.y - prevY;
    float dist = sqrtf(dx * dx + dy * dy);
    player->moving = dist > 0.01f;
    if(player->moving)
        player->moveAngle = atan2f(dy, dx);
    if(dist < 20.0f)
        player->walkCycle += dist / 14.0f;
    vecEnt->animSprite.curSprite = player->moving ? player->walkCycle : 0;
    vecEnt->animSprite.pos[0] = vecEnt->pos.x;
    vecEnt->animSprite.pos[1] = vecEnt->pos.y;
}

void entSys_updateClient() {
    inputCommandList_t *inpCmdList = &client.clRep.inputCommandList;
    unsigned long renderTime = getTimeMillis() - INTERP_DELAY_MS;

    for(int i = 0; i < VECTOR_ENTITY_COUNT; i++) {
        VectorEntity *vecEnt = &vectorEntityList[i];
        if(!vecEnt->active) continue;
        NetEntity *netEnt = &netEntityList[i];
        float prevX = vecEnt->animSprite.pos[0];
        float prevY = vecEnt->animSprite.pos[1];

        if(netEnt->isPuppet) {
            stealth_interpolate(vecEnt, renderTime);
        } else {
            /* Client-side prediction: replay every input the server has not
             * acknowledged on top of the last authoritative position. */
            int inpLen = inpCmd_getLen(inpCmdList);
            for(int j = 0; j < inpLen; j++) {
                inputCommand_t *inpCmd = inpCmd_get(inpCmdList, j);
                if(inpCmd->isDone) continue;
                input_func_common(inpCmd, vecEnt, false);
                inpCmd->isDone = true;
            }
            if(shPrediction.pending) {
                float ex = vecEnt->pos.x - shPrediction.beforeX;
                float ey = vecEnt->pos.y - shPrediction.beforeY;
                float error = sqrtf(ex * ex + ey * ey);
                if(shTestLogs && error > 0.5f)
                    printf("prediction correction %.2f\n", error);
                shPrediction.pending = false;
            }
        }

        animatePlayer(vecEnt, prevX, prevY);
    }
}

void entSys_cleanup() {

}

void entSys_close() {

}
