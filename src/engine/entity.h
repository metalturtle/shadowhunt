#ifndef ENTITY_H
#define ENTITY_H

#include "../basic/basic.h"
#include "../basic/world_def.h"
#include "engine.h"

//init
//add
//get
//update
//delete
//cleanup

#define ENT_INITSIZE 32

typedef enum
{
    ENTCMD_NEW,
    ENTCMD_DEL,
    ENTCMD_STATE,
    ENTCMD_REMOVE,
    ENTCMD_END
} ent_netcmd_e;

typedef struct evec3_st
{
    float pos[3];
} entVec_t;

typedef struct erect_st
{
    float rect[4];
} entRect_t;

typedef struct entityChildren_st
{
    int *children;
    int count;
} entityChildren_t;

/********************ENTITY********************/
typedef struct entityList_st
{
    vector(entVec_t) pos;
    // vector(entityChildren_t) children;
    vector(byte) entBm;
} entityList_t;

extern void ent_initEntList();
extern int ent_addEnt(vec3_t pos, int children);
// void ent_setChildID(int entID, int i, int id);
// int ent_getChildID(int entID, int i);
// extern entVec_t *ent_getPos(int entID);
// extern entityChildren_t *ent_getChildren(int entID);
extern void ent_remove(int entID);

/********************MOVEMENT********************/

// typedef struct entityMove_st
// {
//     float pos[3];
//     float dir[3];
//     float rect[4];
//     float speed;
// } entityMove_t;

// typedef struct entityMoveList_st
// {
//     vector(entityMove_t) entMove;
//     vector(byte) entBm;

// } entityMoveList_t;

extern void ent_initMoveList();
extern int ent_addMove(int entID, vec3_t pos, vec3_t dir, rect2_t rect, float speed);
// extern entityMove_t *ent_getMove(int moveID);
extern void ent_resetMove();
extern void ent_runMove();

/********************RUN FUNCTION********************/

typedef struct entityThink_st
{
    int entID;

    inputCommandList_t *inputCommandList;

    void (*think)(struct entityThink_st *entThink);

} entityThink_t;

typedef struct entityThinkList_st
{
    vector(entityThink_t) entThink;
    vector(byte) entBm;
} entityThinkList_t;

extern void ent_initThinkList();
extern int ent_addThink(int entID, inputCommandList_t *inputCommandList, void (*think)(entityThink_t *));
extern void ent_removeThink(int entID);
extern void ent_runAllThink();

/********************SERIALIZE********************/

typedef struct ESVar_st {
    void *buf;
    uint64_t timestamp;
} ESVar;

typedef struct ESVarQueue_st {
    void *varList;
    uint64_t *tsList;
    int listSize;
    int elemSize;
    int start;
    int end;
    void (*interpolate)(void *obj, ESVar *e1, ESVar *e2, uint64_t timestamp);
} ESVarQueue;

enum ESDefState {
    ESDEF_COUNT,
    ESDEF_ADD,
    ESDEF_WRITETOESV,
    ESDEF_WRITETOENT,
    ESDEF_BSWRITESTATE,
    ESDEF_BSREADSTATE,
    ESDEF_BSTRACKSTATE,
    ESDEF_BSRESETSTATE
};

typedef struct ESDiff_st {
    // int *snapshotIndexes;
    int snapshotIndex;
    bool *shouldSend;
} ESDiff;

typedef struct ESDef_st {
	ESVarQueue *stateList;
	int listSize;
    int snapshotCount;
    int curDef;
    uint64_t timestamp;
    enum ESDefState curState;
    ESDiff *esDiff;
    bitstream_t *bs;
} ESDef;


typedef struct NetEntry_st {
    byte *buf;
    int size;
    bool stateSet;
    int id;
    struct NetEntry_st *next;
} NetEntry;

typedef enum netObjState_st {
    WRITE_INIT,
    READ_INIT,
    WRITE,
    READ,
    TRACK_WRITE_DIFF,
    TRACK_READ_DIFF,
    SETTLE_DIFF
} netObjState;

