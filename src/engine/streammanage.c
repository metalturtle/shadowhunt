#include "../basic/basic.h"
#include "engine.h"

server_t server;
client_t client;

byte writeBuf[MAX_MSGLEN];

/********************QUICKEST STREAM********************/

void streamQuick_init(quickStreamRecord_t *quickRecord, int (*readFunc)(bitstream_t *, void *), int (*writeFunc)(bitstream_t *, void *))
{
    quickRecord->head = quickRecord->tail = NULL;
    quickRecord->recordCount = 0;
    quickRecord->readFunc = readFunc;
    quickRecord->writeFunc = writeFunc;
    quickRecord->data = NULL;
}

int streamQuick_getRecordCount(quickStreamRecord_t *quickRecord)
{
    return quickRecord->recordCount;
}

void streamQuick_begin(quickStreamRecord_t *quickRecord, netcon_t *con, bitstream_t *bs)
{
    byte *buf;

    // buf = (byte *) zidmalloc(TEMPORARYZONE, MAX_SEGMENTLEN);
    stream_init(bs, writeBuf, MAX_SEGMENTLEN);
}

void streamQuick_end(quickStreamRecord_t *quickRecord, netcon_t *con, bitstream_t *bs)
{
    quickStreamNode_t *newData;
    float bitLen;
    int byteLen;

    newData = (quickStreamNode_t *) zidmalloc(TEMPORARYZONE, sizeof(quickStreamNode_t));

    if(quickRecord->head == NULL)
    {
        quickRecord->head = newData;
        quickRecord->tail = newData;
    }
    else
    {
        quickRecord->tail->next = newData;
        quickRecord->tail = newData;
    }

    newData->next = NULL;
    newData->sendSequence = con->outgoingSequence;
    quickRecord->recordCount++;


    memcpy(&quickRecord->bs, bs, sizeof(bitstream_t));

    bitLen = quickRecord->bs.curbyte*8 + quickRecord->bs.curbit;
    byteLen = quickRecord->bs.curbyte + 1;

    newData->data = (byte *) zidmalloc(TEMPORARYZONE, byteLen);
    newData->bitLen = (int) bitLen;

    zmemcpy(newData->data, quickRecord->bs.buf, byteLen);

    // zidfree(quickRecord->bs.buf);
}

int streamQuick_callWriteFunc(quickStreamRecord_t *quickRecord, netcon_t *con, void *data)
{
    quickStreamNode_t *newData;
    byte *buf;
    float bitLen;
    int byteLen;
    bitstream_t bs;

    buf = (byte *) zidmalloc(TEMPORARYZONE, MAX_SEGMENTLEN);
    stream_init(&bs, buf, MAX_SEGMENTLEN);

    int ret = quickRecord->writeFunc(&bs, data);

    if(!ret)
    {
        zidfree(buf);
        return 0;
    }

    newData = (quickStreamNode_t *) zidmalloc(TEMPORARYZONE, sizeof(quickStreamNode_t));

    if(quickRecord->head == NULL)
    {
        quickRecord->head = newData;
        quickRecord->tail = newData;
    }
    else
    {
        quickRecord->tail->next = newData;
        quickRecord->tail = newData;
    }

    newData->next = NULL;
    newData->sendSequence = con->outgoingSequence;
    quickRecord->recordCount++;

    bitLen = bs.curbyte*8 + bs.curbit;
    byteLen = bs.curbyte + 1;

    newData->data = (byte *) zidmalloc(TEMPORARYZONE, byteLen);
    newData->bitLen = (int) bitLen;

    zmemcpy(newData->data, bs.buf, byteLen);

    zidfree(buf);
    
    return 1;
}

void streamQuick_writePacket(quickStreamRecord_t *quickRecord, bitstream_t *bs, netcon_t *con)
{
    quickStreamNode_t *curdata;

    // streamQuick_callWriteFunc(quickRecord, con);

    if(quickRecord->recordCount == 0)
    {
        stream_writeBit(bs, 0);
        return;
    }
    else {
        stream_writeBit(bs, 1);
    }

    stream_writeByte(bs, quickRecord->recordCount);

    for(curdata = quickRecord->head; curdata != NULL; curdata = curdata->next)
    {
        stream_writeBitsData(bs, curdata->data, curdata->bitLen);
    }
}

