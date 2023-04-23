#include "entity.h"
#include "engine.h"
#include "../movement/movement.h"

int mainEnt;

entCreator_t entCreatorList[ENT_CREATORSIZE];
int entCreatorLen = 0;

entityList_t entList;
entityThinkList_t entThinkList;
entitySerializeList_t entSerializeList;
entitySpriteList_t entSpriteList;
animatedSpriteList_t animSpriteList;
s2imap_t *spriteNameMap;
s2imap_t *animSpriteNameMap;

entityMoveList_t entMoveList;

int entIDTranslate[20];

/********************ENTITY********************/

void ent_initEntList()
{
    int initSize = ENT_INITSIZE;
    vecinit(GENERALZONE, entList.pos, entVec_t, initSize);
    vecinit(GENERALZONE, entList.children, entityChildren_t, initSize);
    vecinit(GENERALZONE, entList.entBm, byte, initSize/8);
    zmemset(entList.entBm.arr, 0, entList.entBm.capacity);

    for(int i = 0; i < 20; i++)
    {
        entIDTranslate[i] = -1;
    }
}

int ent_addEnt(vec3_t pos, int childCount)
{
    int id;
    entityChildren_t entChild;
    entVec_t entPos;

    vec3set(entPos.vec, pos);

    if(childCount > 0) 
        entChild.children = (int *) zidmalloc(GENERALZONE, sizeof(int) * childCount);
    else
        entChild.children = NULL;
    entChild.count = childCount;
    zmemset(entChild.children, 0, sizeof(int) * childCount);

    id = bm_findEmpty(entList.entBm.arr, entList.pos.size);
    
    if(id < 0)
    {
        vecpush(entList.pos, entVec_t, entPos);
        vecpush(entList.children, entityChildren_t, entChild);
        id = entList.pos.size - 1;
    }
    else {
        vecset(entList.pos, id, entPos);
        vecset(entList.children, id, entChild);
    }

    if((entList.entBm.size*8) == entList.pos.size) {
        vecpush(entList.entBm, byte, 0);
    }

    bm_setBitVal(entList.entBm.arr, id, 1);

    
    return id;
}

entVec_t *ent_getPos(int entID)
{
    return &vecget(entList.pos, entID);
}

entityChildren_t *ent_getChildren(int entID)
{
    return &vecget(entList.children, entID);
}

void ent_setChildID(int entID, int i, int id)
{
    vecget(entList.children, entID).children[i] = id;
}

int ent_getChildID(int entID, int i)
{
    return vecget(entList.children, entID).children[i];
}

void ent_removeEnt(int entID)
{
    entityChildren_t *entChildren;

    bm_setBitVal(entList.entBm.arr, entID, 0);
    
    entChildren = ent_getChildren(entID);
    if(entChildren->children)
    {
        zidfree(entChildren->children);
        entChildren->count = 0;
    }
}

/********************MOVEMENT********************/

void ent_initMoveList()
{
    int initSize = ENT_INITSIZE;
    vecinit(GENERALZONE, entMoveList.entMove, entityMove_t, initSize);
    vecinit(GENERALZONE, entMoveList.entBm, byte, initSize/8);
    zmemset(entMoveList.entBm.arr, 0, initSize/8);
}

int ent_addMove(int entID, vec3_t pos, vec3_t dir, rect2_t rect, float speed)
{
    entityMove_t entMove;

    vec3set(entMove.pos, pos);
    vec3set(entMove.dir, dir);
    rect2set(entMove.rect, rect);
    entMove.speed = speed;

    int id = bm_findEmpty(entMoveList.entBm.arr, vecsize(entMoveList.entMove));
    if(id < 0)
    {
        id = vecsize(entMoveList.entMove);
        vecpush(entMoveList.entMove, entityMove_t, entMove);
    }
    else {
        vecset(entMoveList.entMove, id, entMove);
    }

    bm_setBitVal(entMoveList.entBm.arr, id, 1);

    physics_addBody(&vecget(entMoveList.entMove, id));

    return id;
}

entityMove_t *ent_getMove(int moveID)
{
    return &vecget(entMoveList.entMove, moveID);
}

void ent_runMove()
{
    physics_run();
}

void ent_resetMove()
{
    // vecreset(entityMoveList.entID);
    // vecreset(entityMoveList.dir);
    // vecreset(entityMoveList.speed);
}

/********************RUN FUNCTION********************/

void ent_initThinkList()
{
    int initSize = ENT_INITSIZE;
    vecinit(GENERALZONE, entThinkList.entThink, entityThink_t, initSize);
    vecinit(GENERALZONE, entThinkList.entBm, byte, initSize/8);
    memset(entThinkList.entBm.arr, 0, initSize/8);
}

