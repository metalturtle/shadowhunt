#include "entity.h"
#include "engine.h"
#include "../movement/movement.h"


s2imap_t *spriteNameMap;
s2imap_t *animSpriteNameMap;




entityList_t entList;
vectorEntityList_t vectorEntityList;

entitySerializerList_t entSerializerList;


entitySpriteList_t entSpriteList;
animatedSpriteList_t animSpriteList;

rayList_t rayList;
renderRayList_t renderRayList;

/********************ENTITY********************/

void ent_initEntList()
{
    int initSize = ENT_INITSIZE;
    vecinit(GENERALZONE, entList.entBm, byte, initSize/8);
    zmemset(entList.entBm.arr, 0, entList.entBm.capacity);

}

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


/********************SERIALIZE AND NETWORKING********************/


// creates a new client entity record list when a new client joins
void ent_initClientEntList(int conID, clientEntityRecordList_t *clEntRecordList)
{
    int initSize = ENT_INITSIZE;

    // set the connection ID
    clEntRecordList->conID = conID;

    // initialise recordList and bitmap vectors
    // recordList contains recent state records of all entities
    vecinit(GENERALZONE, clEntRecordList->recordList, entityRecord_t, initSize);
    vecinit(GENERALZONE, clEntRecordList->bitmap, byte, initSize/8);


    // initialize quick stream that stores a list of new entities to send
    streamQuick_init(&clEntRecordList->newEntRecord, NULL, NULL);
}


// initializes the entity serializer
void ent_setSerializer(
    entitySerializer_t *entSerialize,
    int stateSize,
    int (*readState)(int, bitstream_t *, byte *),
    int (*writeState)(int, bitstream_t *, byte *stateBm, int conID),
    // int (*readInitParam)(int, bitstream_t *),
    int (*readInitParam)(bitstream_t *),
    int (*applyInitParam)(),
    int (*writeInitParam)(int, int, bitstream_t *)
    )
{
    int initSize = ENT_INITSIZE;

    
    // save read and write entity state
    entSerialize->readState = readState;
    entSerialize->writeState = writeState;
    
    //save read and write init entity functions
    entSerialize->readInitParam = readInitParam;
    entSerialize->applyInitParam = applyInitParam;
    entSerialize->writeInitParam = writeInitParam;

    //length of the number of states the entity has
    entSerialize->stateLen = stateSize;


    // initialize client record list.
    // client record list stores list of client entity record list for each client
    // client entity records store the client state of entities
    vecinit(GENERALZONE, entSerialize->clientRecordList, clientEntityRecordList_t, initSize);
    vecinit(GENERALZONE, entSerialize->clientBitmap, byte, initSize/8);

    // initialise entity state list.
    // this is for the server to set the state bits that have changed, and should
    // be sent to all clients
    vecinit(GENERALZONE, entSerialize->entityStateList, entityStateBitmap_t, initSize);
    vecinit(GENERALZONE, entSerialize->entityBitmap, byte, initSize/8);


    // init the entity ID translate map. The server entity IDs
    // are mapped to the client entity IDs
    entSerialize->translateIDMap = i2imap_init(GENERALZONE);

    // init the mapping that translates connection id to the
    // location in clientRecordList vector
    entSerialize->conIDMap = i2imap_init(GENERALZONE);

    //set the client entity to -1
    entSerialize->clientEntID = -1;
}


// client reads the state of entities that are sent by the server
void ent_readEntStateList(entitySerializer_t *entSerializer, bitstream_t *bs)
{
    byte stateBm[32];


    //read the count of entities that are sent
    int entLen = stream_readInt(bs);


    // if entity count is equal to or less than zero, then return
    if(entLen <= 0)
        return;



    // read the state of the entities
    for(int j = 0; j < entLen; j++)
    {
        // read the server entity id
        int remoteEntID = stream_readInt(bs);

        // translate the server entity id to the client entity id
        int translatedID = i2imap_get(entSerializer->translateIDMap, remoteEntID);

        //read the states that were sent
        streamRecent_readStateBits(entSerializer->stateLen, bs, stateBm);

        // call readState function that reads the states sent based
        // on the state flags
        entSerializer->readState(translatedID, bs, stateBm);
    }
}


