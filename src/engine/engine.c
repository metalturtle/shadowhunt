#include "../basic/basic.h"
#include "../basic/world_def.h"
#include "engine.h"
#include "entity.h"
#include "../movement/movement.h"
#include "stealth.h"

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

#define MATCH_RESULTS_MS 3000

bool killCmd = false;

static int connectedPlayerCount(void)
{
    int connectedPlayers = 0;

    for(int conID = 0; conID < server.clRepList.size; conID++) {
        if(!bm_getBitVal(server.clRepBitMap.arr, conID))
            continue;

        int entID = i2imap_get(mainEntMap, conID);
        if(entID < 0 || entID >= VECTOR_ENTITY_COUNT || !vectorEntityList[entID].active)
            continue;
        connectedPlayers++;
    }

    return connectedPlayers;
}

bool eng_roundIsRunning(void)
{
    return MATCH_STATE == MATCH_RUNNING;
}

static const char *winnerName(int winner)
{
    return winner == SH_WINNER_HUNTERS ? "hunters" :
           winner == SH_WINNER_HIDERS ? "hiders" : "none";
}

static void startRunning(void)
{
    stealth_startRound();
    MATCH_STATE = MATCH_RUNNING;
    printf("match state: running\n");
}

static void updateRoundState(void)
{
    int connectedPlayers = connectedPlayerCount();

    switch(MATCH_STATE) {
        case MATCH_WAITING:
            /* A short lobby countdown lets friends who connect together all
             * make it into the first round. */
            if(connectedPlayers < 2) {
                shGame.lobbyEndAt = 0;
            } else if(shGame.lobbyEndAt == 0) {
                shGame.lobbyEndAt = getTimeMillis() + SH_LOBBY_MS;
                printf("match state: lobby countdown\n");
            } else if(getTimeMillis() >= shGame.lobbyEndAt) {
                shGame.lobbyEndAt = 0;
                startRunning();
            }
            break;
        case MATCH_RUNNING:
            if(connectedPlayers < 2) {
                stealth_resetForWaiting();
                MATCH_STATE = MATCH_WAITING;
                printf("match state: waiting\n");
                break;
            }
            if(stealth_evaluateRound() != SH_WINNER_NONE) {
                MATCH_STATE = MATCH_RESULTS;
                printf("match state: results\n");
                printf("round winner: %s\n", winnerName(shGame.winner));
                startTimer(&matchTimer, SH_RESULTS_MS);
            }
            break;
        case MATCH_RESULTS:
            if(checkTimer(&matchTimer)) {
                if(connectedPlayers >= 2) {
                    startRunning();
                } else {
                    stealth_resetForWaiting();
                    MATCH_STATE = MATCH_WAITING;
                    printf("match state: waiting\n");
                }
            }
            break;
        default:
            MATCH_STATE = MATCH_WAITING;
            break;
    }
}

///////////////////////////////////////////////////////////////////////


void clearWeaponShootStatus()
{
    inputCommandList_t *inpCmdList;

    for(int i = 0; i < vecsize(server.clRepList); i++)
    {
        if(!bm_getBitVal(server.clRepBitMap.arr, i))
            continue;

        
        int entID = i2imap_get(mainEntMap, i);
        if(entID < 0 || entID >= VECTOR_ENTITY_COUNT || !vectorEntityList[entID].active)
            continue;
        // vecset(vectorEntityList.weaponShotList, entID, 0);
        int playerID = vectorEntityList[entID].externalID;
        if(playerID < 0 || playerID >= 8)
            continue;
        PlayerData *playerData = &playerDataList[playerID];
        playerData->weaponShot = 0;
    }
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
    intPair_t pair;
    
    // printf("add vector entity\n");
    // entID = ent_addVectorEntity(true);
    // entID = 
    VectorEntity *vecEnt = addSprite(0, NULL, true, false);
    if(vecEnt == NULL) {
        pair.a = -1;
        pair.b = -1;
        return pair;
    }

    pair.a = vecEnt->entID;
    pair.b = vecEnt->typeID;

    i2imap_put(mainEntMap, conID, vecEnt->entID);
    return pair;
}

intPair_t ent_removeEntityFromClient(int conID, netcon_t *con)
{
    intPair_t pair;
    int entID = i2imap_get(mainEntMap, conID);

    i2imap_remove(mainEntMap, conID);

    if(entID >= 0 && entID < VECTOR_ENTITY_COUNT && vectorEntityList[entID].active)
        ent_remove(entID);

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

    stealth_init();

    bot_init();


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

    
    static unsigned long lastServerFrame;
    unsigned long now = getTimeMillis();
    float elapsed = lastServerFrame == 0 ? 0 : (now - lastServerFrame) / 1000.0f;
    lastServerFrame = now;

    stealth_pollHotReload();
    serv_checkTimeout();
    updateRoundState();
    stealth_refillMoveBudgets(elapsed);
    entSys_updateServer();
    stealth_serverTick();

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
                len -= sizeof(netaddr_t);
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

    stealth_pollHotReload();
    bot_update();

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
    if(com_verbose()) printf("world_load: stub called - implement level loading here\n");
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
