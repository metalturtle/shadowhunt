#include "entity.h"
#include "engine.h"
#include "../movement/movement.h"


s2imap_t *spriteNameMap;
s2imap_t *animSpriteNameMap;

i2imap_t *translateIDMap;

entityList_t entList;
VectorEntity vectorEntityList[VECTOR_ENTITY_COUNT];
NetEntity netEntityList[VECTOR_ENTITY_COUNT];

i2imap_t *mainEntMap;

// entitySerializerList_t entSerializerList;


entitySpriteList_t entSpriteList;
animatedSpriteList_t animSpriteList;

renderRayList_t renderRayList;
// emittedRayList_t emittedRayList;
// rayEntityList_t rayEntityList;
// rayHitList_t rayHitList;
killIDList_t killIDList;
// rayHandleList_t rayHandleList;
rayWeaponHandle_t rayWeaponHandle;

SpriteFactory spriteFactoryList[100];
static int spriteFactoryCount;

// #define PUPPET_TYPE 0


float shootAngleList[8] = {0, -1,3, -5, 4, 2, -1, -2};
int shootAngListLen = 8;

/********************ENTITY********************/

VectorEntity* getVectorEntity(int spriteID) {
    return &vectorEntityList[spriteID];
}

// void ent_initEntList()
// {
//     int initSize = ENT_INITSIZE;
//     vecinit(GENERALZONE, entList.entBm, byte, initSize/8);
//     zmemset(entList.entBm.arr, 0, entList.entBm.capacity);

// }

int ent_addGlobalEntity()
{
    int id;

    int maxSize = vecsize(entList.entBm) * 8;
    id = bm_findEmpty(entList.entBm.arr, maxSize);
    
    if(id < 0) {
        vecpush(entList.entBm, byte, 0);
        id = maxSize - 1;
    }

    bm_setBitVal(entList.entBm.arr, id, 1);

    
    return id;
}


void ent_removeGlobalEntity(int entID)
{
    bm_setBitVal(entList.entBm.arr, entID, 0);
}

/********************SPRITE********************/

void sprite_init()
{
    spriteNameMap = s2imap_create(PERMANENTZONE);
    animSpriteNameMap = s2imap_create(PERMANENTZONE);
}

void sprite_add(char *spriteName, int id, int type)
{
    if(type == SPRITE_TYPE_STATIC)
    {
        s2imap_put(spriteNameMap, spriteName, id);
    }
    if(type == SPRITE_TYPE_ANIM)
    {
        s2imap_put(animSpriteNameMap, spriteName, id);
    }
    
}

int sprite_getID(char *spriteName, int type)
{
    if(type == SPRITE_TYPE_ANIM)
        return s2imap_get(animSpriteNameMap, spriteName);

    return s2imap_get(spriteNameMap, spriteName);
}

void ent_initSpriteList()
{
    vecinit(GENERALZONE, entSpriteList.renderList, entitySprite_t, ENT_INITSIZE);

    vecinit(GENERALZONE, animSpriteList.renderList, animatedSprite_t, ENT_INITSIZE);
}



void ent_resetMove()
{
    // vecreset(entityMoveList.entID);
    // vecreset(entityMoveList.dir);
    // vecreset(entityMoveList.speed);
}


/********************VECTOR ENTITY ********************/



int createSpriteFactory(
    void (*setup)(VectorEntity *sprite, SaveDataHandler*),
    void (*think)(VectorEntity *vecEnt, bool isServer, inputCommand_t *inpCmd),
    void (*cleanup)(VectorEntity *sprite),
    void (*initState)(ESDef *esDef),
    void (*processState)(VectorEntity *sprite, ESDef *esDef)
)
    {
    SpriteFactory *spriteFactory = &spriteFactoryList[spriteFactoryCount];
    spriteFactory->typeID = spriteFactoryCount;
    spriteFactory->setup = setup;
    spriteFactory->think = think;
    spriteFactory->cleanup = cleanup;
    spriteFactory->initState = initState;
    spriteFactory->processState = processState;

    return spriteFactoryCount++;
}


