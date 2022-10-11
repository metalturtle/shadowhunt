#include "entity.h"
#include "engine.h"

int mainEnt;

entCreator_t entCreatorList[ENT_CREATORSIZE];
int entCreatorLen = 0;

entityList_t entList;
entityMove_t entityMoveList;
entityThinkList_t entThinkList;
entitySerializeList_t entSerializeList;
entitySpriteList_t entSpriteList;
animatedSpriteList_t animSpriteList;
s2imap_t *spriteNameMap;
s2imap_t *animSpriteNameMap;

int entIDTranslate[20];

/********************ENTITY********************/

void ent_initEntList()
{
    int initSize = ENT_INITSIZE;
    vecinit(GENERALZONE, entList.pos, entVec_t, initSize);
    vecinit(GENERALZONE, entList.entBm, byte, initSize/8);
    memset(entList.entBm.arr, 0, entList.entBm.capacity);

    for(int i = 0; i < 20; i++)
    {
        entIDTranslate[i] = -1;
    }
}

int ent_addEnt(vec3_t pos)
{
    int id = bm_findEmpty(entList.entBm.arr, entList.pos.size);
    
    entVec_t entPos;
    vec3set(entPos.vec, pos);
    
    if(id < 0)
    {
        vecpush(entList.pos, entVec_t, entPos);
        id = entList.pos.size - 1;
    }
    else {
        vecset(entList.pos, id, entPos);
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

void ent_removeEnt(int entID)
{
    bm_setBitVal(entList.entBm.arr, entID, 0);
}

/********************MOVEMENT********************/

void ent_initMoveList()
{
    int initSize = 16;
    vecinit(GENERALZONE, entityMoveList.entID, int, initSize);
    vecinit(GENERALZONE, entityMoveList.dir, entVec_t, initSize);
    vecinit(GENERALZONE, entityMoveList.speed, float, 16);
}

void ent_addMove(int entID, vec3_t dir, float speed)
{
    vecpush(entityMoveList.entID, int, entID);

    vecpushempty(entityMoveList.dir, entVec_t);
    int id = entityMoveList.dir.size - 1;
    vec3set(vecget(entityMoveList.dir, id).vec, dir);

    vecpush(entityMoveList.speed, float, speed);
}

void ent_resetMove()
{
    vecreset(entityMoveList.entID);
    vecreset(entityMoveList.dir);
    vecreset(entityMoveList.speed);
}

/********************RUN FUNCTION********************/

void ent_initThinkList()
{
    int initSize = ENT_INITSIZE;
    vecinit(GENERALZONE, entThinkList.entThink, entityThink_t, initSize);
    vecinit(GENERALZONE, entThinkList.entBm, byte, initSize/8);
    memset(entThinkList.entBm.arr, 0, initSize/8);
}

void ent_addThink(int entID, inputCommandList_t *inputCommandList, void (*think)(entityThink_t *entThink))
{
    int id = bm_findEmpty(entThinkList.entBm.arr, entThinkList.entThink.size);

    entityThink_t entThink;
    entThink.entID = entID;
    entThink.think = think;
    entThink.inputCommandList = inputCommandList;

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
    printf("sprite init\n");
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
    int isServer = cvar_getInt("isServer");
    if(isServer)
        return 0;

    if(type == SPRITE_TYPE_ANIM)
        return s2imap_get(animSpriteNameMap, spriteName);

    return s2imap_get(spriteNameMap, spriteName);
}


void ent_initSpriteList()
{
    vecinit(GENERALZONE, entSpriteList.entSprite, entitySprite_t, ENT_INITSIZE);
    vecinit(GENERALZONE, animSpriteList.animSprite, animatedSprite_t, ENT_INITSIZE);
}

void ent_addSprite(int entID, char *spriteName, int type, float pos[2], float rect[4], float angle, void (*render)(entitySprite_t *sprite))
{
    entitySprite_t entSprite;
    animatedSprite_t animSprite;

    entSprite.entID = entID;
    entSprite.texID = sprite_getID(spriteName, type);
    zmemcpy(entSprite.pos, pos, sizeof(float) * 2);
    zmemcpy(entSprite.rect, rect, sizeof(float) * 4);
    entSprite.angle = angle;
    entSprite.type = type;
    entSprite.render = render;

    printf("ent add sprite %d\n", entSprite.texID);

    vecpush(entSpriteList.entSprite, entitySprite_t, entSprite);

    if(type == SPRITE_TYPE_ANIM)
    {
        animSprite.spriteID = vecsize(entSpriteList.entSprite);
        animSprite.speed = 0.1f;
        animSprite.curSprite = 0;
        animSprite.isRunning = 1;

        entSprite.animID = vecsize(animSpriteList.animSprite);

        vecpush(animSpriteList.animSprite, animatedSprite_t, animSprite);
    }
}

entitySprite_t *ent_getSpriteFromSpriteID(int spriteID)
{
    return &vecget(entSpriteList.entSprite, spriteID);
}

void ent_setAllSpritePos()
{
    entVec_t *entVec;
    entitySprite_t *entSprite;

    for(int i = 0; i < vecsize(entSpriteList.entSprite); i++)
    {
        entSprite = &vecget(entSpriteList.entSprite, i);

        // entVec = ent_getPos(entSprite->entID);
        // vec3set(entSprite->pos, entVec->vec);
        entSprite->render(entSprite);
    }
}

animatedSprite_t *ent_getAnimSprite(int animID)
{
    return &vecget(animSpriteList.animSprite, animID);
}

void ent_runAnimSprite()
{
    animatedSprite_t *animSprite;
    for(int i = 0; i < vecsize(animSpriteList.animSprite); i++)
    {
        animSprite = &vecget(animSpriteList.animSprite, i);
        
        animSprite->curSprite += animSprite->speed * animSprite->isRunning;

        animSprite->curSprite = FRACT(animSprite->curSprite);
    }
}

void ent_handleSprites()
{
    ent_setAllSpritePos();

    ent_runAnimSprite();
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
}