void streamQuick_addPayload(quickStreamRecord_t *quickRecord, void *data)
{
    quickRecord->data = data;
}

void streamQuick_readPacket(quickStreamRecord_t *quickRecord, bitstream_t *bs)
{
    byte isSent;
    int recordCount;
    quickStreamNode_t newData;
    quickStreamNode_t *curdata;

    if(!stream_readBit(bs))
    {
        return;
    }

    recordCount = stream_readByte(bs);

    for(int i = 0; i < recordCount; i++)
    {
        quickRecord->readFunc(bs, quickRecord->data);
    }
    
    quickRecord->data = NULL;
}

void streamQuick_acknowledge(quickStreamRecord_t *quickRecord, netcon_t *con)
{
    quickStreamNode_t *curData, *nextData;
    quickStreamNode_t *lastSent = NULL;
    int lastseq = 0;

    if(!quickRecord->head)
        return;

    for(curData = quickRecord->head; curData != NULL; curData = curData->next)
    {
        if(netcon_getPacketState(con, curData->sendSequence) == NETCON_PACKET_SUCCESS)
        {
            lastSent = curData;
            lastseq = curData->sendSequence;
        }
    }

    printf("recv reqd data: lastrecvseq: %d \n", lastseq);

    if(lastSent == NULL)
    {
        return;
    }

    curData = quickRecord->head;
    while(curData)
    {
        quickRecord->recordCount--;
        
        printf("freeing curdata %p\n", curData);

        nextData = curData->next;
        zidfree(curData->data);
        zidfree(curData);
        if(curData == lastSent)
        {
            quickRecord->head = nextData;
            break;
        }

        curData = nextData;
    }

    if(lastSent == quickRecord->tail)
    {
        quickRecord->head = quickRecord->tail = NULL;
        quickRecord->recordCount = 0;
        return;
    }
}


/********************REQUIRED STREAM********************/

void streamReliable_init(relStreamRecord_t *relRecord, int (*readFunc)(bitstream_t *), int (*writeFunc)(bitstream_t *))
{
    relRecord->windowStartRecordID = 0;
    relRecord->recordCount = 0;
    relRecord->readFunc = readFunc;
    relRecord->writeFunc = writeFunc;
    relRecord->lastRecordID = 0;
}

int streamReliable_callWriteFunc(relStreamRecord_t *relRecord, netcon_t *con)
{
    relStreamNode_t *newData;
    byte *buf;
    float bitLen;
    int byteLen;
    bitstream_t bs;

    buf = (byte *) zidmalloc(TEMPORARYZONE, MAX_SEGMENTLEN);
    stream_init(&bs, buf, MAX_SEGMENTLEN);

    int ret = relRecord->writeFunc(&bs);

    if(!ret)
    {
        zidfree(buf);
        return 0;
    }

    newData = (relStreamNode_t *) zidmalloc(TEMPORARYZONE, sizeof(relStreamNode_t));
    if(relRecord->head == NULL)
    {
        relRecord->head = newData;
        relRecord->tail = newData;
    }
    else
    {
        // printf("adding record %d to tail %d\n",relRecord->lastRecordID+1, relRecord->tail->recordID);
        relRecord->tail->next = newData;
        relRecord->tail = newData;
    }

    newData->next = NULL;
    newData->recordID = ++relRecord->lastRecordID;
    newData->isSent = 0;
    relRecord->recordCount++;

    bitLen = bs.curbyte*8 + bs.curbit;
    byteLen = (int) CEIL(bitLen/8.0);

    newData->data = (byte *) zidmalloc(TEMPORARYZONE, byteLen);
    newData->bitLen = (int) bitLen;

    zmemcpy(newData->data, bs.buf, byteLen);

    zidfree(buf);
    return 1;
}