int ent_addThink(int entID, inputCommandList_t *inputCommandList, void (*think)(entityThink_t *entThink))
{
    int id;

    entityThink_t entThink;
    entThink.entID = entID;
    entThink.think = think;
    entThink.inputCommandList = inputCommandList;

    id = bm_findEmpty(entThinkList.entBm.arr, entThinkList.entThink.size);

    if(id < 0)
    {
        vecpush(entThinkList.entThink, entityThink_t, entThink);
        id = vecsize(entThinkList.entThink) - 1;
    }
    else {
        vecset(entThinkList.entThink, id, entThink);
    }

    if((entThinkList.entBm.size*8) == entThinkList.entThink.size) {
        vecpush(entThinkList.entBm, byte, 0);
    }

    bm_setBitVal(entThinkList.entBm.arr, id, 1);

    return id;
}

entityThink_t *ent_getThinkFromEntID(int entID)
{
    entityThink_t *entThink;

    for(int i = 0; i < vecsize(entThinkList.entThink); i++)
    {
        if(!bm_getBitVal(entThinkList.entBm.arr, i))
            continue;

        entThink = &vecget(entThinkList.entThink, i);
        if(entThink->entID == entID)
            return entThink;
    }

    return NULL;
}

void ent_runAllThink()
{
    entityThink_t *entThink;


    for(int i = 0; i < vecsize(entThinkList.entThink); i++)
    {
        if(!bm_getBitVal(entThinkList.entBm.arr, i))
        {
            continue;
        }
        
        entThink = &vecget(entThinkList.entThink, i);

        entThink->think(entThink);
    }
}

void ent_removeThink(int entID)
{
    bm_setBitVal(entThinkList.entBm.arr, entID, 0);
}

/********************SERIALIZE********************/

void ent_initSerializeList()
{
    int initSize = ENT_INITSIZE;
    vecinit(GENERALZONE, entSerializeList.entSerialize, entitySerialize_t, initSize);
    vecinit(GENERALZONE, entSerializeList.entBm, byte, initSize/8);
    memset(entSerializeList.entBm.arr, 0, initSize/8);

    entSerializeList.entSerializeMap = i2imap_init(GENERALZONE);
}

int ent_addSerialize(int entID, int stateSize,
    void (*read)(entitySerialize_t *entSerialize, bitstream_t *bs, byte *bm),
    void (*write)(entitySerialize_t *entSerialize, bitstream_t *bs, byte *bm))
{
    int id = bm_findEmpty(entSerializeList.entBm.arr, entSerializeList.entSerialize.size);

    entitySerialize_t entSerialize;
    entSerialize.entID = entID;
    entSerialize.read = read;
    entSerialize.write = write;
    entSerialize.stateLen = stateSize;

    if(id < 0)
    {
        vecpush(entSerializeList.entSerialize, entitySerialize_t, entSerialize);
        id = vecsize(entSerializeList.entSerialize) - 1;
    }
    else {
        vecset(entSerializeList.entSerialize, id, entSerialize);
    }
    
    if((entSerializeList.entBm.size*8) == entSerializeList.entSerialize.size) {
        vecpush(entSerializeList.entBm, byte, 0);
    }

    bm_setBitVal(entSerializeList.entBm.arr, id, 1);

    return id;
}

entitySerialize_t *ent_getSerialize(int serializeID)
{
    return &vecget(entSerializeList.entSerialize, serializeID);
}

entitySerialize_t *ent_getSerializeFromEntID(int entID)
{
    entitySerialize_t *entSerialize;

    for(int i = 0; i < vecsize(entSerializeList.entSerialize); i++)
    {
        if(!bm_getBitVal(entSerializeList.entBm.arr, i))
            continue;
        
        entSerialize = &vecget(entSerializeList.entSerialize, i);

        if(entSerialize->entID == entID)
            return entSerialize;
    }

    return NULL;
}

int ent_getSerializeIDFromEntID(int entID)
{
    entitySerialize_t *entSerialize;

    for(int i = 0; i < vecsize(entSerializeList.entSerialize); i++)
    {
        if(!bm_getBitVal(entSerializeList.entBm.arr, i))
            continue;
        
        entSerialize = &vecget(entSerializeList.entSerialize, i);

        if(entSerialize->entID == entID)
            return i;
    }

    return 0;
}

