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

renderRayList_t renderRayList;
// emittedRayList_t emittedRayList;
// rayEntityList_t rayEntityList;
// rayHitList_t rayHitList;
killIDList_t killIDList;
// rayHandleList_t rayHandleList;
rayWeaponHandle_t rayWeaponHandle;

float shootAngleList[8] = {0, -1,3, -5, 4, 2, -1, -2};
int shootAngListLen = 8;

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


    // initialize quick stream that stores a list of entities to remove
    streamQuick_init(&clEntRecordList->removeEntRecord, NULL, NULL);
}


void ent_freeClientEntList(int conID, clientEntityRecordList_t *clEntRecordList)
{
    vecfree(clEntRecordList->recordList);
    vecfree(clEntRecordList->bitmap);

    streamQuick_close(&clEntRecordList->newEntRecord);
    streamQuick_close(&clEntRecordList->removeEntRecord);
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
    int (*writeInitParam)(int, int, bitstream_t *),
    void (*removeEntity)(int)
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
    entSerialize->removeEntity = removeEntity;

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


    for(int i = 0; i < recordCount; i++)
    {
        // get the server entity id
        int remoteEntID = stream_readInt(bs);


        entSerializer->readInitParam(bs);


        // check if the server entity id already exists, which means
        // entity is already created
        int clientEntID = i2imap_get(entSerializer->translateIDMap, remoteEntID);
        if(clientEntID != -1)
            continue;
    
        
        clientEntID = entSerializer->applyInitParam(clientEntID);

        // call the function of the serializer that reads initialization
        // params and creates entities


        // if no entities are created then skip
        if(clientEntID < 0)
            continue;

        // add mapping for the server entity id to the client entity id
        i2imap_put(entSerializer->translateIDMap, remoteEntID, clientEntID);
    }
}


