#include "../basic/basic.h"
#include "engine.h"
#include "entity.h"

i2imap_t *translateIDMap;

worldSnapshotList_t worldSnapshotList;

void initESQueue(ESVarQueue *esQueue, int elemSize, int listSize, void (*interpolate)(void *obj, ESVar *e1, ESVar *e2, uint64_t timestamp)) {
    esQueue->varList = zidmalloc(GENERALZONE, elemSize * listSize);
    esQueue->tsList = (uint64_t*)zidmalloc(GENERALZONE, sizeof(uint64_t) * listSize);
    esQueue->listSize = listSize;
    esQueue->elemSize = elemSize;
    esQueue->start = 0;
    esQueue->end = 0;
    esQueue->interpolate = interpolate;
}

bool isESVQFull(ESVarQueue *esQueue) {
    return ((esQueue->end - esQueue->start) == esQueue->listSize);
}

bool isESVQEmpty(ESVarQueue *esQueue) {
    return esQueue->start == esQueue->end;
}

void popESVQueue(ESVarQueue *esQueue) {
    if(isESVQEmpty(esQueue)) return;
    esQueue->start++;
}

void getESVQLast(ESVarQueue *esQueue, void *ptr) {
    if(isESVQEmpty(esQueue)) return;
    // end points to the next write position, so last written is at (end - 1)
    int ind = ((esQueue->end - 1) % esQueue->listSize);
    void *start = esQueue->varList + ind * esQueue->elemSize;
    memcpy(ptr, start, esQueue->elemSize);
}

void addESVQueue(ESVarQueue *esQueue, void *val, long timestamp) {
    if(isESVQFull(esQueue)) {
        popESVQueue(esQueue);
    }
    int ind = (esQueue->end % esQueue->listSize);
    void *dest = esQueue->varList + ind * (esQueue->elemSize);
    memcpy(dest, val, esQueue->elemSize);
    esQueue->end++;
}

void initESDef(ESDef *esDef, int listSize, int snapshotCount) {
    esDef->stateList = (ESVarQueue*) zidmalloc(GENERALZONE, listSize *sizeof(ESVarQueue));
    esDef->listSize = listSize;
    esDef->curDef = 0;
    esDef->snapshotCount = snapshotCount;
}

void setupESDef(ESDef *esDef, enum ESDefState state, ESDiff *esDiff) {
    esDef->curDef = 0;
    esDef->curState = state;
}

void addESVar(ESDef *esDef, int elemSize, void (*interpolate)(void *obj, ESVar *e1, ESVar *e2, uint64_t timestamp)) {
    if(esDef->curState == ESDEF_COUNT) {
        esDef->curDef++;
    }
    if(esDef->curState == ESDEF_ADD) {
        initESQueue(&esDef->stateList[esDef->curDef], elemSize, esDef->snapshotCount, interpolate );
        esDef->curDef++;
    }
}

void handleESVar(ESDef *esDef, void *ptr) {
    ESVarQueue *esvq = &esDef->stateList[esDef->curDef];
    if(esDef->curState == ESDEF_WRITETOESV) {
        addESVQueue(esvq, ptr, esDef->timestamp);
    }
    if(esDef->curState == ESDEF_WRITETOENT) {
        getESVQLast(esvq, ptr);
    }
    if(esDef->curState == ESDEF_BSWRITESTATE) {
        printf("writing state %d %d\n", esDef->curDef, esDef->curState);
        ESDiff *esDiff = esDef->esDiff;
        ESVarQueue *esQueue = &esDef->stateList[esDef->curDef];
        if(esDiff->shouldSend[esDef->curDef]) {
            stream_writeBit(esDef->bs, 1);
            for(int i = 0; i < esQueue->elemSize; i++) {
                stream_writeByte(esDef->bs, ((byte *)ptr)[i]);
            }
        }
        else {
            stream_writeBit(esDef->bs, 0);
        }
    }
    if(esDef->curState == ESDEF_BSREADSTATE) {
        ESDiff *esDiff = esDef->esDiff;
        ESVarQueue *esQueue = &esDef->stateList[esDef->curDef];
        int bit = stream_readBit(esDef->bs);
        if(bit) {
            for(int i = 0; i < esQueue->elemSize; i++) {
                ((byte *)ptr)[i] = stream_readByte(esDef->bs);
            }
        }
    }
    if(esDef->curState == ESDEF_BSTRACKSTATE) {
        ESDiff *esDiff = esDef->esDiff;
        byte *lastElem = (byte *)esvq->varList + (esvq->end - 1) * esvq->elemSize;
        int diff = memcmp(ptr, lastElem, esvq->elemSize);
        if(diff != 0) {
            esDiff->shouldSend[esDef->curDef] = true;
        }
    }
    if(esDef->curState == ESDEF_BSRESETSTATE) {
        ESDiff *esDiff = esDef->esDiff;
        esDiff->shouldSend[esDef->curDef] = false;
    }

    esDef->curDef++;
}