void streamReliable_writePacket(relStreamRecord_t *relRecord ,bitstream_t *bs, netcon_t *con)
{
    relStreamNode_t *curdata;
    int wind = 0;
    byte sent = 0;

    // streamReliable_callWriteFunc(relRecord, con);

    if(!relRecord->head)
    {
        stream_writeBit(bs, 0);
        return;
    }
    
    wind = (relRecord->head->recordID - 1) - relRecord->windowStartRecordID;
    printf("wind=%d, firstrec=%d, windowrec=%d\n", wind, relRecord->head->recordID, relRecord->windowStartRecordID);
    for(curdata = relRecord->head; (curdata != NULL); curdata = curdata->next)
    {
        printf("writeRelStream, curdata=%p, status=%d, record=%d, issent=%d\n", curdata,
         netcon_getPacketState(con, curdata->sendSequence)
         , curdata->recordID
         , curdata->isSent);
        if(curdata->recordID > (relRecord->windowStartRecordID + RELSTREAMWINDSIZE))
        {
            break;
        }
        if(!curdata->isSent)
        {
            stream_writeBit(bs, 1);
            stream_writeVarLong(bs, curdata->recordID);
            stream_writeVarLong(bs, curdata->bitLen);
            stream_writeBitsData(bs, curdata->data, curdata->bitLen);
            curdata->isSent = 1;
            curdata->sendSequence = con->outgoingSequence;
            sent = 1;
            printf("writeRelStream, sending new packet, record=%d, sequence=%d\n", curdata->recordID, curdata->sendSequence);
            break;
        }
    }

    if(!sent)
    {
        wind = (relRecord->head->recordID - 1) - relRecord->windowStartRecordID;
        printf("resending\n");
        for(curdata = relRecord->head; (curdata != NULL); curdata = curdata->next)
        {
            printf("writeRelStream, curdata=%p, status=%d, record=%d\n",
             curdata, netcon_getPacketState(con, curdata->sendSequence)
             , curdata->recordID);

            if(curdata->recordID > (relRecord->windowStartRecordID + RELSTREAMWINDSIZE))
            {
                break;
            }
            if(netcon_getPacketState(con, curdata->sendSequence) == NETCON_PACKET_DROPPED)
            {
                stream_writeBit(bs, 1);
                stream_writeVarLong(bs, curdata->recordID);
                stream_writeVarLong(bs, curdata->bitLen);
                stream_writeBitsData(bs, curdata->data, curdata->bitLen);
                curdata->sendSequence = con->outgoingSequence;
                sent = 1;
                printf("writeRelStream, resending lost packet, record=%d, sequence=%d\n", curdata->recordID, curdata->sendSequence);
                break;
            }
        }
    }

    if(!sent)
    {
        // printf("writing zero bit\n");
        stream_writeBit(bs, 0);
    }

}