VectorEntity* addSprite(int typeID, SaveDataHandler *saveDataReader, bool isServer, bool isPuppet) {
    VectorEntity *vecEnt = NULL;
    int entID = -1;

    for(int i = 0; i < VECTOR_ENTITY_COUNT; i++) {
        if(!vectorEntityList[i].active) {
            vecEnt = &vectorEntityList[i];
            vecEnt->active = true;
            entID = i;
            break;
        }
    }

    if(vecEnt == NULL) {
        com_error(ERR_FATAL, "Exceeded vector entity count\n");
    }

    // int id = getFreeID(&freeList);
    // if(id == -1) return NULL;
    // Sprite *sprite = &spriteList[id];
    // if(saveDataReader == NULL) {
    //     sprite->dbID = lastSpriteDBID++;
    // }
    // else {
    //     sprite->dbID = saveDataReader->spriteDBID;
    // }
    vecEnt->typeID = typeID;
    vecEnt->entID = entID;
    // vecEnt->shouldPuppet = true;
    // vecEnt->think = spriteFactoryList[typeID].think;

    SpriteFactory *spriteFactory = &spriteFactoryList[typeID];
    spriteFactory->setup(vecEnt, saveDataReader);

    NetEntity *netEnt = &netEntityList[entID];
    netEnt->isNew = true;
    netEnt->isRemoved = false;
    netEnt->entID = entID;
    netEnt->isPuppet = isPuppet;

    memset(&vecEnt->posInterpolate, 0, sizeof(positionInterpolate_t));
    memset(&vecEnt->angleInterpolate, 0, sizeof(angleInterpolate_t));

    
    // if(!isPuppet) {
    //     initNetworkEntity(&netEnt->netObj, vecEnt, isServer);
    //     spriteFactory->serializer(vecEnt, &netEnt->netObj);
    // }
    // else {
    //     initNetworkEntity(&netEnt->netObj, vecEnt, isServer);
    //     spriteFactory->puppetSerializer(vecEnt, &netEnt->netObj);
    // }

    // handleNetworkEntity(vecEnt, &netEnt->netObj);
    

    return vecEnt;
}

/********************RAY********************/


void ent_initKillList()
{
    int initSize = ENT_INITSIZE;
    vecinit(GENERALZONE, killIDList.entIDList, int, initSize);
}
int ent_addKillID(int entID)
{
    vecpush(killIDList.entIDList, int, entID);
    return 0;
}
void ent_resetKillList()
{
    vecreset(killIDList.entIDList);
}

/********************WEAPON********************/

void ent_initRayWeaponHandle(rayWeaponHandle_t * weaponHandle, weaponType_t weaponType)
{
    ent_initRayHandleList(&weaponHandle->rayHandleList);
    weaponHandle->weaponType = weaponType;
}

void ent_setRayWeapon(rayWeaponHandle_t *weaponHandle, weaponOnHand_t *weaponOnHand, int entID)
{
    startTimer(&weaponOnHand->currentShootEndTime, 0);
    startTimer(&weaponOnHand->nextShootEndTime, 0);
    weaponOnHand->curAngID = 0;
    weaponOnHand->ammoCount = 100;
    weaponOnHand->rayEntID = ent_addRayEntity(&weaponHandle->rayHandleList, entID);
}

void ent_setRayWeaponEntity(rayWeaponHandle_t *weaponHandle, weaponOnHand_t *weaponOnHand, int entID)
{
    // entityMove_t *rayEnt;
    // rayEnt = &vecget(weaponHandle->rayHandleList.rayEntityList.entList, weaponOnHand->rayEntID);
    // vec3set(rayEnt->pos, moveEnt->pos);
}

void ent_removeRayWeapon(rayWeaponHandle_t *weaponHandle, weaponOnHand_t *weaponOnHand)
{
    startTimer(&weaponOnHand->currentShootEndTime, 0);
    startTimer(&weaponOnHand->nextShootEndTime, 0);
    weaponOnHand->ammoCount = 0;
    ent_removeRayEntity(&weaponHandle->rayHandleList, weaponOnHand->rayEntID);
}