// void addNetEntry(NetObj *netObj, int size) {
//     // printf("addNetEntry %p\n", netObj);
//     NetEntry *entry = (NetEntry *)zidmalloc(GENERALZONE, sizeof(NetEntry));
//     entry->buf = (byte *)zidmalloc(GENERALZONE, size);
//     memset(entry->buf, 0, size);
//     entry->size = size;
//     entry->next = NULL;
//     entry->stateSet = false;
//     if(netObj->entryHead == NULL) {
//         netObj->entryHead = entry;
//         netObj->entryTail = entry;
//         entry->id = 0;
//     }
//     else {
//         // printf("inside else %p\n", netObj->curEntry);
//         netObj->entryTail->next = entry;
//         netObj->entryTail = entry;
//         // printf("before \n");
//         entry->id = netObj->curEntry->id + 1;
//         // printf("after \n");
//     }
//     // netObj->curState = WRITE;
//     netObj->curEntry = entry;
    
// }

//stream_writeByte
// void handleNetEntry(NetObj *netObj, void *ptr, int size, VectorEntity *vecEnt) {
//     // printf("handleNetEntry %p %p %d %d %p %p\n", netObj, ptr, size, vecEnt->typeID, netObj->curEntry, NULL);
//     if(netObj->curState == WRITE_INIT || netObj->curState == READ_INIT) {
//         netObj->stateCount++;
//         addNetEntry(netObj, size);
//         return;
//     }
    
//     if (netObj->curState == WRITE) {
//         // printf("WRITE\n");
//         // if(netObj->curEntry->id == 0)
//         //     printf("WRITING POS %f \n", *(float *)ptr);
//         if(netObj->curEntry->stateSet) {
//             // printf("writing state value %d \n", netObj->curEntry->id);
//             int entryCount = 0;
//             // memcpy(netObj->curEntry->buf, ptr, size);
//             for(int i = 0; i < size; i++) {
//                 stream_writeByte(netObj->bs, ((byte *)ptr)[i]);
//             }
//         }
//         if(netObj->curEntry->id == 0) {
//             stream_writeInt(netObj->bs, 34124214);
//         }
//     }
//     if (netObj->curState == READ) {
//         if(netObj->curEntry->stateSet) {
//             // printf("reading state value %d \n", netObj->curEntry->id);
//             for(int i = 0; i < size; i++) {
//                 ((byte *)ptr)[i] = stream_readByte(netObj->bs);
//             }
//         }
//         if(netObj->curEntry->id == 0) {
//             int test = stream_readInt(netObj->bs);
//             // printf("READING POS %f %d\n", *(float *)ptr, test);

//         }

//         // memcpy(netObj->curEntry->buf, ptr, size);
//     }
//     if(netObj->curState == TRACK_WRITE_DIFF) {
//         int diff = memcmp(ptr, netObj->curEntry->buf, size);
//         // printf("TRACK_WRITE_DIFF %d\n", diff != 0);
//         if(diff != 0) {
//             // ent_setStateFlags(VECTOR_SERIALIZER, vecEnt->entID, 5, 1);
//             netObj->curEntry->stateSet = true;
//             // memset(netObj->curEntry->buf, 0, size);
//             // bitstream_t bs;
//             // stream_init(&bs, netObj->curEntry->buf, size);
//             // stream_writeBytes(&bs, );
//         }

//         if(netObj->curEntry->stateSet) {
//             // printf("writing state value %d %d\n", netObj->conID, netObj->curEntry->id);
//         }