typedef struct NetObj_st
{
    netObjState curState;
    NetEntry *entryHead;
    NetEntry *entryTail;
    NetEntry *curEntry;
    bitstream_t *bs;
    int stateCount;
    int conID;
} NetObj;



typedef struct entityRecord_st
{
    int entID;
    recentStreamRecord_t recentRecord;
    ESDiff esDiff;
    bool isNotPuppet;
    bool active;
    // NetObj netObj;
} entityRecord_t;

// typedef struct clientEntityRecordList_st
// {
//     int conID;
//     // int entID;
//     // byte *state;

//     vector(entityRecord_t) recordList;
//     vector(byte) bitmap;
    
//     quickStreamRecord_t newEntRecord;
//     quickStreamRecord_t removeEntRecord;

// } clientEntityRecordList_t;

typedef struct worldSnapshot_st {
    vector(entityRecord_t) recordList;
    quickStreamRecord_t newEntRecord;
    quickStreamRecord_t removeEntRecord;
    bool active;
} worldSnapshot_t;

// typedef struct entityRecord_st
// {
//     int entID;
//     recentStreamRecord_t recentRecord;
//     // NetObj netObj;
// } entityRecord_t;

extern void ent_initSerialize(entityRecord_t *entSerialize, int stateSize);
// extern void ent_setStateFlags(int serializerID, int entID, int i, byte flag);
// typedef struct entitySerialiseList_st
// {
//     vector(entityRecord_t) entSerialize;
//     vector(byte) entBm;

//     i2imap_t *entSerializeMap;

// } entitySerializeList_t;

// extern void ent_initSerializeList();
// extern int ent_addSerialize(int entID, int stateSize,
//         void (*read)(entitySerialize_t *entSerialize, bitstream_t *bs, byte *bm),
//         void (*write)(entitySerialize_t *entSerialize, bitstream_t *bs, byte *bm));
// extern void ent_removeSerialize(int entID);
// extern void ent_readSerialize(bitstream_t *bs, cl_entStateRecordList_t *entStateRecordList, inputCommandList_t *inputCommandList);
// extern void ent_writeAllSerialize(bitstream_t *bs, netcon_t *con, cl_entStateRecordList_t *entStateList);
// extern void ent_ackSerialize(netcon_t *con, cl_entStateRecordList_t *entStateList);
// extern entitySerialize_t *ent_getSerialize(int serializeID);
// extern entitySerialize_t *ent_getSerializeFromEntID(int entID);
// extern void ent_addPermanent(int entID, netcon_t *con, cl_entStateRecordList_t *entStateRecordList);

/********************SPRITE METADATA********************/

typedef enum {
    SPRITE_TYPE_STATIC,
    SPRITE_TYPE_ANIM,
} sprite_type_e;

#ifdef __cplusplus
extern "C" {
#endif

extern void sprite_init();
extern void sprite_add(char *spriteName, int id, int type);
extern int sprite_getID(char *spriteName, int type);

#ifdef __cplusplus
}
#endif

/********************ENT SPRITE********************/
typedef struct entitySprite_st
{
    int entID;
    int texID;
    float pos[3];
    float rect[4];
    float angle;
} entitySprite_t;
typedef struct entitySpriteList_st
{
    vector(entitySprite_t) renderList;
    // entitySprite_t *renderSpriteList;
    // int renderSpriteCount;

} entitySpriteList_t;

extern void ent_initSpriteList();


/********************ANIMATED SPRITE********************/
typedef struct animatedSprite_st
{
    int entID;
    int texID;
    float curSprite;
    float pos[3];
    float rect[4];
    float angle;
    
} animatedSprite_t;

typedef struct animatedSpriteList_st
{
    vector(animatedSprite_t) renderList;

} animatedSpriteList_t;


// int ent_addAnimSprite(int entID, char *spriteName, float pos[2], float rect[4], float angle);
// animatedSprite_t *ent_getAnimSprite(int spriteID);
// animatedSprite_t *ent_getAnimSpriteFromEnt(int entID);