void streamReliable_readPacket(relStreamRecord_t *relRecord, bitstream_t *bs)
{
    relStreamNode_t *curData, *newData, *prevData, *lastData, *nextData;
    byte *buf;
    int recordID, bitLen, byteLen;
    bitstream_t readbs;

    if(!stream_readBit(bs))
        return;
    
    recordID = stream_readVarLong(bs);
    bitLen = stream_readVarLong(bs);

    printf("readRelStream, relRecord->lastRecordID=%d, recordID=%d\n", relRecord->lastRecordID, recordID);

    if((relRecord->lastRecordID + 1) > recordID)
    {
        com_printf("Error: received reliable recordID less than the latest recordID\n");
        return;
    }

    if((relRecord->lastRecordID + 1) == recordID)
    {
        relRecord->readFunc(bs);
        relRecord->lastRecordID++;
        
        lastData = NULL;
        for(curData = relRecord->head; curData != NULL; curData = curData->next)
        {
            printf("readRelStream, currecord=%d\n", curData->recordID);
            if((relRecord->lastRecordID + 1) == curData->recordID)
            {
                byteLen = curData->bitLen/8;
                if(curData->bitLen%8)
                    byteLen++;
                stream_init(&readbs, curData->data, byteLen);
                relRecord->readFunc(&readbs);
                relRecord->lastRecordID++;
                lastData = curData;
            }
            else {
                break;
            }
        }

        if(!lastData)
            return;

        curData = relRecord->head;
        while(curData)
        {
            relRecord->recordCount--;

            nextData = curData->next;
            zidfree(curData->data);
            zidfree(curData);
            if(curData == lastData)
            {
                relRecord->head = nextData;
                break;
            }

            curData = nextData;
        }

        if(lastData == relRecord->tail)
        {
            relRecord->head = relRecord->tail = NULL;
            relRecord->recordCount = 0;
            return;
        }
    }
    else
    {
        int byteLen = bitLen/8;
        int remLen = bitLen%8;
        int ceilLen = byteLen;
        if(remLen)
            ceilLen += 1;
        
        buf = (byte *) zidmalloc(TEMPORARYZONE, ceilLen);
        stream_copyBitsData(bs, buf, bitLen);

        newData = (relStreamNode_t *) zidmalloc(TEMPORARYZONE, sizeof(relStreamNode_t));

        newData->next = NULL;
        newData->data = buf;
        newData->recordID = recordID;
        newData->bitLen = bitLen;

        if(relRecord->head)
        {
            prevData = NULL;
            for(curData = relRecord->head; curData != NULL; curData = curData->next)
            {
                if(recordID < curData->recordID)
                    break;
                prevData = curData;
            }

            if(!prevData)
            {
                newData->next = relRecord->head;
                relRecord->head = newData;
            }
            else if(!curData)
            {
                relRecord->tail->next = newData;
                relRecord->tail = newData;
            }
            else {
                prevData->next = newData;
                newData->next = curData;
            }      
        }
        else {
            relRecord->head = newData;
            relRecord->tail = newData;
        }
        relRecord->recordCount++;
    }
}

void streamReliable_acknowledge(relStreamRecord_t *relRecord, netcon_t *con)
{
    relStreamNode_t *curData, *nextData, *prevData;
    int i;

    if(!relRecord->head)
        return;

    printf("checkRelStreamReceived\n");

    curData = relRecord->head;
    prevData = NULL;
    while(curData)
    {
        if(!curData->isSent || netcon_getPacketState(con, curData->sendSequence) != NETCON_PACKET_SUCCESS)
        {
            prevData = curData;
            curData = curData->next;
            continue;
        }

        printf("freeing record=%d\n", curData->recordID);
        relRecord->recordCount--;

        nextData = curData->next;

        zidfree(curData->data);
        zidfree(curData);

        if(relRecord->head == curData) {
            relRecord->head = nextData;
        }
        if(relRecord->tail == curData) {
            relRecord->tail = prevData;
        }

        curData = nextData;
        if(prevData) {
            prevData->next = nextData;
        }
    }

    if(relRecord->head) {
        relRecord->windowStartRecordID = relRecord->head->recordID - 1;
    }
    else {
        relRecord->windowStartRecordID = relRecord->lastRecordID;
    }
}


/********************MOST RECENT STREAM********************/


void streamRecent_init(recentStreamRecord_t *recentRecord, int stateSize, int (*readFunc)(bitstream_t *, byte *), int (*writeFunc)(bitstream_t *, byte *))
{
    recentRecord->head = recentRecord->tail = NULL;
    recentRecord->stateBitLen = stateSize;
    recentRecord->stateByteLen = (int)CEIL(((float)stateSize)/8.0);
    recentRecord->readFunc = readFunc;
    recentRecord->writeFunc = writeFunc;

    recentRecord->stateChangeBm = (byte *) zidmalloc(GENERALZONE, stateSize);
    zmemset(recentRecord->stateChangeBm, 0, stateSize);
}