//         stream_writeBit(netObj->bs, netObj->curEntry->stateSet);
//     }
//     // if(netObj->curState == TRACK_WRITE_DIFF || netObj->curEntry->stateSet) {
//     //     int diff = memcmp(ptr, netObj->curEntry->buf, size);
//     //     if(diff != 0) {
//     //         // ent_setStateFlags(VECTOR_SERIALIZER, vecEnt->entID, 5, 1);
//     //         netObj->curEntry->stateSet = true;
//     //         stream_writeBit(netObj->bs, 1);
//     //     }
//     //     else {
//     //         stream_writeBit(netObj->bs, 1);
//     //     }
//     //     // if(diff != 0) {
//     //     //     printf("diff found %d %d\n", diff, size);
//     //     //     stream_writeInt(netObj->bs, 1);
//     //     // } else {
//     //     //     stream_writeInt(netObj->bs, 0);
//     //     // }
//     // }
//     if(netObj->curState == TRACK_READ_DIFF) {
//         // printf("TRACK_")
//         netObj->curEntry->stateSet = stream_readBit(netObj->bs);
//         // printf("checking if state is set %d \n", netObj->curEntry->stateSet);
//     }
//     if(netObj->curState == SETTLE_DIFF) {
//         int diff = memcmp(ptr, netObj->curEntry->buf, size);
//         // if(netObj->curEntry->stateSet) {
//             // printf("SETTLE_DIFF %d\n", diff != 0);
//             memcpy(netObj->curEntry->buf, ptr, size);
//         // }
//     }

//     netObj->curEntry = netObj->curEntry->next;
//     // netObj->curState = WRITE;
// }

// void initNetworkEntity(NetObj *netObj, VectorEntity *ent, bool isWrite) {
//     netObj->curState = isWrite ? WRITE_INIT : READ_INIT;
//     netObj->entryHead = NULL;
//     netObj->entryTail = NULL;
//     netObj->curEntry = NULL;
//     netObj->bs = NULL;
//     netObj->stateCount = 0;
// }

// void resetNetworkStateBits(NetObj *netObj) {
//     NetEntry *netEntry = netObj->entryHead;
//     int i = 0;
//     while(netEntry != NULL) {
//         netEntry->stateSet = false;
//         netEntry->id = i++;
//         netEntry = netEntry->next;
//     }
// }

// void setupNetworkEntity(NetObj *netObj, VectorEntity *ent, bitstream_t *bs, int state) {
//     netObj->bs = bs;
//     netObj->curState = state;
//     netObj->curEntry = netObj->entryHead;
//     if(state == TRACK_WRITE_DIFF || state == TRACK_READ_DIFF) {
//         resetNetworkStateBits(netObj);
//     }
//     // NetEntry *netEntry = netObj->entryHead;
//     // int i = 0;
//     // while(netEntry != NULL) {
//     //     netEntry->stateSet = false;
//     //     netEntry->id = i++;
//     //     netEntry = netEntry->next;
//     // }
// }

// void setNetworkEntityChanged(NetObj *netObj, byte *stateBm) {
//     // printf("checking set %p %p %d \n", netObj, stateBm, netObj->stateCount);
//     NetEntry *netEnt = netObj->entryHead;
//     for(int i = 0; i < netObj->stateCount; i++)
//     {
//         // printf("state index %d \n", i);
//         // check if state is changed
//         if(bm_getBitVal(stateBm, i))
//         {
//             netEnt->stateSet = true;
//         }
//         netEnt = netEnt->next;
//     }
// }



void addEntityRecord(serv_clrep_t *newClRep, VectorEntity *vecEnt) {
    worldSnapshot_t *worldSnapshot = &vecget(worldSnapshotList.list, newClRep->worldSnapshotID);
    bitstream_t newWriteBs;
    NetEntity *netEnt = &netEntityList[vecEnt->entID];
    printf("writing new netEnt \n");
    int entRecID = vecsize(worldSnapshot->recordList);
    vecpushempty(worldSnapshot->recordList, entityRecord_t);

    // bm_setBitVal(clEntList->bitmap.arr, entRecID, 1);
    entityRecord_t *entRecord = &vecget(worldSnapshot->recordList, entRecID);
    entRecord->entID = vecEnt->entID;

    byte *stateChangeBm = (byte *) zidmalloc(GENERALZONE, (int)CEIL(((float)netEnt->esDef.listSize)/8.0));
    streamRecent_init(&entRecord->recentRecord, 8, NULL, NULL, stateChangeBm);    
    // set the entity ID
    streamQuick_begin(&worldSnapshot->newEntRecord, newClRep->con, &newWriteBs);
    stream_writeInt(&newWriteBs, vecEnt->entID);
    stream_writeInt(&newWriteBs, vecEnt->typeID);
    stream_writeBit(&newWriteBs, netEnt->clientOwner == newClRep->conID);
    // stream_writeBit(&newWriteBs, netEnt->isPuppet);
    streamQuick_end(&worldSnapshot->newEntRecord, newClRep->con, &newWriteBs);
}