/********************ENT CREATOR********************/

#define ENT_CREATORSIZE 10


/********************INIT********************/

extern void ent_init();
// extern void ent_initRecordList(cl_entStateRecordList_t *entStateRecordList);
extern int ent_createEnt(int createId, void *);



// /********************SHOOTER ENTITY********************/
// typedef struct movableEntity_st
// {
//     float pos[3];
//     float bound[4];
//     entityMove_t move;
// } movableEntity_t;

/********************SHOOTER ENTITY********************/
typedef struct emittedRayList_st
{
    vector(int) entIDList;
    vector(float) xList;
    vector(float) yList;
    vector(float) xDirList;
    vector(float) yDirList;
} emittedRayList_t;


typedef struct renderRayList_st
{
    vector(float) xList;
    vector(float) yList;
    vector(float) xDirList;
    vector(float) yDirList;
    vector(unsigned long int) endTimeList;

} renderRayList_t;


typedef struct rayEntityList_st
{
    vector(int) entIDList;
    vector(int) entList;
    vector(byte) entBitmap;
} rayEntityList_t;


typedef struct rayHitList_st
{
    vector(int) fromList;
    vector(int) toList;
    vector(float) uList;
} rayHitList_t;


typedef struct rayHandleList_st
{
    emittedRayList_t emittedRayList;
    rayEntityList_t rayEntityList;
    rayHitList_t rayHitList;
} rayHandleList_t;


extern void ent_initRayHandleList(rayHandleList_t *rayHandleList);
extern int ent_addRayEntity(rayHandleList_t *rayHandleList, int entID);
extern void ent_removeRayEntity(rayHandleList_t *rayHandleList, int entID);
extern int ent_emitRay(rayHandleList_t *rayHandleList, int entID, float pos[2], float dir[2]);
extern void ent_setHitEntity(rayHandleList_t *rayHandleList, int rayID, int fromID, int toID);
extern void ent_resetHitEntityList(rayHandleList_t *rayHandleList);
extern void ent_resetRayList(rayHandleList_t *rayHandleList);

/********************WEAPON********************/

typedef struct weaponOnHand_st
{
    int ammoCount;
	endTimer_t currentShootEndTime;
	endTimer_t nextShootEndTime;
    int rayEntID;
    int curAngID;
    float angle;
} weaponOnHand_t;

typedef struct weaponType_st
{
    int maxAmmoCount;
    int accuracy;
    int currentShootDelay;
    int nextShootDelay;
    int onHandCapacity;
    int totalCapacity;
} weaponType_t;

typedef struct rayWeaponHandle_st
{
    weaponType_t weaponType;
    rayHandleList_t rayHandleList;
} rayWeaponHandle_t;

extern void ent_initRayWeaponHandle(rayWeaponHandle_t * weaponHandle, weaponType_t weaponType);
extern qbool ent_handleRayWeaponShoot(rayWeaponHandle_t *weaponHandle, int entID, weaponOnHand_t *weaponOnHand, float pos[2], float angle);
extern void ent_setRayWeapon(rayWeaponHandle_t *weaponHandle, weaponOnHand_t *weaponOnHand, int entID);
extern void ent_resetRayWeapon(rayWeaponHandle_t *weaponHandle);
extern void ent_setRayWeaponEntity(rayWeaponHandle_t *weaponHandle, weaponOnHand_t *weaponOnHand, int entID);
extern void ent_removeRayWeapon(rayWeaponHandle_t *weaponHandle, weaponOnHand_t *weaponOnHand);

/********************ENTITY INDEX********************/

typedef struct positionInterpolate_st
{
    float pos[3][2];
    long timestamp[3];
    int last;
    int first;
} positionInterpolate_t;

typedef struct angleInterpolate_st
{
    float angle[3];
    long timestamp[3];
    int last;
    int first;
} angleInterpolate_t;