// read list of new entities that should be created
void ent_readRemoveEntList(
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
    int recordCount = streamQuick_readCount(&clEntList->removeEntRecord, bs);


    for(int i = 0; i < recordCount; i++)
    {
        // get the server entity id
        int remoteEntID = stream_readInt(bs);


        // check if the server entity id already exists, which means
        // entity is already created
        int clientEntID = i2imap_get(entSerializer->translateIDMap, remoteEntID);
        if(clientEntID < 0)
            continue;



        // if no entities are created then skip
        if(clientEntID < 0)
            continue;

        
        entSerializer->removeEntity(clientEntID);

        // add mapping for the server entity id to the client entity id
        i2imap_remove(entSerializer->translateIDMap, remoteEntID);
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


// write list of new entities for sending to a client
void ent_writeRemoveEntList(int conID, entitySerializer_t *entSerializer, netcon_t *con, bitstream_t *bs)
{
    // get the client entity record list
    int recID = i2imap_get(entSerializer->conIDMap, conID);
    clientEntityRecordList_t *clEntList = &vecget(entSerializer->clientRecordList, recID);


    // if there is nothing to write then return
    if(streamQuick_getRecordCount(&clEntList->removeEntRecord) <= 0) {
        return;
    }


    // write new entity command
    stream_writeByte(bs, ENTCMD_REMOVE);

    // write the stored list of new entities and their initialization params
    streamQuick_writePacket(&clEntList->removeEntRecord, bs, con);
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


        // write the list of new entities to remove
        ent_writeRemoveEntList(conID, entSerializer, con, bs);
        

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
                case ENTCMD_REMOVE:
                    ent_readRemoveEntList(conID, i, entSerializer, bs);
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

        streamQuick_acknowledge(&(clEntList->removeEntRecord), con);

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


        for(int j = 0; j < vecsize(entSerialize->entityStateList); j++)
        {
            if(!bm_getBitVal(entSerialize->entityBitmap.arr, j)) {
                continue;
            }


            int entRecID = vecsize(clEntRecordList->recordList);
            vecpushempty(clEntRecordList->recordList, entityRecord_t);


            bm_setBitVal(clEntRecordList->bitmap.arr, entRecID, 1);


            entityRecord_t *entRecord = &vecget(clEntRecordList->recordList, entRecID);

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


void ent_addSyncedEntState(int entID, int entType)
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
        freeid = vecsize(clEntList->recordList);
        vecpush(clEntList->bitmap, byte, 0);
        vecpushempty(clEntList->recordList, entityRecord_t);
        entRecord = &vecget(clEntList->recordList, freeid);
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


void ent_removeSyncedEntState(int entID, int entType)
{
    entitySerializer_t *entSerialize = &entSerializerList.list[entType];
    entityStateBitmap_t *entStateBitmap = &vecget(entSerialize->entityStateList, entID);
    zidfree(entStateBitmap->state);
    bm_setBitVal(entSerialize->entityBitmap.arr, entID, 0);
}

void ent_handleClientLeave(int conID, netcon_t *con)
{
    for(int i = 0; i < entSerializerList.length; i++)
    {
        entitySerializer_t *entSerialize = &entSerializerList.list[i];
        int recID = i2imap_get(entSerialize->conIDMap, conID);
        clientEntityRecordList_t *clEntList = &vecget(entSerialize->clientRecordList, recID);


        for(int e = 0; e < vecsize(clEntList->recordList); e++)
        {
            if(!bm_getBitVal(clEntList->bitmap.arr, e))
                continue;

            entityRecord_t *entRecord = &vecget(clEntList->recordList, e);


            streamRecent_close(&entRecord->recentRecord);
        }

        i2imap_remove(entSerialize->conIDMap, conID);

        bm_setBitVal(entSerialize->clientBitmap.arr, recID, 0);

        ent_freeClientEntList(conID, clEntList);
    }
}

void ent_removeSyncedEntFromClient(int entID, int conID, netcon_t *con, int entType)
{
    bitstream_t bs;
    entitySerializer_t *entSerializer = &(entSerializerList.list[entType]);


    // get the client entity record list
    int recID = i2imap_get(entSerializer->conIDMap, conID);
    clientEntityRecordList_t *clEntList = &vecget(entSerializer->clientRecordList, recID);


    for(int e = 0; e < vecsize(clEntList->recordList); e++)
    {
        if(!bm_getBitVal(clEntList->bitmap.arr, e))
            continue;

        
        entityRecord_t *entRecord = &vecget(clEntList->recordList, e);

        if(entRecord->entID == entID)
        {
            // zidfree(entRecord->recentRecord.stateChangeBm);
            streamRecent_close(&entRecord->recentRecord);
            streamQuick_begin(&clEntList->removeEntRecord, con, &bs);
            stream_writeInt(&bs, entID);
            streamQuick_end(&clEntList->removeEntRecord, con, &bs);
            bm_setBitVal(clEntList->bitmap.arr, e, 0);
            break;
        }
    }
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

    vecinit(GENERALZONE, vectorEntityList.posList, entVec_t, initSize);
    vecinit(GENERALZONE, vectorEntityList.movableList, entityMove_t, initSize);
    // vecinit(GENERALZONE, vectorEntityList.spriteList, entitySprite_t, initSize);
    vecinit(GENERALZONE, vectorEntityList.animSpriteList, animatedSprite_t, initSize);
    vecinit(GENERALZONE, vectorEntityList.shootTimerList, endTimer_t, initSize);
    vecinit(GENERALZONE, vectorEntityList.moveIDList, int, initSize);
    vecinit(GENERALZONE, vectorEntityList.posInterpolateList, positionInterpolate_t, initSize);
    vecinit(GENERALZONE, vectorEntityList.angleInterpolateList, angleInterpolate_t, initSize);
    // vecinit(GENERALZONE, vectorEntityList.rayEntIDList, int, initSize);
    vecinit(GENERALZONE, vectorEntityList.weaponOnHandList, weaponOnHand_t, initSize);
    vecinit(GENERALZONE, vectorEntityList.healthList, int, initSize);
    vecinit(GENERALZONE, vectorEntityList.weaponShotList, int, initSize);
    vecinit(GENERALZONE, vectorEntityList.bitmap, byte, initSize/8);
    vectorEntityList.mainEntMap = i2imap_init(GENERALZONE);
}


int ent_addVectorEntity()
{

    int id = bm_findEmpty(vectorEntityList.bitmap.arr, vecsize(vectorEntityList.bitmap));
    if(id < 0)
    {
        id = vecsize(vectorEntityList.posList);
        vecpushempty(vectorEntityList.posList, entVec_t);
        vecpushempty(vectorEntityList.movableList, entityMove_t);
        vecpushempty(vectorEntityList.animSpriteList, animatedSprite_t);
        vecpushempty(vectorEntityList.shootTimerList, endTimer_t);
        vecpushempty(vectorEntityList.posInterpolateList, positionInterpolate_t);
        vecpushempty(vectorEntityList.angleInterpolateList, angleInterpolate_t);
        vecpushempty(vectorEntityList.moveIDList, int);
        vecpushempty(vectorEntityList.weaponOnHandList, weaponOnHand_t);
        vecpushempty(vectorEntityList.weaponShotList, int);
        vecpushempty(vectorEntityList.healthList, int);
    }

    entVec_t *entPos = &vecget(vectorEntityList.posList, id);
    entityMove_t *moveEnt = &vecget(vectorEntityList.movableList, id);
    animatedSprite_t *sprite = &vecget(vectorEntityList.animSpriteList, id);
    endTimer_t *endTimer = &vecget(vectorEntityList.shootTimerList, id);
    positionInterpolate_t *posIntp = &vecget(vectorEntityList.posInterpolateList, id);
    angleInterpolate_t *angIntp = &vecget(vectorEntityList.angleInterpolateList, id);
    weaponOnHand_t *weaponOnHand = &vecget(vectorEntityList.weaponOnHandList, id);

    vecset(vectorEntityList.weaponShotList, id, 0);

    vec3xyz(entPos->pos, 300, 330, 300);
    vec3xyz(moveEnt->pos, 300, 330, 300);
    rect2xywh(moveEnt->rect, -5, -5, 10, 10);

    vec3set(moveEnt->pos, moveEnt->pos);
    vec3xyz(moveEnt->dir, 0, 0, 0);

    vec3set(sprite->pos, moveEnt->pos);
    // rect2set(sprite->rect, moveEnt->bound);
    rect2xywh(sprite->rect, -5, -5, 10, 10);
    startTimer(endTimer, 200);
    zmemset(posIntp, 0, sizeof(positionInterpolate_t));
    zmemset(angIntp, 0, sizeof(angleInterpolate_t));


    int moveID = physics_addBody(moveEnt);
    vecset(vectorEntityList.moveIDList, id, moveID);


    sprite->texID = 2;
    sprite->curSprite = 0;


    ent_setRayWeapon(&rayWeaponHandle, weaponOnHand, id, moveEnt);

    vecset(vectorEntityList.healthList, id, 100);

    bm_setBitVal(vectorEntityList.bitmap.arr, id, 1);

    return id;
}

void ent_removeVectorEntity(int entID)
{
    int moveID = vecget(vectorEntityList.moveIDList, entID);
    physics_removeBody(moveID);

    weaponOnHand_t *weaponOnHand = &vecget(vectorEntityList.weaponOnHandList, entID);
    ent_removeRayWeapon(&rayWeaponHandle, weaponOnHand);

    bm_setBitVal(vectorEntityList.bitmap.arr, entID, 0);
}

/********************RAY********************/

void ent_initRayRenderList()
{
    int initSize = ENT_INITSIZE;
    vecinit(GENERALZONE, renderRayList.xList, float, initSize);
    vecinit(GENERALZONE, renderRayList.yList, float, initSize);
    vecinit(GENERALZONE, renderRayList.xDirList, float, initSize);
    vecinit(GENERALZONE, renderRayList.yDirList, float, initSize);
    vecinit(GENERALZONE, renderRayList.endTimeList, unsigned long int, initSize);
}

void ent_initRayHandleList(rayHandleList_t *rayHandleList)
{
    int initSize = ENT_INITSIZE;
    
    emittedRayList_t *emittedRayList = &rayHandleList->emittedRayList;
    rayEntityList_t *rayEntityList = &rayHandleList->rayEntityList;
    rayHitList_t *rayHitList = &rayHandleList->rayHitList;

    vecinit(GENERALZONE, emittedRayList->entIDList, int, initSize);
    vecinit(GENERALZONE, emittedRayList->xList, float, initSize);
    vecinit(GENERALZONE, emittedRayList->yList, float, initSize);
    vecinit(GENERALZONE, emittedRayList->xDirList, float, initSize);
    vecinit(GENERALZONE, emittedRayList->yDirList, float, initSize);


    vecinit(GENERALZONE, rayEntityList->entIDList, int, initSize);
    vecinit(GENERALZONE, rayEntityList->entList, entityMove_t, initSize);
    vecinit(GENERALZONE, rayEntityList->entBitmap, byte, initSize/8);


    vecinit(GENERALZONE, rayHitList->fromList, int, initSize);
    vecinit(GENERALZONE, rayHitList->toList, int, initSize);
    vecinit(GENERALZONE, rayHitList->uList, float, initSize);
}


int ent_addRayEntity(rayHandleList_t *rayHandleList, int entID, entityMove_t *setMove)
{
    int id = bm_findEmpty(rayHandleList->rayEntityList.entBitmap.arr,
         vecsize(rayHandleList->rayEntityList.entBitmap));
    if(id < 0)
    {
        id = vecsize(rayHandleList->rayEntityList.entList);
        vecpushempty(rayHandleList->rayEntityList.entList, entityMove_t);
        vecpushempty(rayHandleList->rayEntityList.entIDList, int);
    }

    entityMove_t *entMove = &vecget(rayHandleList->rayEntityList.entList, id);
    vec3set(entMove->pos, setMove->pos);
    rect2set(entMove->rect, setMove->rect);
    vec3xyz(entMove->dir, 0, 0, 0);
    vecset(rayHandleList->rayEntityList.entIDList, id, entID);

    bm_setBitVal(rayHandleList->rayEntityList.entBitmap.arr, id, 1);
    
    return id;
}

void ent_removeRayEntity(rayHandleList_t *rayHandleList, int entID)
{
    bm_setBitVal(rayHandleList->rayEntityList.entBitmap.arr, entID, 0);
}

int ent_emitRay(rayHandleList_t *rayHandleList, int entID, float pos[2], float dir[2])
{
    int id = vecsize(rayHandleList->emittedRayList.xList);
    vecpush(rayHandleList->emittedRayList.xList, float, pos[0]);
    vecpush(rayHandleList->emittedRayList.yList, float, pos[1]);
    vecpush(rayHandleList->emittedRayList.xDirList, float, dir[0]);
    vecpush(rayHandleList->emittedRayList.yDirList, float, dir[1]);
    vecpush(rayHandleList->emittedRayList.entIDList, int, entID);

    return id;
}

void ent_resetRayList(rayHandleList_t *rayHandleList)
{
    vecreset(rayHandleList->emittedRayList.xList);
    vecreset(rayHandleList->emittedRayList.yList);
    vecreset(rayHandleList->emittedRayList.xDirList);
    vecreset(rayHandleList->emittedRayList.yDirList);
    vecreset(rayHandleList->emittedRayList.entIDList);
}

void ent_setHitEntity(rayHandleList_t *rayHandleList, int rayID, int fromID, int toID)
{
    vecpush(rayHandleList->rayHitList.fromList, int, fromID);
    vecpush(rayHandleList->rayHitList.toList, int, toID);
}

void ent_resetHitEntityList(rayHandleList_t *rayHandleList)
{
    vecreset(rayHandleList->rayHitList.fromList);
    vecreset(rayHandleList->rayHitList.toList);   
}


void ent_initKillList()
{
    int initSize = ENT_INITSIZE;
    vecinit(GENERALZONE, killIDList.entIDList, int, initSize);
}
int ent_addKillID(int entID)
{
    vecpush(killIDList.entIDList, int, entID);
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

void ent_setRayWeapon(rayWeaponHandle_t *weaponHandle, weaponOnHand_t *weaponOnHand, int entID, entityMove_t *moveEnt)
{
    startTimer(&weaponOnHand->currentShootEndTime, 0);
    startTimer(&weaponOnHand->nextShootEndTime, 0);
    weaponOnHand->curAngID = 0;
    weaponOnHand->ammoCount = 10;
    weaponOnHand->rayEntID = ent_addRayEntity(&weaponHandle->rayHandleList, entID, moveEnt);
}

void ent_setRayWeaponEntity(rayWeaponHandle_t *weaponHandle, weaponOnHand_t *weaponOnHand, entityMove_t *moveEnt)
{
    entityMove_t *rayEnt;
    rayEnt = &vecget(weaponHandle->rayHandleList.rayEntityList.entList, weaponOnHand->rayEntID);
    vec3set(rayEnt->pos, moveEnt->pos);
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

    angle += deg2rad(shootAngleList[weaponOnHand->curAngID & (shootAngListLen - 1)]);
    weaponOnHand->curAngID++;
    weaponOnHand->angle = angle;

    vec3setang2(dir, angle);
    vec3mult(dir, 50);

    ent_emitRay(&weaponHandle->rayHandleList, entID, pos, dir);

    weaponOnHand->ammoCount -= 1;

    printf("calling shoot\n");

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
    weaponType.nextShootDelay = 400;
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
    ent_initEntList();
    ent_initSpriteList();
    ent_initRayRenderList();
    // ent_initRayList();
    // ent_initMoveList();
    ent_initVectorRayWeapon();
    ent_initVectorEntityList();
    ent_initKillList();
}