// read list of new entities that should be created
void ent_readNewEntList(
    serv_clrep_t *newClRep,
    bitstream_t *bs
    )
{
    // get the particular client entity record list
    worldSnapshot_t *worldSnapshot = &vecget(worldSnapshotList.list, newClRep->worldSnapshotID);

    
    // get the number of records that were sent
    int recordCount = streamQuick_readCount(&worldSnapshot->newEntRecord, bs);

    for(int i = 0; i < recordCount; i++)
    {
        // get the server entity id
        int remoteEntID = stream_readInt(bs);
        int typeID = stream_readInt(bs);
        int isOwner = stream_readBit(bs);

        // check if the server entity id already exists, which means
        // entity is already created
        int clientEntID = i2imap_get(translateIDMap, remoteEntID);

        if(clientEntID != -1)
            continue;
    
        
        // clientEntID = entSerializer->applyInitParam(clientEntID);
        // clientEntID = entSerializer->applyInitParam();
        printf("client ent id %d %d %d\n", clientEntID, typeID, isOwner);
        VectorEntity *vecEnt = addSprite(typeID, NULL, false, !isOwner);
        clientEntID = vecEnt->entID;

        printf("creating entity %d %d \n", remoteEntID, typeID);
        // if(isOwner) {
        //     VectorEntity *vecEnt = addSprite(typeID, NULL, false);
        //     clientEntID = vecEnt->entID;
        //     printf("creating entity %d %d \n", remoteEntID, typeID);
        // }
        // else {
        //     VectorEntity *vecEnt = addSprite(PUPPET_TYPE, NULL, false);
        //     clientEntID = vecEnt->entID;
        //     printf("adding puppet \n");
        // }

        // call the function of the serializer that reads initialization
        // params and creates entities


        // if no entities are created then skip
        if(clientEntID < 0)
            continue;

        // add mapping for the server entity id to the client entity id
        i2imap_put(translateIDMap, remoteEntID, clientEntID);
    }
}

// client reads the state of entities that are sent by the server
void ent_readEntStateList(bitstream_t *bs)
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
        int translatedID = i2imap_get(translateIDMap, remoteEntID);

        //read the states that were sent
        // streamRecent_readStateBits(entSerializer->stateLen, bs, stateBm);

        // call readState function that reads the states sent based
        // on the state flags
        // entSerializer->readState(translatedID, bs, stateBm);

        // printf("translatedID %d %d\n", translatedID, remoteEntID);
        VectorEntity *vecEnt = &vectorEntityList[translatedID];
        NetEntity *netEnt = &netEntityList[translatedID];

        // setupNetworkEntity(&netEnt->netObj, vecEnt, bs, TRACK_READ_DIFF);

        setupESDef(&netEnt->esDef, ESDEF_BSREADSTATE, NULL);
        spriteFactoryList[vecEnt->typeID].processState(vecEnt, &netEnt->esDef);

    
        // if(netEnt->isPuppet) {
        //     spriteFactoryList[vecEnt->typeID].puppetSerializer(vecEnt, &netEnt->netObj);
        // }
        // else {
        //     spriteFactoryList[vecEnt->typeID].serializer(vecEnt, &netEnt->netObj);
        // }
        // setupNetworkEntity(&netEnt->netObj, vecEnt, bs, READ);
        // // setNetworkEntityChanged(&netEnt->netObj, stateBm, entSerializer->stateLen);
        // // handleNetworkEntity(&vecEnt->netObj, vecEnt);
        // if(netEnt->isPuppet) {
        //     spriteFactoryList[vecEnt->typeID].puppetSerializer(vecEnt, &netEnt->netObj);
        // }
        // else {
        //     spriteFactoryList[vecEnt->typeID].serializer(vecEnt, &netEnt->netObj);
        // }
        
    }
}

