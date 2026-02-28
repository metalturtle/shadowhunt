#include "../basic/basic.h"
#include "../basic/world_def.h"
#include "engine.h"
#include "entity.h"
#include "../movement/movement.h"

// camera_t worldCamera;
// cl_inputList_t cl_inputList;

VectorEntity mainEntity;
// SDL_FPoint baselinePoints[INPCMD_MAX_SIZE];
float P_X = 300,P_Y = 300;

byte keyMap[256];

struct inputCmdConfig_st inpCmdConfig;
pickupList_t healthPickupList;
pickupList_t weaponPickupList;

endTimer_t pickupSpawnTimer;
PlayerData playerDataList[8];
// Puppet puppetList[8];
i2imap_t *mainEntMap;


#define VECENT_SPEED 1
#define VECTOR_TYPE_ID 1


int MATCH_STATE = 0;
endTimer_t matchTimer;

bool killCmd = false;

///////////////////////////////////////////////////////////////////////


void clearWeaponShootStatus()
{
    inputCommandList_t *inpCmdList;

    for(int i = 0; i < vecsize(server.clRepList); i++)
    {
        if(!bm_getBitVal(server.clRepBitMap.arr, i))
            continue;

        
        int entID = i2imap_get(mainEntMap, i);
        // vecset(vectorEntityList.weaponShotList, entID, 0);
        PlayerData *playerData = &playerDataList[entID];
        playerData->weaponShot = 0;
    }
}