// read list of new entities that should be created
void ent_readNewEntList(
    int conID,
    int entType,
    entitySerializer_t *entSerializer,
    bitstream_t *bs
    )
{
    // get the particular client entity record list
    int recID = i2imap_get(entSerializer->conIDMap, conID);
    clientEntityRecordList_t *clEntList = &vecget(entSerializer->clientRecordList, recID);

    
    // get the number of records that were sent
    int recordCount = streamQuick_readCount(&clEntList->newEntRecord, bs);


    printf("read new ent list %d\n", recordCount);
    for(int i = 0; i < recordCount; i++)
    {
        // get the server entity id
        int remoteEntID = stream_readInt(bs);

        // printf("remote ent id %d \n", remoteEntID);

        printf("before read param %d \n", remoteEntID);
        entSerializer->readInitParam(bs);


        // check if the server entity id already exists, which means
        // entity is already created
        int clientEntID = i2imap_get(entSerializer->translateIDMap, remoteEntID);
        if(clientEntID != -1)
            continue;
    
        printf("client ent ID: %d \n", clientEntID);
        
        clientEntID = entSerializer->applyInitParam(clientEntID);

        // call the function of the serializer that reads initialization
        // params and creates entities

        printf("client ent ID after creation: %d \n", clientEntID);

        // if no entities are created then skip
        if(clientEntID < 0)
            continue;

        // add mapping for the server entity id to the client entity id
        i2imap_put(entSerializer->translateIDMap, remoteEntID, clientEntID);
    }
}


// write list of new entities for sending to a client
void ent_writeNewEntList(int conID, entitySerializer_t *entSerializer, netcon_t *con, bitstream_t *bs)
{
    // get the client entity record list
    int recID = i2imap_get(entSerializer->conIDMap, conID);
    clientEntityRecordList_t *clEntList = &vecget(entSerializer->clientRecordList, recID);


    // if there is nothing to write then return
    if(streamQuick_getRecordCount(&clEntList->newEntRecord) <= 0) {
        return;
    }


    // write new entity command
    stream_writeByte(bs, ENTCMD_NEW);

    // write the stored list of new entities and their initialization params
    streamQuick_writePacket(&clEntList->newEntRecord, bs, con);
}


// write a list of entity states for the client
void ent_writeStateList(int conID, entitySerializer_t *entSerializer, netcon_t *con, bitstream_t *bs)
{
    byte stateBm[32];
    // get the client entity record list
    int recID = i2imap_get(entSerializer->conIDMap, conID);
    clientEntityRecordList_t *clEntList = &vecget(entSerializer->clientRecordList, recID);

    // get the count of entities to write
    int entLen = 0;
    for(int e = 0; e < vecsize(clEntList->recordList); e++)
    {
        if(!bm_getBitVal(clEntList->bitmap.arr, e)) {
            continue;
        }

        entLen++;
    }


    // if there are no entities to write, then return
    if(entLen == 0)
        return;


    // write the entity state cmd
    stream_writeByte(bs, ENTCMD_STATE);


    stream_writeInt(bs, entLen);

    //go through the list of client entity records
    for(int e = 0; e < vecsize(clEntList->recordList); e++)
    {
        if(!bm_getBitVal(clEntList->bitmap.arr, e))
        {
            continue;
        }

        // get the entity record from the client record list
        entityRecord_t *entRecord = &vecget(clEntList->recordList, e);


        // get the entity state that should be sent to all clients
        entityStateBitmap_t entState = vecget(entSerializer->entityStateList, entRecord->entID);


        // write the entity id
        stream_writeInt(bs, entRecord->entID);


        // save the state bits
        streamRecent_setStateBits(&entRecord->recentRecord, entState.state);


        // write the entity state to the bitstream
        streamRecent_writeStateBits(&entRecord->recentRecord, bs, con, stateBm);


        // write the entity state for the state bits that are set
        entSerializer->writeState(entRecord->entID, bs, stateBm, conID);

    }
}


// All serializers write the list of new entities to create, and the entity state
void ent_writeSerializerList(int conID, netcon_t *con, bitstream_t *bs)
{
    // loop through the entity serializer list
    for(int i = 0; i < entSerializerList.length; i++)
    {
        entitySerializer_t *entSerializer = &(entSerializerList.list[i]);

        // write the list of new entities to create
        ent_writeNewEntList(conID, entSerializer, con, bs);


        // write the entity state
        ent_writeStateList(conID, entSerializer, con, bs);


        stream_writeByte(bs, ENTCMD_END);
    }
}