// read list of new entities that should be created
void ent_readRemoveEntList(
    serv_clrep_t *newClRep,
    bitstream_t *bs
    )
{
    // get the particular client entity record list
    // clientEntityRecordList_t *clEntList = &newClRep->clientEntRecordList;
    worldSnapshot_t *worldSnapshot = &vecget(worldSnapshotList.list, newClRep->worldSnapshotID);

    
    // get the number of records that were sent
    int recordCount = streamQuick_readCount(&worldSnapshot->removeEntRecord, bs);


    for(int i = 0; i < recordCount; i++)
    {
        // get the server entity id
        int remoteEntID = stream_readInt(bs);


        // check if the server entity id already exists, which means
        // entity is already created
        int clientEntID = i2imap_get(translateIDMap, remoteEntID);

        // if no entities are created then skip
        if(clientEntID < 0)
            continue;


        VectorEntity *vecEnt = getVectorEntity(clientEntID);
        
        spriteFactoryList[vecEnt->typeID].cleanup(vecEnt);

        // add mapping for the server entity id to the client entity id
        i2imap_remove(translateIDMap, remoteEntID);
    }
}

// read the commands for creating, reading state of entities
void ent_readSerializerList(serv_clrep_t *newClRep, bitstream_t *bs)
{
    byte cmd;
    while((cmd = stream_readByte(bs)) != ENTCMD_END)
    {
        // printf("checking cmd %d \n", cmd);
        switch(cmd)
        {
            case ENTCMD_NEW:
                ent_readNewEntList(newClRep, bs);
                break;
            case ENTCMD_REMOVE:
                ent_readRemoveEntList(newClRep, bs);
                break;
            case ENTCMD_STATE:
                ent_readEntStateList(bs);
                break;
            default:
                com_error(ERR_FATAL, "Error: wrong ent command received %d %d %d\n", cmd, ENTCMD_STATE, ENTCMD_NEW);
        }
    }
}

// void ent_settleStateDiff() {
//     byte stateBm[32];
//     bitstream_t bs;

//     stream_init(&bs, stateBm, 32);
//     for(int i = 0; i < VECTOR_ENTITY_COUNT; i++) {
//         VectorEntity *vecEnt = &vectorEntityList[i];
//         NetEntity *netEnt = &netEntityList[i];
//         if(!vecEnt->active) continue;

//         // setupNetworkEntity(&netEnt->netObj, vecEnt, bs, TRACK_READ_DIFF);

//         // if(netEnt->isPuppet) {
//         //     spriteFactoryList[vecEnt->typeID].puppetSerializer(vecEnt, &netEnt->netObj);
//         // }
//         // else {
//         //     spriteFactoryList[vecEnt->typeID].serializer(vecEnt, &netEnt->netObj);
//         // }

//         setupESDef(&netEnt->esDef, )
//         // setupNetworkEntity(&netEnt->netObj, vecEnt, &bs, TRACK_WRITE_DIFF);

//         // if(!netEnt->isPuppet) {
//         // spriteFactoryList[vecEnt->typeID].serializer(vecEnt, &netEnt->netObj);
//         // }
//         // else {
//         //     spriteFactoryList[vecEnt->typeID].puppetSerializer(vecEnt, &netEnt->netObj);
//         // }


//         // setupNetworkEntity(&netEnt->netObj, vecEnt, &bs, SETTLE_DIFF);
//         // // setNetworkEntityChanged(&netEnt->netObj, stateBm, entSerializer->stateLen);
//         // // handleNetworkEntity(&vecEnt->netObj, vecEnt);
//         // if(!netEnt->isPuppet) {
//         //     spriteFactoryList[vecEnt->typeID].puppetSerializer(vecEnt, &netEnt->netObj);
//         // }
//         // else {
//         //     spriteFactoryList[vecEnt->typeID].serializer(vecEnt, &netEnt->netObj);
//         // }
//     }
// }

// write list of new entities for sending to a client
void ent_writeNewEntList(serv_clrep_t *newClRep, bitstream_t *bs)
{
    // get the client entity record list
    // clientEntityRecordList_t *clEntList = &newClRep->clientEntRecordList;
    worldSnapshot_t *worldSnapshot = &vecget(worldSnapshotList.list, newClRep->worldSnapshotID);
    bitstream_t newWriteBs;

    for(int i = 0; i < VECTOR_ENTITY_COUNT; i++) {
        NetEntity *netEnt = &netEntityList[i];
        VectorEntity *vecEnt = &vectorEntityList[i];

        if(netEnt->isNew) {
            addEntityRecord(newClRep, vecEnt);
        }
    }

    // if there is nothing to write then return
    if(streamQuick_getRecordCount(&worldSnapshot->newEntRecord) <= 0) {
        return;
    }


    // write new entity command
    stream_writeByte(bs, ENTCMD_NEW);

    // write the stored list of new entities and their initialization params
    streamQuick_writePacket(&worldSnapshot->newEntRecord, bs, newClRep->con);

    printf("write new entity list %d %d \n", ENTCMD_NEW, worldSnapshot->newEntRecord.recordCount);
}