void ent_setStateFlags(entitySerialize_t *entSerialize, recentStreamRecord_t *entStateRecord)
{
    for(int i = 0; i < entSerialize->stateLen; i++)
    {
        byte flag = bm_getBitVal(entSerialize->stateFlags, i);

        streamRecent_setState(entStateRecord, i, flag);
    }

    zmemset(entSerialize->stateFlags, 0, entStateRecord->stateByteLen);

}

int ent_newEntCmd(bitstream_t *bs, void *data)
{
    int remoteEntID = stream_readInt(bs);
    int entType = stream_readInt(bs);

    if(entIDTranslate[remoteEntID] != -1)
        return 0;
    
    int createEntID = entCreatorList[entType].createEnt(data);

    entIDTranslate[remoteEntID] = createEntID;

    return 0;
}

void ent_readEntState(bitstream_t *bs, cl_entStateRecordList_t *entStateRecordList)
{
    entitySerialize_t *entSerialize;
    byte stateBm[32];

    int entLen = stream_readInt(bs);
    
    for(int i = 0; i < entLen; i++)
    {
        int remEntID = stream_readInt(bs);
        int localEntID = entIDTranslate[remEntID];

        entSerialize = ent_getSerializeFromEntID(localEntID);

        streamRecent_readStateBits(entSerialize->stateLen, bs, stateBm);

        entSerialize->read(entSerialize, bs, stateBm);
    }
}

void ent_readSerialize(bitstream_t *bs, cl_entStateRecordList_t *entStateRecordList, inputCommandList_t *inputCommandList)
{
    byte cmd;

    while((cmd = stream_readByte(bs)) != ENTCMD_END)
    {
        switch(cmd)
        {
            case ENTCMD_NEW:
                streamQuick_addPayload(&entStateRecordList->newEntRecord, inputCommandList);
                streamQuick_readPacket(&entStateRecordList->newEntRecord, bs);
                break;
            case ENTCMD_STATE:
                ent_readEntState(bs, entStateRecordList);
                break;
            default:
                com_error(ERR_FATAL, "Error: wrong ent command received\n");
        }
    }
}

void ent_writeAllSerialize(bitstream_t *bs, netcon_t *con, cl_entStateRecordList_t *entStateRecordList)
{
    entitySerialize_t *entSerialize;
    cl_entStateRecord_t *entStateRecord;
    byte stateBm[32];

    if(streamQuick_getRecordCount(&entStateRecordList->newEntRecord) > 0)
    {
        stream_writeByte(bs, ENTCMD_NEW);
        streamQuick_writePacket(&entStateRecordList->newEntRecord, bs, con);
    }

    stream_writeByte(bs, ENTCMD_STATE);
    stream_writeInt(bs, entStateRecordList->permEntLen);

    for(entStateRecord = entStateRecordList->permanentHead; entStateRecord != NULL; entStateRecord = entStateRecord->next)
    {
        entSerialize = ent_getSerializeFromEntID(entStateRecord->entID);

        ent_setStateFlags(entSerialize, &entStateRecord->stateRecord);

        stream_writeInt(bs, entStateRecord->entID);
        
        streamRecent_writeStateBits(&entStateRecord->stateRecord, bs, con, stateBm);

        entSerialize->write(entSerialize, bs, stateBm);
    }

    stream_writeByte(bs, ENTCMD_END);
}

void ent_ackSerialize(netcon_t *con, cl_entStateRecordList_t *entStateRecordList)
{
    cl_entStateRecord_t *entStateRecord;

    streamQuick_acknowledge(&entStateRecordList->newEntRecord, con);

    for(entStateRecord = entStateRecordList->permanentHead; entStateRecord != NULL; entStateRecord = entStateRecord->next)
    {
        streamRecent_acknowledge(&entStateRecord->stateRecord, con);
    }
}

void ent_removeSerialize(int entID)
{
    bm_setBitVal(entSerializeList.entBm.arr, entID, 0);
}

/********************NETWORKING********************/

void ent_initRecordList(cl_entStateRecordList_t *entStateRecordList)
{
    entStateRecordList->clientEntID = -1;

    entStateRecordList->permanentHead = NULL;
    entStateRecordList->permanentTail = NULL;
    entStateRecordList->permEntLen = 0;

    entStateRecordList->viewHead = NULL;
    entStateRecordList->viewTail = NULL;
    entStateRecordList->viewEntLen = 0;

    streamQuick_init(&entStateRecordList->newEntRecord, ent_newEntCmd, NULL);
}