void ent_cleanupSerializerState()
{
    for(int i = 0; i < entSerializerList.length; i++)
    {
        entitySerializer_t *entSerializer = &(entSerializerList.list[i]);
        for(int j = 0; j < vecsize(entSerializer->entityStateList); j++)
        {
            if(!bm_getBitVal(entSerializer->entityBitmap.arr, j))
            {
                continue;
            }

            entityStateBitmap_t *entState = &vecget(entSerializer->entityStateList, j);


            int byteNum = (int)CEIL(((float)entSerializer->stateLen)/8.0);
            for(int b = 0; b < byteNum; b++)
            {
                entState->state[b] = 0;
            }
        }
    }
}

// read the commands for creating, reading state of entities
void ent_readSerializerList(int conID, netcon_t *con, bitstream_t *bs)
{
    for(int i = 0; i < entSerializerList.length; i++)
    {
        entitySerializer_t *entSerializer = &(entSerializerList.list[i]);

        byte cmd;
        while((cmd = stream_readByte(bs)) != ENTCMD_END)
        {
            switch(cmd)
            {
                case ENTCMD_NEW:
                    ent_readNewEntList(conID, i, entSerializer, bs);
                    break;
                case ENTCMD_STATE:
                    ent_readEntStateList(entSerializer, bs);
                    break;
                default:
                    com_error(ERR_FATAL, "Error: wrong ent command received %d %d %d\n", cmd, ENTCMD_STATE, ENTCMD_NEW);
            }
        }
    }
}


// go through all entity records of a client, and acknowledge entity states sent
void ent_ackSerializerList(int conID, netcon_t *con, bitstream_t *bs)
{
    // loop through the list of entity serializers
    for(int i = 0; i < entSerializerList.length; i++)
    {
        entitySerializer_t *entSerializer = &(entSerializerList.list[i]);


        // get the client entity record list
        int recID = i2imap_get(entSerializer->conIDMap, conID);
        clientEntityRecordList_t *clEntList = &vecget(entSerializer->clientRecordList, recID);


        streamQuick_acknowledge(&(clEntList->newEntRecord), con);

        // loop through the entity records for the client
        for(int e = 0; e < vecsize(clEntList->recordList); e++)
        {
           if(!bm_getBitVal(clEntList->bitmap.arr, e))
            {
                continue;
            }


            // get the entity record and call acknowledge, for the states
            // that were recieved by the client
            entityRecord_t *entRecord = &vecget(clEntList->recordList, e);
            streamRecent_acknowledge(&(entRecord->recentRecord), con);
        }
    }
}

// set the state flags for the entity, that should be sent to all clients
void ent_setStateFlags(int serializerID, int entID, int i, byte flag)
{
    entitySerializer_t *entSerializer = &(entSerializerList.list[serializerID]);
    entityStateBitmap_t entStateBM = vecget(entSerializer->entityStateList, entID);
    bm_setBitVal(entStateBM.state, i, flag);
}