// write list of new entities for sending to a client
void ent_writeRemoveEntList(serv_clrep_t *newClRep, bitstream_t *bs)
{
    // get the client entity record list
    // clientEntityRecordList_t *clEntList = &newClRep->clientEntRecordList;
    worldSnapshot_t *worldSnapshot = &vecget(worldSnapshotList.list, newClRep->worldSnapshotID);

    
    // if there is nothing to write then return
    if(streamQuick_getRecordCount(&worldSnapshot->removeEntRecord) <= 0) {
        return;
    }


    // write new entity command
    stream_writeByte(bs, ENTCMD_REMOVE);

    // write the stored list of new entities and their initialization params
    streamQuick_writePacket(&worldSnapshot->removeEntRecord, bs, newClRep->con);
}

// write a list of entity states for the client
void ent_writeStateList(serv_clrep_t *newClRep, bitstream_t *bs)
{

    // if(newClRep->conID == 0) {
    //     // printf("checking ent record %d \n", e);
    //     return;
    // }

    byte stateBm[32];
    // get the client entity record list
    // clientEntityRecordList_t *clEntList = &newClRep->clientEntRecordList;
    worldSnapshot_t *worldSnapshot = &vecget(worldSnapshotList.list, newClRep->worldSnapshotID);

    // get the count of entities to write
    int entLen = 0;
    for(int e = 0; e < vecsize(worldSnapshot->recordList); e++)
    {
        entityRecord_t *entRecord = &vecget(worldSnapshot->recordList, e);
        if(!entRecord->active) {
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
    for(int e = 0; e < vecsize(worldSnapshot->recordList); e++)
    {
        entityRecord_t *entRecord = &vecget(worldSnapshot->recordList, e);
        if(!entRecord->active)
        {
            continue;
        }

        // get the entity record from the client record list
        // entityRecord_t *entRecord = &vecget(worldSnapshot->recordList, e);


        // get the entity state that should be sent to all clients
        // entityStateBitmap_t entState = vecget(entSerializer->entityStateList, entRecord->entID);


        // write the entity id
        stream_writeInt(bs, entRecord->entID);


        // save the state bits
        // streamRecent_setStateBits(&entRecord->recentRecord, entState.state);

        

        // write the entity state to the bitstream
        streamRecent_writeStateBits(&entRecord->recentRecord, bs, newClRep->con, stateBm);


        // write the entity state for the state bits that are set
        // entSerializer->writeState(entRecord->entID, bs, stateBm, conID);

        VectorEntity *vecEnt = &vectorEntityList[entRecord->entID];
        NetEntity *netEnt = &netEntityList[entRecord->entID];

        for(int i = 0; i < netEnt->esDef.listSize; i++) {
            entRecord->esDiff.shouldSend[i] = bm_getBitVal(stateBm, i);
        }

        setupESDef(&netEnt->esDef, ESDEF_BSWRITESTATE, &entRecord->esDiff);
        spriteFactoryList[vecEnt->typeID].processState(vecEnt, &netEnt->esDef);

        // setupNetworkEntity(&netEnt->netObj, vecEnt, bs, TRACK_WRITE_DIFF);
        // setNetworkEntityChanged(&netEnt->netObj, stateBm);

        // netEnt->netObj.conID = newClRep->conID;
        // if(netEnt->clientOwner == newClRep->conID) {
        // spriteFactoryList[vecEnt->typeID].serializer(vecEnt, &netEnt->netObj);
        // }
        // else {
        //     spriteFactoryList[vecEnt->typeID].puppetSerializer(vecEnt, &netEnt->netObj);
        // }

        // setupNetworkEntity(&netEnt->netObj, vecEnt, bs, WRITE);
        // // setNetworkEntityChanged(&vecEnt->netObj, stateBm, entRecord->recentRecord.stateBitLen);
        // // handleNetworkEntity(&vecEnt->netObj, vecEnt);
        // if(netEnt->clientOwner == newClRep->conID) {
        //     spriteFactoryList[vecEnt->typeID].serializer(vecEnt, &netEnt->netObj);
        // }
        // else {
        //     spriteFactoryList[vecEnt->typeID].puppetSerializer(vecEnt, &netEnt->netObj);
        // }
    }
}

// All serializers write the list of new entities to create, and the entity state
void ent_writeSerializerList(serv_clrep_t *newClRep, bitstream_t *bs)
{
        // write the list of new entities to create
        ent_writeNewEntList(newClRep, bs);


        // write the list of new entities to remove
        ent_writeRemoveEntList(newClRep, bs);
        

        // write the entity state
        ent_writeStateList(newClRep,  bs);


        stream_writeByte(bs, ENTCMD_END);
}

// go through all entity records of a client, and acknowledge entity states sent
void ent_ackSerializerList( serv_clrep_t *newClRep, bitstream_t *bs)
{
    // clientEntityRecordList_t *clEntList = &newClRep->clientEntRecordList;
    worldSnapshot_t *worldSnapshot = &vecget(worldSnapshotList.list, newClRep->worldSnapshotID);


    streamQuick_acknowledge(&(worldSnapshot->newEntRecord), newClRep->con);

    streamQuick_acknowledge(&(worldSnapshot->removeEntRecord), newClRep->con);

    // loop through the entity records for the client
    for(int e = 0; e < vecsize(worldSnapshot->recordList); e++)
    {
        entityRecord_t *entRecord = &vecget(worldSnapshot->recordList, e);
        if(!entRecord->active)
        {
            continue;
        }


        // get the entity record and call acknowledge, for the states
        // that were recieved by the client
        streamRecent_acknowledge(&(entRecord->recentRecord), newClRep->con);
    }
}

void ent_initializeClient(serv_clrep_t *newClRep) {
    // clientEntityRecordList_t *clEntList = &newClRep->clientEntRecordList;
    worldSnapshot_t *worldSnapshot = &vecget(worldSnapshotList.list, newClRep->worldSnapshotID);

    for(int i = 0; i < VECTOR_ENTITY_COUNT; i++) {
        VectorEntity *vecEnt = &vectorEntityList[i];
        if(!vecEnt->active) continue;

        addEntityRecord(newClRep, vecEnt);

    }
}


void resetNetEnt() {
    for(int i = 0; i < VECTOR_ENTITY_COUNT; i++) {
        VectorEntity *vecEnt = &vectorEntityList[i];
        NetEntity *netEnt = &netEntityList[i];
        if(!vecEnt->active) continue;

        
        if(netEnt->isNew) {
            // printf("new entity %d \n", i);
            netEnt->isNew = false;
        }
        if(netEnt->isRemoved) {
            // printf("removed entity %d \n", i);
            netEnt->isRemoved = false;
        }
    }
}

void netEntSys_init()
{
    translateIDMap = i2imap_init(GENERALZONE);
    vecinit(GENERALZONE, worldSnapshotList.list, worldSnapshot_t, VECTOR_ENTITY_COUNT);
    for(int i = 0; i < VECTOR_ENTITY_COUNT; i++) {
        worldSnapshot_t *worldSnapshot = &vecget(worldSnapshotList.list, i);
        worldSnapshot->active = false;
        // worldSnapshot->snapshotIndex = 0;
    }
}

int neEntSys_initSnapshot() {
    vecpushempty(worldSnapshotList.list, worldSnapshot_t);
    int size = vecsize(worldSnapshotList.list) - 1;
    worldSnapshot_t *worldSnapshot = &vecget(worldSnapshotList.list, size);
    

    vecinit(GENERALZONE, worldSnapshot->recordList, entityRecord_t, 8);
    // vecinit(GENERALZONE, worldSnapshot->bitmap, byte, 8);
    streamQuick_init(&worldSnapshot->newEntRecord, NULL, NULL);
    streamQuick_init(&worldSnapshot->removeEntRecord, NULL, NULL);
    worldSnapshot->active = true;
    return size;
}

/* Stub implementation for legacy handleNetEntry function */
void handleNetEntry(NetObj *netObj, void *ptr, int size, VectorEntity *vecEnt) {
    // This function was used in the old serialization system
    // Now replaced by handleESVar for the ESVar system
    // Keeping as stub for backwards compatibility
}