// typedef struct vectorEntity_st
// {
//     entVec_t entPos;
//     entityMove_t movable;
//     entitySprite_t sprite;

// } vectorEntity_t;

// typedef struct vectorEntityList_st
// {
//     vector(entVec_t) posList;
//     vector(entityMove_t) movableList;
//     vector(animatedSprite_t) animSpriteList;
//     vector(endTimer_t) shootTimerList;
//     vector(positionInterpolate_t) posInterpolateList;
//     vector(angleInterpolate_t) angleInterpolateList;
//     vector(int) healthList;
//     vector(int) moveIDList;
//     // vector(int) rayEntIDList;
//     vector(weaponOnHand_t) weaponOnHandList;
//     vector(int) weaponShotList;
//     vector(byte) bitmap;

//     i2imap_t *mainEntMap;

// } vectorEntityList_t;


typedef struct VectorEntity_st {
    int entID;
    int typeID;
    SDL_FRect rect;
    SDL_FPoint pos;
    cpBody *collision;
    SDL_FPoint dir;
    float health;
    animatedSprite_t animSprite;
    bool active;
    int externalID;

    positionInterpolate_t posInterpolate;
    angleInterpolate_t angleInterpolate;
} VectorEntity;

typedef struct NetEntity_st {
    bool isNew;
    bool isRemoved;
    bool isPuppet;
    // NetObj netObj;
    ESDef esDef;
    int entID;
    int clientOwner;
} NetEntity;

// positionInterpolate_t posInterpolate;
// angleInterpolate_t angleInterpolate;

#define VECTOR_ENTITY_COUNT 8

extern VectorEntity vectorEntityList[VECTOR_ENTITY_COUNT];

extern i2imap_t *mainEntMap;

// struct Sprite_st {
//     SDL_FRect rect;
//     SDL_FPoint center;
//     SDL_FPoint velocity;
//     float scale;
//     float rotate;
//     int textureID;
//     float health;
//     void (*think)(struct Sprite_st *sprite);
//     int entID;
//     int typeID;
//     int spriteID;
//     b2BodyId collisionID;
//     int dbID;
// };
// typedef struct Sprite_st Sprite;


extern void ent_initVectorEntityList();
// extern int ent_addVectorEntity(bool isServer);
// extern void ent_removeVectorEntity(int entID);


/********************PICKUPS********************/

typedef struct PickupList_st
{
    vector(entVec_t) posList;
    vector(entRect_t) rectList;
    vector(byte) bitmap;
} pickupList_t;


extern void ent_initPickupList(pickupList_t *pickupList);
extern int ent_addPickup(pickupList_t *pickupList, entVec_t pos, entRect_t rect);
extern void ent_removePickup(pickupList_t *pickupList, int pickupID);

/********************KILLED ENTITY********************/

typedef struct killedEntityList_st
{
    vector(animatedSprite_t) animSpriteList;
    vector(byte) bitmap;

    i2imap_t *mainEntMap;

} killedEntityList_t;

extern void ent_initKilledEntityList();
extern int ent_addKilledEntity();
extern void ent_removeKilledEntity(int entID);


/********************KILLED ENTITY********************/

typedef struct killIDList_st
{
    vector(int) entIDList;

} killIDList_t;

extern void ent_initKillList();
extern int ent_addKillID(int entID);
extern void ent_resetKillList();

typedef struct entityStateBitmap_st
{
    byte *state;
} entityStateBitmap_t;

// typedef struct clientEntityRecordList_st
// {
//     int conID;
//     // int entID;
//     // byte *state;

//     vector(entityRecord_t) recordList;
//     vector(byte) bitmap;
    
//     quickStreamRecord_t newEntRecord;
//     quickStreamRecord_t removeEntRecord;

// } clientEntityRecordList_t;


// typedef struct entitySerializer_st
// {
//     int (*readState)(int, bitstream_t *, byte *);
//     int (*writeState)(int entID, bitstream_t *, byte *stateBm, int conID);