// create client entity list for each serializer
void ent_handleClientJoin(int conID, netcon_t *con)
{
    clientEntityRecordList_t *clEntRecordList;
    bitstream_t bs;

    
    // loop through serializers
    for(int i = 0; i < entSerializerList.length; i++)
    {
        entitySerializer_t *entSerialize = &entSerializerList.list[i];


        // find an empty slot
        int id = bm_findEmpty(entSerialize->clientBitmap.arr, vecsize(entSerialize->clientRecordList));


        // if no space in the list, then extend the vectors
        if(id < 0)
        {
            id = vecsize(entSerialize->clientRecordList);
            vecpush(entSerialize->clientBitmap, byte, 0);
            vecpushempty(entSerialize->clientRecordList, clientEntityRecordList_t);
            vecpushempty(entSerialize->entityStateList, entityStateBitmap_t);
            printf("pushed entity state %p \n", vecget(entSerialize->entityStateList, id));
        }


        //get the client entity record list
        clEntRecordList = &vecget(entSerialize->clientRecordList, id);


        // initialize the client entity record list
        ent_initClientEntList(conID, clEntRecordList);

        i2imap_put(entSerialize->conIDMap, conID, id);

        bm_setBitVal(entSerialize->clientBitmap.arr, id, 1);


        int bmSize = 0;
        for(int j = 0; j < vecsize(entSerialize->entityStateList); j++)
        {
            if(!bm_getBitVal(entSerialize->entityBitmap.arr, j)) {
                continue;
            }

            bmSize += 1;
        }


        for(int j = 0; j < bmSize; j++)
        {
            vecpush(clEntRecordList->bitmap, byte, 0);
        }


        printf("entity state list size %d \n", vecsize(entSerialize->entityStateList));
        for(int j = 0; j < vecsize(entSerialize->entityStateList); j++)
        {
            if(!bm_getBitVal(entSerialize->entityBitmap.arr, j)) {
                continue;
            }


            printf("adding enitity %d \n", j);

            vecpushempty(clEntRecordList->recordList, entityRecord_t);
            bm_setBitVal(clEntRecordList->bitmap.arr, vecsize(clEntRecordList->recordList) - 1, 1);


            entityRecord_t *entRecord = &vecget(clEntRecordList->recordList, j);

            // allocate a state change bitmap. Init the state record in the
            // ent record object
            byte *stateChangeBm = (byte *) zidmalloc(GENERALZONE, (int)CEIL(((float)entSerialize->stateLen)/8.0));
            streamRecent_init(&entRecord->recentRecord, entSerialize->stateLen, NULL, NULL, stateChangeBm);


            int entID = j;

            // set the entity ID
            entRecord->entID = entID;


            //init the quick record for storing new entities and their init params
            streamQuick_begin(&clEntRecordList->newEntRecord, con, &bs);
            stream_writeInt(&bs, entID);
            entSerialize->writeInitParam(entID, conID, &bs);
            streamQuick_end(&clEntRecordList->newEntRecord, con, &bs);
            printf("1 adding new entity to the new ent list %d %d \n", conID, entID);

            // entRecord = &vecget(clEntRecordList->recordList, vecsize(clEntList->recordList) - 1);
            // freeid = vecsize(clEntRecordList->recordList) - 1;
        }
    }
}


void ent_setupSyncedEnt(int entID, int entType)
{
    entitySerializer_t *entSerialize = &entSerializerList.list[entType];
    if(entID == vecsize(entSerialize->entityStateList)) {
        vecpushempty(entSerialize->entityStateList, entityStateBitmap_t);
    }

    entityStateBitmap_t *entStateBitmap = &vecget(entSerialize->entityStateList, entID);
    entStateBitmap->state = (byte *) zidmalloc(GENERALZONE, (int)CEIL(((float)entSerialize->stateLen)/8.0));
    bm_setBitVal(entSerialize->entityBitmap.arr, entID, 1);
}