void interpolate_angle(VectorEntity *vecEnt)
{
    long simTime = getTimeMillis();
    simTime -= 200;

    // Puppet *puppet = &puppetList[vecEnt->externalID];

        angleInterpolate_t *angIntp = &vecEnt->angleInterpolate;
        animatedSprite_t *sprite = &vecEnt->animSprite;


        // if(e == MAIN_ENT_ID)
        //     return;


        if(angIntp->last < 3)
            return;
        

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
            return;


        if(angIntp->timestamp[nextPos] == angIntp->timestamp[lastPos])
        {
            printf("zero difference betwen timestamp %d %d %lu\n", lastPos, nextPos, angIntp->timestamp[nextPos]);
            return;
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


void add_sprite_for_render()
{
    for(int e = 0; e < VECTOR_ENTITY_COUNT; e++)
    {
        // animatedSprite_t *sprite = &vecget(vectorEntityList.animSpriteList, e);
        VectorEntity *vecEnt = &vectorEntityList[e];
    
        // if(!bm_getBitVal(vectorEntityList.bitmap.arr, e))
        if(!vecEnt->active)
            continue;
        
        // animatedSprite_t *animSprite = &vecEnt->animSprite;

        vecpush(animSpriteList.renderList, animatedSprite_t, vecEnt->animSprite);
    }
}



void eng_processServerEntities()
{
    // updateMove(true);

    // set_camera();

    // printf("gameDeltaTime %f \n", engineParameters.gameDeltaTime);
    // b2World_Step(worldId, engineParameters.gameDeltaTime, 4);

    // updatePos();
    
    // kill_vector_entities();

    // ent_resetKillList();

    // check_weapon_pickup_collided();

    // check_pickup_collided();

    // eng_runMatch();

    // add_health_pickup();

    // set_move_from_pos();


    // input_func_server();


    // set_physics_movement();


    // physics_run();


    // set_physics_to_move();


    // check_move_func();


    // set_pos_from_move();
    
}

void eng_processClientEntities()
{

    // set_actual_to_baseline();

    
    // set_move_from_pos();


    // input_func_client();



    // for(int i = 0; i < inpLen; i++) {
    //     inpCmd = inpCmd_get(inpCmdList, i);
    //     inpCmd->isDone = true;
    // }

    // if(MAIN_ENT_ID != -1) {
    //     moveTest(&vectorEntityList[MAIN_ENT_ID]);
    // }
    

    // if()
    // updateMove(false);


    // b2World_Step(worldId, engineParameters.gameDeltaTime, 4);

    // updatePos();

    // setCamera();


    // set_physics_movement();


    // physics_run();


    // set_physics_to_move();


    // set_pos_from_move();




    // set_camera();


    // handle_ray_list(&rayWeaponHandle.rayHandleList);


    // handle_ray_hits();


    // ent_resetHitEntityList(&rayWeaponHandle.rayHandleList);


    add_sprite_for_render();


    // add_pickup_sprite();


    // add_ray_to_render();


    // ent_resetRayWeapon(&rayWeaponHandle);

    // for(int i = 0; i < VECTOR_ENTITY_COUNT; i++) {
    //     VectorEntity *vecEnt = &vectorEntityList[i];
    //     NetEntity *netEnt = &netEntityList[i];
    //     if(!vecEnt->active) continue;

        
    //     if(netEnt->isNew) {
    //         // printf("new entity %d \n", i);
    //         netEnt->isNew = false;
    //     }
    //     if(netEnt->isRemoved) {
    //         // printf("removed entity %d \n", i);
    //         netEnt->isRemoved = false;
    //     }
    // }

}


void setupPuppet(VectorEntity *vecEnt, SaveDataHandler* saveHandle) {
    // for(int i = 0; i < VECTOR_ENTITY_COUNT; i++) {
    //     Puppet *puppet = &puppetList[i];
    //     if(!puppet->active) {
    //         vecEnt->externalID = i;
    //         puppet->active = true;
    //         memset(&puppet->posInterpolate, 0, sizeof(positionInterpolate_t));
    //         memset(&puppet->angleInterpolate, 0, sizeof(angleInterpolate_t));
    //         break;
    //     }
    // }
}


intPair_t ent_setupEntityForClient(int conID, netcon_t *con)
{
    int entID;

    intPair_t pair;
    
    // printf("add vector entity\n");
    // entID = ent_addVectorEntity(true);
    // entID = 
    VectorEntity *vecEnt = addSprite(0, NULL, true, false);

    pair.a = vecEnt->entID;
    pair.b = vecEnt->typeID;

    i2imap_put(mainEntMap, conID, entID);
    return pair;
}

intPair_t ent_removeEntityFromClient(int conID, netcon_t *con)
{
    intPair_t pair;
    int entID = i2imap_get(mainEntMap, conID);
    // ent_removeVectorEntity(entID);
    

    i2imap_remove(mainEntMap, conID);

    pair.a = entID;
    pair.b = 0;

    return pair;
}

void removeEntity(int entID)
{
    printf("removing entity : %d", entID);
    // ent_removeVectorEntity(entID);
}



void eng_afterRender()
{

    // set_baseline_to_actual();

    vecreset(entSpriteList.renderList);
    vecreset(animSpriteList.renderList);
}



void eng_init() {
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

    world_load();
    // physics_init();


    ent_init();

    entSys_init();


    // initPickupList();

    // initWeaponPickupList();

    mainEntMap = i2imap_init(GENERALZONE);

    startTimer(&pickupSpawnTimer, 5000);
}

void eng_setup() {
    
    int isServer = cvar_getInt("isServer");
    if(isServer) {
        sv_setup();
    } else {
        cl_setup();
    }
    entSys_setup();

    world_setup();
}

void eng_updateServer() {
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

    
    // serv_frame();
    entSys_updateServer();

    // clear

    serv_clearInputs();

    serv_sendPacketAll();
}

void eng_updateClient() {
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
            // printf("checking key event %d \n", ev->value);
            cl_keyEvent(ev->value);
            break;
        case SYSEVENT_MOUSE:
        {
            float mouseX = ((float)(ev->value))/10000.0;
            float mouseY = ((float)(ev->value2))/10000.0;
            // printf("mouse %f,%f \n", mouseX, mouseY);
            cl_mouseEvent(mouseX, mouseY);
            break;
        }
        case SYSEVENT_PACKET:
        {
                buf = (byte *) ev->ptr;
                len = ev->value;
                netaddr_t *fromAddr = (netaddr_t *) buf;
                buf += sizeof(netaddr_t);
                cl_packetEvent(fromAddr, buf, len);
                zidfree(ev->ptr);
                break;
        }
            default:
                break;
        }
    }

    // cl_frame();
    // cl_update();

    cl_addInputCmd();

    entSys_updateClient();

    cl_update();



    // add_sprite_for_render();
}

void eng_cleanup() {

}

void eng_close() {

}

/* Stub implementations for missing functions */

void world_load(void) {
    // Stub: Load world/level data
    printf("world_load: stub called - implement level loading here\n");
}

void eng_runFrame(void) {
    // Run one frame of the game loop
    int isServer = cvar_getInt("isServer");
    
    if (isServer) {
        eng_updateServer();
    } else {
        eng_updateClient();
    }
}