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
    ENTCMD_END
} ent_netcmd_e;

typedef struct evec3_st
{
    float vec[3];
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

typedef struct entityMove_st
{
    float pos[3];
    float dir[3];
    float rect[4];
    float speed;
} entityMove_t;

typedef struct entityMoveList_st
{
    vector(entityMove_t) entMove;
    vector(byte) entBm;

} entityMoveList_t;

extern void ent_initMoveList();
extern int ent_addMove(int entID, vec3_t pos, vec3_t dir, rect2_t rect, float speed);
extern entityMove_t *ent_getMove(int moveID);
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

typedef struct entityRecord_st
{
    int entID;
    recentStreamRecord_t recentRecord;
} entityRecord_t;

extern void ent_initSerialize(entityRecord_t *entSerialize, int stateSize);
extern void ent_setStateFlags(int serializerID, int entID, int i, byte flag);
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
extern void ent_initRecordList(cl_entStateRecordList_t *entStateRecordList);
extern int ent_createEnt(int createId, void *);



/********************SHOOTER ENTITY********************/
typedef struct movableEntity_st
{
    float pos[3];
    float bound[4];
    entityMove_t move;
} movableEntity_t;


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

typedef struct vectorEntity_st
{
    movableEntity_t movable;
    entitySprite_t sprite;

} vectorEntity_t;

typedef struct vectorEntityList_st
{
    vector(movableEntity_t) movableList;
    vector(animatedSprite_t) animSpriteList;
    vector(endTimer_t) shootTimerList;
    vector(positionInterpolate_t) posInterpolateList;
    vector(angleInterpolate_t) angleInterpolateList;
    vector(int) moveIDList;
    vector(byte) bitmap;

    i2imap_t *mainEntMap;

} vectorEntityList_t;


extern void ent_initVectorEntityList();
extern int ent_addVectorEntity();
extern void ent_removeVectorEntity(int entID);


typedef struct entityStateBitmap_st
{
    byte *state;
} entityStateBitmap_t;

typedef struct clientEntityRecordList_st
{
    int conID;
    
    vector(entityRecord_t) recordList;
    vector(byte) bitmap;
    
    quickStreamRecord_t newEntRecord;

} clientEntityRecordList_t;


typedef struct entitySerializer_st
{
    int (*readState)(int, bitstream_t *, byte *);
    int (*writeState)(int entID, bitstream_t *, byte *stateBm, int conID);

    // int (*readInitParam)(int, bitstream_t *);
    int (*readInitParam)(bitstream_t *);
    int (*applyInitParam)();
    int (*writeInitParam)(int, int, bitstream_t *);

    vector(clientEntityRecordList_t) clientRecordList;
    vector(byte) clientBitmap;

    vector(entityStateBitmap_t) entityStateList;
    vector(byte) entityBitmap;

    int stateLen;

    i2imap_t *translateIDMap;
    i2imap_t *conIDMap;

    int clientEntID;

} entitySerializer_t;


typedef struct entitySerializerList_st
{
    entitySerializer_t *list;
    int length;
} entitySerializerList_t;


extern void ent_setSerializer(
    entitySerializer_t *entSerialize,
    int stateSize,
    int (*readState)(int, bitstream_t *, byte *),
    int (*writeState)(int entID, bitstream_t *, byte *stateBm, int conID),
    int (*readInitParam)(bitstream_t *),
    int (*applyInitParam)(),
    int (*writeInitParam)(int, int, bitstream_t *)
    );


extern void ent_readSerializerList(int conID, netcon_t *con, bitstream_t *bs);
extern void ent_writeSerializerList(int conID, netcon_t *con, bitstream_t *bs);
// extern void ent_writeSerializerList(netcon_t *, bitstream_t *bs);
// extern void ent_ackSerializerList(bitstream_t *bs);
extern void ent_ackSerializerList(int conID, netcon_t *con, bitstream_t *bs);

extern void ent_addSyncedEntToClient(int entID, int conID, netcon_t *con, int entType);

extern intPair_t ent_setupEntityForClient(int conID, netcon_t *con);
extern void ent_handleClientJoin(int, netcon_t *);

extern void ent_setupSyncedEnt(int entID, int entType);

extern void ent_cleanupSerializerState();

/********************SHOOTER ENTITY********************/
typedef struct rayList_st
{
    vector(float) xList;
    vector(float) yList;
    vector(float) xDirList;
    vector(float) yDirList;
} rayList_t;


typedef struct renderRayList_st
{
    vector(float) xList;
    vector(float) yList;
    vector(float) xDirList;
    vector(float) yDirList;
    vector(unsigned long int) endTimeList;

} renderRayList_t;


/********************ENTITY INDEX********************/

extern entityList_t entList;
extern vectorEntityList_t vectorEntityList;

extern entitySerializerList_t entSerializerList;


extern entitySpriteList_t entSpriteList;
extern animatedSpriteList_t animSpriteList;

extern rayList_t rayList;
extern renderRayList_t renderRayList;

#endif