qbool ent_handleRayWeaponShoot(rayWeaponHandle_t *weaponHandle, int entID, weaponOnHand_t *weaponOnHand, float pos[2], float angle)
{
    float dir[3];
    if(weaponOnHand->ammoCount <= 0)
        return qfalse;

    if(!checkTimer(&weaponOnHand->currentShootEndTime))
        return qfalse;

    if(!checkTimer(&weaponOnHand->nextShootEndTime))
        return qfalse;

    startTimer(&weaponOnHand->currentShootEndTime, weaponHandle->weaponType.currentShootDelay);
    startTimer(&weaponOnHand->nextShootEndTime, weaponHandle->weaponType.nextShootDelay);

    // angle += deg2rad(shootAngleList[weaponOnHand->curAngID & (shootAngListLen - 1)]);
    // weaponOnHand->curAngID++;
    weaponOnHand->angle = angle;
    angle = deg2rad(angle);

    vec3setang2(dir, angle);
    vec3mult(dir, 50);

    ent_emitRay(&weaponHandle->rayHandleList, entID, pos, dir);

    // weaponOnHand->ammoCount -= 1;

    return qtrue;
}

void ent_resetRayWeapon(rayWeaponHandle_t *weaponHandle)
{
    ent_resetRayList(&weaponHandle->rayHandleList);
}

void ent_initVectorRayWeapon()
{
    weaponType_t weaponType;

    weaponType.maxAmmoCount = 10;
    weaponType.accuracy = 0;
    weaponType.currentShootDelay = 0;
    weaponType.nextShootDelay = 100;
    weaponType.onHandCapacity = 10;
    weaponType.totalCapacity = 10;

    ent_initRayWeaponHandle(&rayWeaponHandle, weaponType);
}

/********************PICKUPS********************/

// typedef struct PickupList_st
// {
//     vector(entVec_t) posList;
//     vector(entRect_t) rectList;
//     vector(byte) bitmap;
// } PickupList_t;


void ent_initPickupList(pickupList_t *pickupList)
{
    int initSize = ENT_INITSIZE;
    vecinit(GENERALZONE, pickupList->posList, entVec_t, initSize);
    vecinit(GENERALZONE, pickupList->rectList, entRect_t, initSize);
    vecinit(GENERALZONE, pickupList->bitmap, byte, initSize/8);
}

int ent_addPickup(pickupList_t *pickupList, entVec_t posToSet, entRect_t rectToSet)
{
    int id = bm_findEmpty(pickupList->bitmap.arr,
    vecsize(pickupList->bitmap));
    if(id < 0)
    {
        id = vecsize(pickupList->posList);
        vecpushempty(pickupList->posList, entVec_t);
        vecpushempty(pickupList->rectList, entRect_t);
    }

    entVec_t *pickupPos = &vecget(pickupList->posList, id);
    entRect_t *pickupRect = &vecget(pickupList->rectList, id);

    vec3set(pickupPos->pos, posToSet.pos);
    rect2set(pickupRect->rect, rectToSet.rect);

    bm_setBitVal(pickupList->bitmap.arr, id, 1);

    return id;
}

void ent_removePickup(pickupList_t *pickupList, int pickupID)
{
    bm_setBitVal(pickupList->bitmap.arr, pickupID, 0);
}



void ent_init()
{
    translateIDMap = i2imap_init(GENERALZONE);
    ent_initEntList();
    ent_initSpriteList();
    ent_initRayRenderList();
    // ent_initRayList();
    // ent_initMoveList();
    ent_initVectorRayWeapon();
    // ent_initVectorEntityList();
    ent_initKillList();
}

/* Stub implementations for missing functions */

void ent_initEntList(void) {
    // Stub: entity list initialization placeholder
    printf("ent_initEntList: stub called\n");
}

void ent_handleClientLeave(serv_clrep_t *newClRep) {
    // Stub: handle client disconnection
    printf("ent_handleClientLeave: stub called for client %d\n", newClRep->conID);
}

void ent_removeSyncedEntFromClient(int entID, serv_clrep_t *newClRep, int entType) {
    // Stub: remove synced entity from client
    printf("ent_removeSyncedEntFromClient: stub called for entity %d\n", entID);
}

void ent_removeSyncedEntState(int entID, int entType) {
    // Stub: remove synced entity state
    printf("ent_removeSyncedEntState: stub called for entity %d\n", entID);
}

void ent_settleStateDiff(void) {
    // Stub: settle state differences between server and clients
    // This would typically reconcile entity states after network updates
}