int streamRecent_writeStateBits(recentStreamRecord_t *recentRecord, bitstream_t *bs, netcon_t *con, byte *stateBm)
{
    recentStreamNode_t *curData, *newData;
    byte setFlag = 0;
    
    newData = NULL;
    memset(stateBm, 0, sizeof(stateBm));

    for(int i = 0; i < recentRecord->stateBitLen; i++)
    {
        if(bm_getBitVal(recentRecord->stateChangeBm, i)) {
            bm_setBitVal(stateBm, i, 1);
            if(!setFlag) setFlag = 1;
        }
    }

    if(setFlag)
    {
        newData = (recentStreamNode_t *) zidmalloc(TEMPORARYZONE, sizeof(recentStreamNode_t));
        newData->next = NULL;
        newData->sendSequence = con->outgoingSequence;
        newData->stateBm = (byte *)zidmalloc(TEMPORARYZONE, recentRecord->stateByteLen);
        zmemcpy(newData->stateBm, stateBm, recentRecord->stateByteLen);
    }

    for(curData = recentRecord->head; curData != NULL; curData = curData->next)
    {
        printf("curdata:%p, seq=%d, status=%d, bit:", curData, curData->sendSequence
        ,netcon_getPacketState(con, curData->sendSequence)
        ); printbit(curData->stateBm[0]); printf("\n");
        if(netcon_getPacketState(con, curData->sendSequence) == NETCON_PACKET_DROPPED)
        {
            for(int i = 0; i < recentRecord->stateBitLen; i++)  {
                if(bm_getBitVal(curData->stateBm, i)) {
                    if(!setFlag) setFlag = 1;
                    bm_setBitVal(stateBm, i, 1);
                }
            }
            curData->sendSequence = con->outgoingSequence;
        }
    }

    if(newData) {
        if(!recentRecord->head)
        {
            recentRecord->head = newData;
            recentRecord->tail = newData;
            newData->prev = NULL;
        }
        else {
            recentRecord->tail->next = newData;
            newData->prev = recentRecord->tail;
            recentRecord->tail = newData;
        }
    }

    for(int i = 0; i < recentRecord->stateBitLen; i++)
    {
        stream_writeBit(bs, bm_getBitVal(stateBm, i));
    }

    zmemset(recentRecord->stateChangeBm, 0, recentRecord->stateByteLen);

    return setFlag;
}

void streamRecent_writePacket(recentStreamRecord_t *recentRecord, bitstream_t *bs, netcon_t *con)
{
    byte stateBm[8];
    recentStreamNode_t *curData, *newData;
    byte setFlag = 0;
    
    newData = NULL;
    memset(stateBm, 0, sizeof(stateBm));

    for(int i = 0; i < recentRecord->stateBitLen; i++)
    {
        if(bm_getBitVal(recentRecord->stateChangeBm, i)) {
            bm_setBitVal(stateBm, i, 1);
            if(!setFlag) setFlag = 1;
        }
    }

    if(setFlag)
    {
        newData = (recentStreamNode_t *) zidmalloc(TEMPORARYZONE, sizeof(recentStreamNode_t));
        newData->next = NULL;
        newData->sendSequence = con->outgoingSequence;
        newData->stateBm = (byte *)zidmalloc(TEMPORARYZONE, recentRecord->stateByteLen);
        zmemcpy(newData->stateBm, stateBm, recentRecord->stateByteLen);
    }

    for(curData = recentRecord->head; curData != NULL; curData = curData->next)
    {
        printf("curdata:%p, seq=%d, status=%d, bit:", curData, curData->sendSequence
        ,netcon_getPacketState(con, curData->sendSequence)
        ); printbit(curData->stateBm[0]); printf("\n");
        if(netcon_getPacketState(con, curData->sendSequence) == NETCON_PACKET_DROPPED)
        {
            for(int i = 0; i < recentRecord->stateBitLen; i++)  {
                if(bm_getBitVal(curData->stateBm, i)) {
                    if(!setFlag) setFlag = 1;
                    bm_setBitVal(stateBm, i, 1);
                }
            }
            curData->sendSequence = con->outgoingSequence;
        }
    }

    if(newData) {
        if(!recentRecord->head)
        {
            recentRecord->head = newData;
            recentRecord->tail = newData;
            newData->prev = NULL;
        }
        else {
            recentRecord->tail->next = newData;
            newData->prev = recentRecord->tail;
            recentRecord->tail = newData;
        }
    }

    for(int i = 0; i < recentRecord->stateBitLen; i++)
    {
        stream_writeBit(bs, bm_getBitVal(stateBm, i));
    }

    if(setFlag) recentRecord->writeFunc(bs, stateBm);

    zmemset(recentRecord->stateChangeBm, 0, recentRecord->stateByteLen);

}