void ent_addPermanent(int entID, netcon_t *con, cl_entStateRecordList_t *entStateRecordList)
{
    cl_entStateRecord_t *clEntRecord;
    entitySerialize_t *entSerialize;
    bitstream_t bs;

    clEntRecord = (cl_entStateRecord_t *) zidmalloc(GENERALZONE, sizeof(cl_entStateRecord_t));

    clEntRecord->entID = entID;
    clEntRecord->next = NULL;

    if(entStateRecordList->permanentHead == NULL)
    {
        entStateRecordList->permanentHead = entStateRecordList->permanentTail = clEntRecord;
    }
    else {
        clEntRecord->prev = entStateRecordList->permanentTail;
        entStateRecordList->permanentTail = clEntRecord;
    }

    streamRecent_init(&clEntRecord->stateRecord, 2, NULL, NULL);

    streamQuick_begin(&entStateRecordList->newEntRecord, con, &bs);
    stream_writeInt(&bs, entID);
    stream_writeInt(&bs, 0);
    streamQuick_end(&entStateRecordList->newEntRecord, con, &bs);

    entStateRecordList->permEntLen++;
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
    vecinit(GENERALZONE, entSpriteList.entSprite, entitySprite_t, ENT_INITSIZE);
    vecinit(GENERALZONE, entSpriteList.entBm, byte, ENT_INITSIZE/8);
    zmemset(entSpriteList.entBm.arr, 0, ENT_INITSIZE/8);
    entSpriteList.renderSpriteList = NULL;
    entSpriteList.renderSpriteCount = 0;

    vecinit(GENERALZONE, animSpriteList.animSprite, animatedSprite_t, ENT_INITSIZE);
    vecinit(GENERALZONE, animSpriteList.entBm, byte, ENT_INITSIZE/8);
    zmemset(animSpriteList.entBm.arr, 0, ENT_INITSIZE/8);
    animSpriteList.renderSpriteList = NULL;
    animSpriteList.renderSpriteCount = 0;


}

int ent_addSprite(int entID, char *spriteName, float pos[2], float rect[4], float angle)
{
    entitySprite_t entSprite;
    animatedSprite_t animSprite;

    entSprite.entID = entID;
    entSprite.texID = s2imap_get(spriteNameMap, spriteName);
    zmemcpy(entSprite.pos, pos, sizeof(float) * 2);
    zmemcpy(entSprite.rect, rect, sizeof(float) * 4);
    entSprite.angle = angle;

    int id = bm_findEmpty(entSpriteList.entBm.arr, entSpriteList.entSprite.size);
    if(id < 0)
    {
        id = vecsize(entSpriteList.entSprite);
        vecpush(entSpriteList.entSprite, entitySprite_t, entSprite);
    }
    else {
        vecset(entSpriteList.entSprite, id, entSprite);
    }

    bm_setBitVal(entSpriteList.entBm.arr, id, 1);

    return id;
}

entitySprite_t *ent_getSpriteFromEnt(int entID)
{
    entitySprite_t *entSprite;
    
    for(int i = 0; i < vecsize(entSpriteList.entSprite); i++)
    {
        entSprite = &vecget(entSpriteList.entSprite, i);
        if(entSprite->entID == entID)
            return entSprite;
    }

    return NULL;
}

entitySprite_t *ent_getSprite(int spriteID)
{
    return &vecget(entSpriteList.entSprite, spriteID);
}

int ent_addAnimSprite(int entID, char *spriteName, float pos[2], float rect[4], float angle)
{
    animatedSprite_t animSprite;

    animSprite.entID = entID;
    animSprite.texID = s2imap_get(animSpriteNameMap, spriteName);
    zmemcpy(animSprite.pos, pos, sizeof(float) * 2);
    zmemcpy(animSprite.rect, rect, sizeof(float) * 4);
    animSprite.angle = angle;

    int id = bm_findEmpty(animSpriteList.entBm.arr, vecsize(animSpriteList.animSprite));

    if(id < 0)
    {
        id = vecsize(animSpriteList.animSprite);
        vecpush(animSpriteList.animSprite, animatedSprite_t, animSprite);
    }
    else {
        vecset(animSpriteList.animSprite, id, animSprite);
    }

    bm_setBitVal(animSpriteList.entBm.arr, id, 1);

    vecpush(animSpriteList.animSprite, animatedSprite_t, animSprite);

    return id;
}

animatedSprite_t *ent_getAnimSprite(int spriteID)
{
    return &vecget(animSpriteList.animSprite, spriteID);
}

animatedSprite_t *ent_getAnimSpriteFromEnt(int entID)
{
    animatedSprite_t *animSprite;
    
    for(int i = 0; i < vecsize(animSpriteList.animSprite); i++)
    {
        animSprite = &vecget(animSpriteList.animSprite, i);
        if(animSprite->entID == entID)
            return animSprite;
    }

    return NULL;
}