//     // int (*readInitParam)(int, bitstream_t *);
//     int (*readInitParam)(bitstream_t *);
//     int (*applyInitParam)(void);
//     int (*writeInitParam)(int, int, bitstream_t *);
//     void (*removeEntity)(int);

//     vector(clientEntityRecordList_t) clientRecordList;
//     vector(byte) clientBitmap;

//     vector(entityStateBitmap_t) entityStateList;
//     vector(byte) entityBitmap;

//     int stateLen;

//     i2imap_t *translateIDMap;
//     i2imap_t *conIDMap;


// } entitySerializer_t;


// typedef struct entitySerializerList_st
// {
//     entitySerializer_t *list;
//     int length;
// } entitySerializerList_t;


// extern void ent_setSerializer(
//     entitySerializer_t *entSerialize,
//     int stateSize,
//     int (*readState)(int, bitstream_t *, byte *),
//     int (*writeState)(int entID, bitstream_t *, byte *stateBm, int conID),
//     int (*readInitParam)(bitstream_t *),
//     int (*applyInitParam)(void),
//     int (*writeInitParam)(int, int, bitstream_t *),
//     void (*removeEntity)(int)
//     );


extern void ent_readSerializerList(serv_clrep_t *newClRep, bitstream_t *bs);
extern void ent_writeSerializerList(serv_clrep_t *newClRep, bitstream_t *bs);
void ent_ackSerializerList( serv_clrep_t *newClRep, bitstream_t *bs);
// extern void ent_writeSerializerList(netcon_t *, bitstream_t *bs);
// extern void ent_ackSerializerList(bitstream_t *bs);
// extern void ent_ackSerializerList(int conID, netcon_t *con, bitstream_t *bs);


extern intPair_t ent_setupEntityForClient(int conID, netcon_t *con);
extern intPair_t ent_removeEntityFromClient(int conID, netcon_t *con);
// extern void ent_handleClientJoin(int, serv_clrep_t *);
// extern void ent_handleClientLeave(int, netcon_t *);
void ent_handleClientLeave(serv_clrep_t *newClRep);


extern void ent_addSyncedEntState(int entID, int entType);
// extern void ent_addSyncedEntToClient(int entID, int conID, netcon_t *con, int entType);
// extern void ent_addSyncedEntToClient(int entID, serv_clrep_t *newClRep, int entType);

// extern void ent_cleanupSerializerState();

extern void ent_removeSyncedEntState(int entID, int entType);
// extern void ent_removeSyncedEntFromClient(int entID, int conID, netcon_t *con, int entType);
void ent_removeSyncedEntFromClient(int entID, serv_clrep_t *newClRep, int entType);

/********************ENTITY INDEX********************/

extern entityList_t entList;
// extern vectorEntityList_t vectorEntityList;

// extern entitySerializerList_t entSerializerList;


extern entitySpriteList_t entSpriteList;
extern animatedSpriteList_t animSpriteList;

extern renderRayList_t renderRayList;
// extern emittedRayList_t emittedRayList;
// extern rayEntityList_t rayEntityList;
// extern rayHitList_t rayHitList;
extern killIDList_t killIDList;

extern rayWeaponHandle_t rayWeaponHandle;


extern void initNetworkEntity(NetObj *netObj, VectorEntity *ent, bool isWrite);
extern void handleNetworkEntity(VectorEntity *ent, NetObj *netObj) ;
extern void setupNetworkEntity(NetObj *netObj, VectorEntity *ent, bitstream_t *bs, int state) ;
// extern void setNetworkEntityChanged(NetObj *netObj, byte *stateBm, int size);
extern void setNetworkEntityChanged(NetObj *netObj, byte *stateBm);

// extern b2BodyId createBox(float x, float y, float width, float height) ;
extern void createEdgeBoundary(float x, float y, float width, float height) ;
extern cpBody* createCircle(float x, float y, float radius) ;
extern void moveEntityWithCollision(VectorEntity *vecEnt, float deltaTime);
extern void moveCollision(VectorEntity *vecEnt, float deltaTime) ;
extern void handleMove(SDL_FPoint *pos, SDL_FPoint *dir);
extern float check_intersection(float pos[2], float dir[2], float wall[4]);