void streamRecent_readStateBits(int stateLen, bitstream_t *bs, byte *stateBm)
{
    byte bit;

    zmemset(stateBm, 0, stateLen);
    for(int i = 0; i < stateLen; i++)
    {
        if((bit = stream_readBit(bs)) > 0)
        {
            printf("found bit %d\n", i);
            bm_setBitVal(stateBm, i, bit);
        }
    }
}


// void streamRecent_readStateBits(recentStreamRecord_t *recentRecord, bitstream_t *bs, byte *stateBm)
// {
//     byte bit;

//     zmemset(stateBm, 0, recentRecord->stateBitLen);
//     for(int i = 0; i < recentRecord->stateBitLen; i++)
//     {
//         if((bit = stream_readBit(bs)) > 0)
//         {
//             printf("found bit %d\n", i);
//             bm_setBitVal(stateBm, i, bit);
//         }
//     }
// }

void streamRecent_readPacket(recentStreamRecord_t *recentRecord, bitstream_t *bs)
{
    byte stateBm[8];
    byte bit;

    zmemset(stateBm, 0, recentRecord->stateBitLen);
    for(int i = 0; i < recentRecord->stateBitLen; i++)
    {
        if((bit = stream_readBit(bs)) > 0)
        {
            printf("found bit %d\n", i);
            bm_setBitVal(stateBm, i, bit);
        }
    }

    recentRecord->readFunc(bs, stateBm);
}

qbool isStateEmpty(recentStreamNode_t *node, int len)
{

    for(int i = 0; i < len; i++) {
        if(node->stateBm[i] != 0)
            return qfalse;
    }
    return qtrue;
}

void streamRecent_acknowledge(recentStreamRecord_t *recentRecord, netcon_t *con)
{
    recentStreamNode_t *prevData, *curData, *nextData;
    byte stateBm[8];

    if(!recentRecord->head)
        return;
    
    zmemset(stateBm, 0, recentRecord->stateByteLen);
    for(prevData = recentRecord->tail; prevData != NULL; prevData = prevData->prev)
    {
        if(netcon_getPacketState(con, prevData->sendSequence) == NETCON_PACKET_SUCCESS)
        {
            for(int i = 0; i < recentRecord->stateByteLen; i++)
            {
                stateBm[i] |= prevData->stateBm[i];
                // if(bm_getBitVal(prevData->stateBm, i)) {
                //     bm_setBitVal(stateBm, i, 1);
                // }
            }
        }
        else {
            for(int i = 0; i < recentRecord->stateBitLen; i++)
            {
                if(bm_getBitVal(stateBm, i) && bm_getBitVal(prevData->stateBm, i))
                {
                    printf("resetting old bit i=%d\n", i);
                    bm_setBitVal(prevData->stateBm, i, 0);
                }
            }
        }
    }

    curData = recentRecord->head;
    prevData = NULL;
    while(curData)
    {
        if((netcon_getPacketState(con, curData->sendSequence) != NETCON_PACKET_SUCCESS)
            && (!isStateEmpty(curData, recentRecord->stateByteLen)))
        {
            prevData = curData;
            curData = curData->next;
            continue;
        }

        // printf("freeing curdata %p\n", curData);
        nextData = curData->next;
        zidfree(curData->stateBm);
        zidfree(curData);

        if(recentRecord->head == curData) {
            recentRecord->head = nextData;
        }
        if(recentRecord->tail == curData) {
            recentRecord->tail = prevData;
        }
        
        curData = nextData;
        if(prevData) {
            prevData->next = curData;
        }
        if(curData) {
            curData->prev = prevData;
        }
    }
}

void streamRecent_setState(recentStreamRecord_t *recentRecord, int i, byte flag)
{
    bm_setBitVal(recentRecord->stateChangeBm, i, flag);
}
