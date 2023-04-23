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
    vector(entityChildren_t) children;
    vector(byte) entBm;
} entityList_t;

extern void ent_initEntList();
extern int ent_addEnt(vec3_t pos, int children);
void ent_setChildID(int entID, int i, int id);
int ent_getChildID(int entID, int i);
extern entVec_t *ent_getPos(int entID);
extern entityChildren_t *ent_getChildren(int entID);
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

typedef struct entitySerialize_st
{
    int entID;
    void (*read)(struct entitySerialize_st *entSerialize, bitstream_t *bs, byte *bm);
    void (*write)(struct entitySerialize_st *entSerialize, bitstream_t *bs, byte *bm);

    byte stateFlags[32];
    int stateLen;

} entitySerialize_t;

typedef struct entitySerialiseList_st
{
    vector(entitySerialize_t) entSerialize;
    vector(byte) entBm;

    i2imap_t *entSerializeMap;

} entitySerializeList_t;

extern void ent_initSerializeList();
extern int ent_addSerialize(int entID, int stateSize,
        void (*read)(entitySerialize_t *entSerialize, bitstream_t *bs, byte *bm),
        void (*write)(entitySerialize_t *entSerialize, bitstream_t *bs, byte *bm));
extern void ent_removeSerialize(int entID);
extern void ent_readSerialize(bitstream_t *bs, cl_entStateRecordList_t *entStateRecordList, inputCommandList_t *inputCommandList);
extern void ent_writeAllSerialize(bitstream_t *bs, netcon_t *con, cl_entStateRecordList_t *entStateList);
extern void ent_ackSerialize(netcon_t *con, cl_entStateRecordList_t *entStateList);
extern entitySerialize_t *ent_getSerialize(int serializeID);
extern entitySerialize_t *ent_getSerializeFromEntID(int entID);
extern void ent_addPermanent(int entID, netcon_t *con, cl_entStateRecordList_t *entStateRecordList);

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
    vector(entitySprite_t) entSprite;
    vector(byte) entBm;

    entitySprite_t *renderSpriteList;
    int renderSpriteCount;

} entitySpriteList_t;

extern void ent_initSpriteList();
int ent_addSprite(int entID, char *spriteName, float pos[2], float rect[4], float angle);
extern entitySprite_t *ent_getSprite(int spriteID);
extern entitySprite_t *ent_getSpriteFromEnt(int entID);
extern void ent_handleSprites();

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
    vector(animatedSprite_t) animSprite;
    vector(byte) entBm;

    animatedSprite_t *renderSpriteList;
    int renderSpriteCount;

} animatedSpriteList_t;

int ent_addAnimSprite(int entID, char *spriteName, float pos[2], float rect[4], float angle);
animatedSprite_t *ent_getAnimSprite(int spriteID);
animatedSprite_t *ent_getAnimSpriteFromEnt(int entID);

/********************ENT CREATOR********************/

#define ENT_CREATORSIZE 10

typedef struct entCreator_st
{
    int (*createEnt)(void *);
} entCreator_t;

extern void ent_addCreator( int (*createEnt)(void *));

/********************INIT********************/

extern void ent_init();
extern void ent_initRecordList(cl_entStateRecordList_t *entStateRecordList);
extern int ent_createEnt(int createId, void *data);

#endif