typedef struct SaveDataHandler_st {
    // sqlite3_stmt *stmt;
    int spriteDBID;
} SaveDataHandler;

typedef struct SpriteFactory_st {
    int typeID;

    // vector(clientEntityRecordList_t) clientRecordList;
    // vector(byte) clientBitmap;

    // vector(entityStateBitmap_t) entityStateList;
    // vector(byte) entityBitmap;

    int stateLen;

    // i2imap_t *conIDMap;

    void (*setup)(VectorEntity *sprite, SaveDataHandler*);
void (*think)(VectorEntity *vecEnt, bool isServer, inputCommand_t *inpCmd);
    void (*cleanup)(VectorEntity *sprite);
    void (*initState)(ESDef *esDef);
    void (*processState)(VectorEntity *sprite, ESDef *esDef);
} SpriteFactory;

extern int createSpriteFactory(
    void (*setup)(VectorEntity *sprite, SaveDataHandler*),
    void (*think)(VectorEntity *vecEnt, bool isServer, inputCommand_t *inpCmd),
    void (*cleanup)(VectorEntity *sprite),
    void (*initState)(ESDef *esDef),
    void (*processState)(VectorEntity *sprite, ESDef *esDef)
);
extern VectorEntity* addSprite(int typeID, SaveDataHandler *saveDataReader, bool isServer, bool isPuppet);

extern SpriteFactory spriteFactoryList[100];

typedef struct Player_st {
    int rayEntID;
    int weaponShot;
    void (*think)(struct VectorEntity_st *sprite);
    endTimer_t shootTimer;
    weaponOnHand_t weaponOnHand;
    bool active;
} PlayerData;

extern PlayerData playerDataList[8];

typedef struct Puppet_st {
    positionInterpolate_t posInterpolate;
    angleInterpolate_t angleInterpolate;
    bool active;
} Puppet;

// extern Puppet puppetList[8];

extern void resetNetworkStateBits(NetObj *netObj);

extern NetEntity netEntityList[VECTOR_ENTITY_COUNT];

extern void resetNetEnt();
extern void ent_initializeClient(serv_clrep_t *newClRep);
extern void ent_settleStateDiff();
extern void ent_initRayRenderList();
extern VectorEntity* getVectorEntity(int spriteID);
extern void handleNetEntry(NetObj *netObj, void *ptr, int size, VectorEntity *vecEnt);

extern void addESVar(ESDef *esDef, int elemSize, void (*interpolate)(void *obj, ESVar *e1, ESVar *e2, uint64_t timestamp));
extern void handleESVar(ESDef *esDef, void *ptr);

/* ESVarQueue functions */
extern void initESQueue(ESVarQueue *esQueue, int elemSize, int listSize, void (*interpolate)(void *obj, ESVar *e1, ESVar *e2, uint64_t timestamp));
extern bool isESVQFull(ESVarQueue *esQueue);
extern bool isESVQEmpty(ESVarQueue *esQueue);
extern void popESVQueue(ESVarQueue *esQueue);
extern void getESVQLast(ESVarQueue *esQueue, void *ptr);
extern void addESVQueue(ESVarQueue *esQueue, void *val, long timestamp);

/* ESDef functions */
extern void initESDef(ESDef *esDef, int listSize, int snapshotCount);
extern void setupESDef(ESDef *esDef, enum ESDefState state, ESDiff *esDiff);

typedef struct worldSnapshotList_st {
    vector(worldSnapshot_t) list;
} worldSnapshotList_t;

extern worldSnapshotList_t worldSnapshotList;

extern void entSys_init();

extern void entSys_updateServer();

extern void serv_clearInputs();

extern void serv_sendPacketAll();

extern void entSys_updateClient();

extern int neEntSys_initSnapshot();

extern void netEntSys_init();
#endif