void ent_setAllSpritePos()
{
    entVec_t *entVec;
    entitySprite_t *entSprite;

    for(int i = 0; i < vecsize(entSpriteList.entSprite); i++)
    {
        if(!bm_getBitVal(entSpriteList.entBm.arr, i))
            continue;
    
        entSprite = &vecget(entSpriteList.entSprite, i);

        entVec = ent_getPos(entSprite->entID);
    }
}

void sprite_addSpriteToRender()
{
    int k = 0;
    entitySprite_t *entSprite;
    rect2_t spriteRect;

    if(entSpriteList.renderSpriteList != NULL)
    {
        zidfree(entSpriteList.renderSpriteList);
        entSpriteList.renderSpriteList = NULL;
        entSpriteList.renderSpriteCount = 0;
    }

    for(int i = 0; i < vecsize(entSpriteList.entSprite); i++)
    {
        if(!bm_getBitVal(entSpriteList.entBm.arr, i))
            continue;
        
        entSprite = &vecget(entSpriteList.entSprite, i);

        rect2set(spriteRect, entSprite->rect);
        vec2add(spriteRect, spriteRect, entSprite->pos);


        if(!checkRectIntersect(spriteRect, worldCamera.window))
            continue;

        k++;
    }

    if(k == 0)
        return;

    entSpriteList.renderSpriteList = (entitySprite_t *) zidmalloc(TEMPORARYZONE, sizeof(entitySprite_t) * k);
    entSpriteList.renderSpriteCount = k;

    k=0;
    for(int i = 0; i < vecsize(entSpriteList.entSprite); i++)
    {
        if(!bm_getBitVal(entSpriteList.entBm.arr, i))
            continue;

        entSprite = &vecget(entSpriteList.entSprite, i);

        rect2set(spriteRect, entSprite->rect);
        vec2add(spriteRect, spriteRect, entSprite->pos);

        if(!checkRectIntersect(spriteRect, worldCamera.window))
            continue;
    
        zmemcpy(&entSpriteList.renderSpriteList[k], &vecget(entSpriteList.entSprite, i), sizeof(entitySprite_t));
        k++;
    }
}

void sprite_addAnimToRender()
{
    int k = 0;
    animatedSprite_t *animSprite;
    rect2_t spriteRect;

    if(animSpriteList.renderSpriteList != NULL)
    {
        zidfree(animSpriteList.renderSpriteList);
        animSpriteList.renderSpriteList = NULL;
        animSpriteList.renderSpriteCount = 0;
    }

    for(int i = 0; i < vecsize(animSpriteList.animSprite); i++)
    {
        if(!bm_getBitVal(animSpriteList.entBm.arr, i))
            continue;

        animSprite = &vecget(animSpriteList.animSprite, i);
        
        rect2set(spriteRect, animSprite->rect);
        vec2add(spriteRect, spriteRect, animSprite->pos);

        if(!checkRectIntersect(spriteRect, worldCamera.window))
            continue;

        k++;
    }

    if(k == 0)
        return;

    animSpriteList.renderSpriteList = (animatedSprite_t *) zidmalloc(TEMPORARYZONE, sizeof(animatedSprite_t) * k);
    animSpriteList.renderSpriteCount = k;

    k=0;
    for(int i = 0; i < vecsize(animSpriteList.animSprite); i++)
    {
        if(!bm_getBitVal(animSpriteList.entBm.arr, i))
            continue;

        animSprite = &vecget(animSpriteList.animSprite, i);

        rect2set(spriteRect, animSprite->rect);
        vec2add(spriteRect, spriteRect, animSprite->pos);        
        
        if(!checkRectIntersect(spriteRect, worldCamera.window))
            continue;
    
        zmemcpy(&animSpriteList.renderSpriteList[k], &vecget(animSpriteList.animSprite, i), sizeof(animatedSprite_t));
        k++;
    }
}

void ent_handleSprites()
{
    ent_setAllSpritePos();

    sprite_addSpriteToRender();

    sprite_addAnimToRender();
}


/********************INIT********************/

void ent_addCreator( int (*createEnt)())
{
    entCreatorList[entCreatorLen].createEnt = createEnt;
    entCreatorLen++;
}

int ent_createEnt(int createId, void *data)
{
    return entCreatorList[createId].createEnt(data);
}

void ent_init()
{
    ent_initEntList();
    ent_initThinkList();
    ent_initSerializeList();
    ent_initSpriteList();
    ent_initMoveList();
}