// add entity records for all clients
void ent_addSyncedEntToClient(int entID, int conID, netcon_t *con, int entType)
{
    bitstream_t bs;

    entitySerializer_t *entSerializer = &(entSerializerList.list[entType]);


    // get the client entity record list
    int recID = i2imap_get(entSerializer->conIDMap, conID);
    clientEntityRecordList_t *clEntList = &vecget(entSerializer->clientRecordList, recID);


    // check if there is an unused entity record
    int freeid = -1;
    entityRecord_t *entRecord = NULL;
    for(int e = 0; e < vecsize(clEntList->recordList); e++)
    {
        if(!bm_getBitVal(clEntList->bitmap.arr, e))
        {
            freeid = e;
            entRecord = &vecget(clEntList->recordList, e);
            break;
        }
    }


    // if no unused entity record, then extend vector
    if(entRecord == NULL)
    {
        vecpush(clEntList->bitmap, byte, 0);
        vecpushempty(clEntList->recordList, entityRecord_t);
        entRecord = &vecget(clEntList->recordList, vecsize(clEntList->recordList) - 1);
        freeid = vecsize(clEntList->recordList) - 1;
    }


    bm_setBitVal(clEntList->bitmap.arr, freeid, 1);


    // allocate a state change bitmap. Init the state record in the
    // ent record object
    byte *stateChangeBm = (byte *) zidmalloc(GENERALZONE, (int)CEIL(((float)entSerializer->stateLen)/8.0));
    streamRecent_init(&entRecord->recentRecord, entSerializer->stateLen, NULL, NULL, stateChangeBm);


    // set the entity ID
    entRecord->entID = entID;

    //init the quick record for storing new entities and their init params
    streamQuick_begin(&clEntList->newEntRecord, con, &bs);
    stream_writeInt(&bs, entID);
    entSerializer->writeInitParam(entID, conID, &bs);
    streamQuick_end(&clEntList->newEntRecord, con, &bs);
    printf("2 adding new entity to the new ent list %d %d \n", conID, entID);
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


void ent_initVectorEntityList()
{
    // inputCommandList_t
    int initSize = ENT_INITSIZE;
    vecinit(GENERALZONE, vectorEntityList.movableList, movableEntity_t, initSize);
    // vecinit(GENERALZONE, vectorEntityList.spriteList, entitySprite_t, initSize);
    vecinit(GENERALZONE, vectorEntityList.animSpriteList, animatedSprite_t, initSize);
    vecinit(GENERALZONE, vectorEntityList.shootTimerList, endTimer_t, initSize);
    vecinit(GENERALZONE, vectorEntityList.moveIDList, int, initSize);
    vecinit(GENERALZONE, vectorEntityList.posInterpolateList, positionInterpolate_t, initSize);
    vecinit(GENERALZONE, vectorEntityList.angleInterpolateList, angleInterpolate_t, initSize);
    vecinit(GENERALZONE, vectorEntityList.bitmap, byte, initSize/8);
    vectorEntityList.mainEntMap = i2imap_init(GENERALZONE);
}


int ent_addVectorEntity()
{

    int id = bm_findEmpty(vectorEntityList.bitmap.arr, vecsize(vectorEntityList.bitmap));
    if(id < 0)
    {
        id = vecsize(vectorEntityList.movableList);
        vecpushempty(vectorEntityList.movableList, movableEntity_t);
        vecpushempty(vectorEntityList.animSpriteList, animatedSprite_t);
        vecpushempty(vectorEntityList.shootTimerList, endTimer_t);
        vecpushempty(vectorEntityList.posInterpolateList, positionInterpolate_t);
        vecpushempty(vectorEntityList.angleInterpolateList, angleInterpolate_t);
        vecpushempty(vectorEntityList.moveIDList, int);
    }

    movableEntity_t *moveEnt = &vecget(vectorEntityList.movableList, id);
    animatedSprite_t *sprite = &vecget(vectorEntityList.animSpriteList, id);
    endTimer_t *endTimer = &vecget(vectorEntityList.shootTimerList, id);
    positionInterpolate_t *posIntp = &vecget(vectorEntityList.posInterpolateList, id);
    angleInterpolate_t *angIntp = &vecget(vectorEntityList.angleInterpolateList, id);


    vec3xyz(moveEnt->pos, 300, 330, 300);
    rect2xywh(moveEnt->bound, 0, 0, 10, 10);

    vec3set(moveEnt->move.pos, moveEnt->pos);
    rect2set(moveEnt->move.rect, moveEnt->bound);
    vec3xyz(moveEnt->move.dir, 0, 0, 0);

    vec3set(sprite->pos, moveEnt->pos);
    rect2set(sprite->rect, moveEnt->bound);
    // rect2xywh(sprite->rect, 0, 0, 10, 10);
    startTimer(endTimer, 200);
    zmemset(posIntp, 0, sizeof(positionInterpolate_t));
    zmemset(angIntp, 0, sizeof(angleInterpolate_t));


    int moveID = physics_addBody(&moveEnt->move);
    vecset(vectorEntityList.moveIDList, id, moveID);


    sprite->texID = 3;
    sprite->curSprite = 0;


    bm_setBitVal(vectorEntityList.bitmap.arr, id, 1);
    return id;
}

void ent_removeVectorEntity(int entID)
{

}

/********************RAY********************/

void ent_initRayList()
{
    int initSize = ENT_INITSIZE;
    vecinit(GENERALZONE, rayList.xList, float, initSize);
    vecinit(GENERALZONE, rayList.yList, float, initSize);
    vecinit(GENERALZONE, rayList.xDirList, float, initSize);
    vecinit(GENERALZONE, rayList.yDirList, float, initSize);

    
    vecinit(GENERALZONE, renderRayList.xList, float, initSize);
    vecinit(GENERALZONE, renderRayList.yList, float, initSize);
    vecinit(GENERALZONE, renderRayList.xDirList, float, initSize);
    vecinit(GENERALZONE, renderRayList.yDirList, float, initSize);
    vecinit(GENERALZONE, renderRayList.endTimeList, unsigned long int, initSize);
}


/********************INIT********************/


void ent_init()
{
    ent_initEntList();
    ent_initSpriteList();
    ent_initRayList();
    // ent_initMoveList();
    ent_initVectorEntityList();
}