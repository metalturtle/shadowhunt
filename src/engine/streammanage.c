#include "../basic/basic.h"
#include "engine.h"

server_t server;
client_t client;

byte writeBuf[MAX_MSGLEN];

/********************QUICKEST STREAM********************/


/*
===============
streamQuick_init

Sets the quickRecord struct to point to empty data, and sets the read and write bitstream functions. 
===============
*/
void streamQuick_init(
    quickStreamRecord_t *quickRecord,
    int (*readFunc)(bitstream_t *, void *),
    int (*writeFunc)(bitstream_t *, void *)
)
{
    quickRecord->head = quickRecord->tail = NULL;
    quickRecord->recordCount = 0;
    quickRecord->readFunc = readFunc;
    quickRecord->writeFunc = writeFunc;
    quickRecord->data = NULL;
}


/*
===============
streamQuick_getRecordCount

Gets the number of records. 
===============
*/
int streamQuick_getRecordCount(quickStreamRecord_t *quickRecord)
{
    return quickRecord->recordCount;
}


/*
===============
streamQuick_begin

Initializes the bitstream. 
===============
*/
void streamQuick_begin(quickStreamRecord_t *quickRecord, netcon_t *con, bitstream_t *bs)
{
    // byte *buf;

    // buf = (byte *) zidmalloc(TEMPORARYZONE, MAX_SEGMENTLEN);
    stream_init(bs, writeBuf, MAX_SEGMENTLEN);
}


/*
===============
streamQuick_end

Saves a quickstream record and adds it to the list in quickRecord struct. 
===============
*/
void streamQuick_end(quickStreamRecord_t *quickRecord, netcon_t *con, bitstream_t *bs)
{
    quickStreamNode_t *newData;
    float bitLen;
    int byteLen;


    // Allocate a quickStreamNode in the temporary memory 
    newData = (quickStreamNode_t *) zidmalloc(TEMPORARYZONE, sizeof(quickStreamNode_t));


    //Add the node to the quickRecord list. Only head is checked for NULL condition as tail will be only NULL if head is NULL.
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
    quickRecord->recordCount++;


    //Set the node sequence to the current outgoing sequence.
    newData->next = NULL;
    newData->sendSequence = con->outgoingSequence;


    // Make a duplicate of the bitstream, allocate data in temporary zone, and save it in the node
    memcpy(&quickRecord->bs, bs, sizeof(bitstream_t));

    bitLen = quickRecord->bs.curbyte*8 + quickRecord->bs.curbit;
    byteLen = quickRecord->bs.curbyte + 1;

    newData->data = (byte *) zidmalloc(TEMPORARYZONE, byteLen);
    newData->bitLen = (int) bitLen;

    zmemcpy(newData->data, quickRecord->bs.buf, byteLen);
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


/*
===============
streamQuick_close

Releases the quick stream records and buffers.
===============
*/
void streamQuick_close(quickStreamRecord_t *quickRecord)
{
    quickStreamNode_t *curNode, *nextNode;
    for(curNode = quickRecord->head; curNode != NULL;)
    {
        zidfree(curNode->data);
        nextNode = curNode->next;
        zidfree(curNode);
        curNode = nextNode;
    }

    quickRecord->head = quickRecord->tail = NULL;
    quickRecord->recordCount = 0;
}


/*
===============
streamQuick_writePacket

Writes all the quickNode records into a bitstream.
===============
*/
void streamQuick_writePacket(quickStreamRecord_t *quickRecord, bitstream_t *bs, netcon_t *con)
{
    quickStreamNode_t *curdata;


    // Write a bit indicating if there are records to send
    if(quickRecord->recordCount == 0)
    {
        printf("returned bit zero record count \n");
        stream_writeBit(bs, 0);
        return;
    }
    else {
        stream_writeBit(bs, 1);
    }


    // Write the number of records that are sent
    stream_writeByte(bs, quickRecord->recordCount);


    // write the data from all the records into the stream
    for(curdata = quickRecord->head; curdata != NULL; curdata = curdata->next)
    {
        stream_writeBitsData(bs, curdata->data, curdata->bitLen);
    }
}


/*
===============
streamQuick_addPayload

Sets the bitstream data directly into a record
===============
*/
void streamQuick_addPayload(quickStreamRecord_t *quickRecord, void *data)
{
    quickRecord->data = data;
}


/*
===============
streamQuick_readPacket

Sets the bitstream data directly into a record
===============
*/

int streamQuick_readCount(quickStreamRecord_t *quickRecord, bitstream_t *bs)
{
    if(!stream_readBit(bs))
    {
        return 0;
    }


    // Read the number of records that are sent
    return stream_readByte(bs);
}


void streamQuick_readPacket(quickStreamRecord_t *quickRecord, bitstream_t *bs)
{
    // byte isSent;
    int recordCount;
    // quickStreamNode_t newData;
    // quickStreamNode_t *curdata;


    // Check if there is data to read
    if(!stream_readBit(bs))
    {
        return;
    }


    // Read the number of records that are sent
    recordCount = stream_readByte(bs);


    // Call the read bitstream function for every record
    for(int i = 0; i < recordCount; i++)
    {
        quickRecord->readFunc(bs, quickRecord->data);
    }
    

    quickRecord->data = NULL;
}


/*
===============
streamQuick_acknowledge

Sets the bitstream data directly into a record
===============
*/
void streamQuick_acknowledge(quickStreamRecord_t *quickRecord, netcon_t *con)
{
    quickStreamNode_t *curData, *nextData;
    quickStreamNode_t *lastSent = NULL;
    // int lastseq = 0;


    // If no records, then return
    if(!quickRecord->head)
        return;


    //Go through all the records, and see which is the last sequence successfully acknowledged
    for(curData = quickRecord->head; curData != NULL; curData = curData->next)
    {
        if(netcon_getPacketState(con, curData->sendSequence) == NETCON_PACKET_SUCCESS)
        {
            lastSent = curData;
            // lastseq = curData->sendSequence;
        }
    }

    // printf("recv reqd data: lastrecvseq: %d \n", lastseq);



    //if there are no sequences acknowledged, then return
    if(lastSent == NULL)
    {
        return;
    }


    // Free up the records that are sent successfully
    curData = quickRecord->head;
    while(curData)
    {
        quickRecord->recordCount--;
        
        // printf("freeing curdata %p\n", curData);

        // saving the next record, and freeing the current record
        nextData = curData->next;
        zidfree(curData->data);
        zidfree(curData);

        // If the current record is the last successful record, then set the head to next record 
        if(curData == lastSent)
        {
            quickRecord->head = nextData;
            break;
        }


        //set the current record to next record
        curData = nextData;
    }



    // If all records are freed, then reset the record list to empty
    if(lastSent == quickRecord->tail)
    {
        quickRecord->head = quickRecord->tail = NULL;
        quickRecord->recordCount = 0;
        return;
    }
}


/********************REQUIRED STREAM********************/


/*
===============
streamReliable_init

Initializes ReliableStreamRecord, setting the records to zero and adding read and write serializing functions
===============
*/
void streamReliable_init(relStreamRecord_t *relRecord, int (*readFunc)(bitstream_t *), int (*writeFunc)(bitstream_t *))
{
    relRecord->windowStartRecordID = 0;
    relRecord->recordCount = 0;
    relRecord->readFunc = readFunc;
    relRecord->writeFunc = writeFunc;
    relRecord->lastRecordID = 0;
}


/*
===============
streamReliable_callWriteFunc


===============
*/
int streamReliable_callWriteFunc(relStreamRecord_t *relRecord, netcon_t *con)
{
    relStreamNode_t *newData;
    byte *buf;
    float bitLen;
    int byteLen;
    bitstream_t bs;


    //Allocates a temporary buffer for bitstream
    buf = (byte *) zidmalloc(TEMPORARYZONE, MAX_SEGMENTLEN);
    stream_init(&bs, buf, MAX_SEGMENTLEN);


    //Call the write function for the bitstream buffer
    int ret = relRecord->writeFunc(&bs);


    //If write failed, then free the buffer and retrn
    if(!ret)
    {
        zidfree(buf);
        return 0;
    }


    //allocate a reliable stream node in temporary zone
    newData = (relStreamNode_t *) zidmalloc(TEMPORARYZONE, sizeof(relStreamNode_t));

    
    //Add the new node into the end of the reliable record linked list
    if(relRecord->head == NULL)
    {
        relRecord->head = newData;
        relRecord->tail = newData;
    }
    else
    {
        relRecord->tail->next = newData;
        relRecord->tail = newData;
    }
    relRecord->recordCount++;


    //Init the new node, set a new recordID
    newData->next = NULL;
    newData->recordID = ++relRecord->lastRecordID;
    newData->isSent = 0;


    // Allocate a new buffer with size number of bytes written in the new node, and copy the data
    bitLen = bs.curbyte*8 + bs.curbit;
    byteLen = (int) CEIL(bitLen/8.0);

    newData->data = (byte *) zidmalloc(TEMPORARYZONE, byteLen);
    newData->bitLen = (int) bitLen;

    zmemcpy(newData->data, bs.buf, byteLen);


    //Free the bitstream buffer
    zidfree(buf);
    return 1;
}


/*
===============
streamReliable_writePacket

Checks for unsent or dropped reliable records, and writes the record data into the packet bitstream 
===============
*/
void streamReliable_writePacket(relStreamRecord_t *relRecord ,bitstream_t *packetBitStream, netcon_t *con)
{
    relStreamNode_t *curdata;
    int wind = 0;
    byte sent = 0;


    // Write a bit to indicate that there are records sent
    if(!relRecord->head)
    {
        stream_writeBit(packetBitStream, 0);
        return;
    }


    // The window size is the number of records that are unsent.
    wind = (relRecord->head->recordID - 1) - relRecord->windowStartRecordID;
    printf("wind=%d, firstrec=%d, windowrec=%d\n", wind, relRecord->head->recordID, relRecord->windowStartRecordID);


    //Go through all the records, find any unsent packet, and write it to the packet bitstream
    for(curdata = relRecord->head; (curdata != NULL); curdata = curdata->next)
    {
        // printf("writeRelStream, curdata=%p, status=%d, record=%d, issent=%d\n", curdata,
        //  netcon_getPacketState(con, curdata->sendSequence)
        //  , curdata->recordID
        //  , curdata->isSent);


        // If the window size is exceeded, then break
        if(curdata->recordID > (relRecord->windowStartRecordID + RELSTREAMWINDSIZE))
        {
            break;
        }


        // If an unsent record is found
        if(!curdata->isSent)
        {

            // Write a bit to signify that there is a record to read, the record ID, the bit size, and the record data
            stream_writeBit(packetBitStream, 1);
            stream_writeVarLong(packetBitStream, curdata->recordID);
            stream_writeVarLong(packetBitStream, curdata->bitLen);
            stream_writeBitsData(packetBitStream, curdata->data, curdata->bitLen);

            // Set the isSent flag to 1, and save the current outgoing sequence
            curdata->isSent = 1;
            curdata->sendSequence = con->outgoingSequence;

            sent = 1;
            printf("writeRelStream, sending new packet, record=%d, sequence=%d\n", curdata->recordID, curdata->sendSequence);
            break;
        }
    }


    // If there are no unsent records, then look for sent records which were dropped/not recieved.
    if(!sent)
    {
        wind = (relRecord->head->recordID - 1) - relRecord->windowStartRecordID;
        printf("resending\n");
        for(curdata = relRecord->head; (curdata != NULL); curdata = curdata->next)
        {
            // printf("writeRelStream, curdata=%p, status=%d, record=%d\n",
            //  curdata, netcon_getPacketState(con, curdata->sendSequence)
            //  , curdata->recordID);


            // If window size is exceeded, then break
            if(curdata->recordID > (relRecord->windowStartRecordID + RELSTREAMWINDSIZE))
            {
                break;
            }


            // Check if the record was sent, but the packet was dropped
            if(curdata->isSent && (netcon_getPacketState(con, curdata->sendSequence) == NETCON_PACKET_DROPPED))
            {
                //Write the bit signifying a record to read, the recordID, number of bits to read and the data
                stream_writeBit(packetBitStream, 1);
                stream_writeVarLong(packetBitStream, curdata->recordID);
                stream_writeVarLong(packetBitStream, curdata->bitLen);
                stream_writeBitsData(packetBitStream, curdata->data, curdata->bitLen);

                //Save the new outgoing sequence
                curdata->sendSequence = con->outgoingSequence;
                sent = 1;
                printf("writeRelStream, resending lost packet, record=%d, sequence=%d\n", curdata->recordID, curdata->sendSequence);
                break;
            }
        }
    }


    // If there were no unsent or dropped records, then write a zero bit signifying no record to read
    if(!sent)
    {
        stream_writeBit(packetBitStream, 0);
    }

}


/*
===============
streamReliable_readPacket

This function will check if the record in the packet stream has a recordID that is incremented
from the previous record by one. If not, then save the record in a list, and read it later when 
all the previous records are recieved and the record ids are in serial order. If the recieved record
is incremented from the previous processed record by one, then read the data, and iterate and process records
from the list if they have serial order. The relStraemRecord_t is used differently in for reading packet.
The lastRecordID is the starting of the window, which indicates the last record processed.
===============
*/
void streamReliable_readPacket(relStreamRecord_t *relRecord, bitstream_t *bs)
{
    relStreamNode_t *curData, *newData, *prevData, *lastData, *nextData;
    byte *buf;
    int recordID, bitLen, byteLen;
    bitstream_t readbs;


    // Check if there is record to read, if not then return
    if(!stream_readBit(bs))
        return;
    

    // Get the recordID and the bit length
    recordID = stream_readVarLong(bs);
    bitLen = stream_readVarLong(bs);

    printf("readRelStream, relRecord->lastRecordID=%d, recordID=%d\n", relRecord->lastRecordID, recordID);


    // Check if the recieved recordID is already recieved and processed earlier
    if((relRecord->lastRecordID + 1) > recordID)
    {
        com_printf("Error: received reliable recordID less than the latest recordID\n");
        return;
    }


    // Check if there was no record skipped, and the recieved record is in correct order
    if((relRecord->lastRecordID + 1) == recordID)
    {
        // Call the read stream function
        relRecord->readFunc(bs);


        // Increment the lastRecordID, which means that all the records till
        //the recieved record have come in order
        relRecord->lastRecordID++;


        // See that the records which were out of order earlier are in
        // correct order now, and read their data.
        lastData = NULL;
        for(curData = relRecord->head; curData != NULL; curData = curData->next)
        {
            printf("readRelStream, currecord=%d\n", curData->recordID);

            // If the previously out of order record is now in order
            if((relRecord->lastRecordID + 1) == curData->recordID)
            {
                // Calculate the number of bytes the record data has.
                byteLen = curData->bitLen/8;
                if(curData->bitLen%8)
                    byteLen++;

                //Init a bitstream and read the data from the record
                stream_init(&readbs, curData->data, byteLen);
                relRecord->readFunc(&readbs);


                //Increment the lastRecordID
                relRecord->lastRecordID++;
                lastData = curData;
            }
            // Else there are still records that are not recieved, and
            // the records are not serially in order
            else {
                break;
            }
        }


        // If there are no ordered records in the list, then return
        if(!lastData)
            return;


        // Remove the records from the list which have already been read
        curData = relRecord->head;
        while(curData)
        {

            // Decrement the record count
            relRecord->recordCount--;

            // Free up the record
            nextData = curData->next;
            zidfree(curData->data);
            zidfree(curData);

            //If this is the last record read, then point the head to the next record
            if(curData == lastData)
            {
                relRecord->head = nextData;
                break;
            }

            curData = nextData;
        }


        // If all the records have been processed, then set the head and tail to NULL,
        // and the record to zero
        if(lastData == relRecord->tail)
        {
            relRecord->head = relRecord->tail = NULL;
            relRecord->recordCount = 0;
        }
    }
    // If a record was skipped, and the recieved record is out of order
    else
    {

        // Calculate the number of bytes the record takes
        int byteLen = bitLen/8;
        int remLen = bitLen%8;
        int ceilLen = byteLen;
        if(remLen)
            ceilLen += 1;
        

        // Allocate a temporary buffer with the size of the recieved record data, and copy the record data
        // into this buffer
        buf = (byte *) zidmalloc(TEMPORARYZONE, ceilLen);
        stream_copyBitsData(bs, buf, bitLen);


        // Allocate a reliable stream node, and set the temporary buffer, the recieved recordID
        // and the bit size to this new node.
        newData = (relStreamNode_t *) zidmalloc(TEMPORARYZONE, sizeof(relStreamNode_t));
        newData->next = NULL;
        newData->data = buf;
        newData->recordID = recordID;
        newData->bitLen = bitLen;


        // If there are other records that were out of order, then the new record is added
        // to the list in a sorted manner
        if(relRecord->head)
        {

            // Find a place to insert the new record in the list
            prevData = NULL;
            for(curData = relRecord->head; curData != NULL; curData = curData->next)
            {
                if(recordID < curData->recordID)
                    break;
                prevData = curData;
            }

            // If the new record is older than the other records in the list, then
            // place it at the head of the list
            if(!prevData)
            {
                newData->next = relRecord->head;
                relRecord->head = newData;
            }
            // If the new record is the latest record, then place it at the end of the list
            else if(!curData)
            {
                relRecord->tail->next = newData;
                relRecord->tail = newData;
            }
            // If the new record is in between other records
            else {
                prevData->next = newData;
                newData->next = curData;
            }      
        }
        // If no other out of order records, then save the recieved record
        // as the first record in the list
        else {
            relRecord->head = newData;
            relRecord->tail = newData;
        }

        // Increment the record count
        relRecord->recordCount++;
    }
}


/*
===============
streamReliable_acknowledge

Checks if the records are successfully sent and acknowledged by the receiver, and removes them
from the record list 
===============
*/
void streamReliable_acknowledge(relStreamRecord_t *relRecord, netcon_t *con)
{
    relStreamNode_t *curData, *nextData, *prevData;
    // int i;


    // If the record list is empty, then there are no records to acknowledge, and return
    if(!relRecord->head)
        return;

    printf("checkRelStreamReceived\n");


    // Go through the record list, and remove records that are acknowledged
    curData = relRecord->head;
    prevData = NULL;
    while(curData)
    {

        // If the current record is not sent or is not acknowledged, then skip it
        if(!curData->isSent || netcon_getPacketState(con, curData->sendSequence) != NETCON_PACKET_SUCCESS)
        {
            // Save the previous node
            prevData = curData;
            curData = curData->next;
            continue;
        }


        //Reduce the record count
        printf("freeing record=%d\n", curData->recordID);
        relRecord->recordCount--;


        // Save the next record
        nextData = curData->next;


        // Free the current record and its data
        zidfree(curData->data);
        zidfree(curData);


        // If the current record is the head of the list, then set
        // the head to the next record
        if(relRecord->head == curData) {
            relRecord->head = nextData;
        }

        //If the current record is the tail of the list, then set the 
        // tail to the previous node
        if(relRecord->tail == curData) {
            relRecord->tail = prevData;
        }

        // Set current record to the next record for the next iteration
        curData = nextData;

        //If previous data exists, then set the prev record next to next record
        if(prevData) {
            prevData->next = nextData;
        }
    }


    // If there are records in the list, then set the window start to the last record processed
    if(relRecord->head) {
        relRecord->windowStartRecordID = relRecord->head->recordID - 1;
    }
    // else all records are processed in the list and set the window start
    // to the last record created
    else {
        relRecord->windowStartRecordID = relRecord->lastRecordID;
    }
}


/********************MOST RECENT STREAM********************/


/*
===============
streamRecent_init

Checks if the records are successfully sent and acknowledged by the receiver, and removes them
from the record list 
===============
*/
void streamRecent_init(
    recentStreamRecord_t *recentRecord,
    int stateSize,
    int (*readFunc)(bitstream_t *, byte *),
    int (*writeFunc)(bitstream_t *, byte *),
    byte *stateChangeBm
    )
{
    recentRecord->head = recentRecord->tail = NULL;
    recentRecord->stateBitLen = stateSize;
    recentRecord->stateByteLen = (int)CEIL(((float)stateSize)/8.0);
    recentRecord->readFunc = readFunc;
    recentRecord->writeFunc = writeFunc;

    recentRecord->stateChangeBm = stateChangeBm;
}


// void streamRecent_getUnsentState(recentStreamRecord_t *recentRecord, netcon_t *con, byte *stateBm) {
//     memset(stateBm, 0, recentRecord->stateByteLen);

// }
/*
===============
streamRecent_writeStateBits


Check if there is a new state, or if there are states that were dropped, and write the state bits
in the packet bitstream
===============
*/
int streamRecent_writeStateBits(recentStreamRecord_t *recentRecord, bitstream_t *bs, netcon_t *con, byte *stateBm)
{
    recentStreamNode_t *curData, *newData = NULL;
    byte setFlag = 0;
    

    //Set the byte data in stateBm to empty
    memset(stateBm, 0, recentRecord->stateByteLen);


    //Get the state bits that are changed, set the bits in stateSent bitmap
    for(int i = 0; i < recentRecord->stateBitLen; i++)
    {
        // check if state is changed
        if(bm_getBitVal(recentRecord->stateChangeBm, i))
        {
            // Set the same bit in stateSent bitmap
            bm_setBitVal(stateBm, i, 1);

            // Track that a state has been changed
            if(!setFlag) setFlag = 1;
        }
    }


    // If any state has been changed, then create a new record
    if(setFlag)
    {
        // Allocate the new record
        newData = (recentStreamNode_t *) zidmalloc(TEMPORARYZONE, sizeof(recentStreamNode_t));
        newData->next = NULL;

        // Mark the record with the current outgoing sequence
        newData->sendSequence = con->outgoingSequence;

        // Allocate the byte array for the state bitmap
        newData->stateBm = (byte *)zidmalloc(TEMPORARYZONE, recentRecord->stateByteLen);
        zmemcpy(newData->stateBm, stateBm, recentRecord->stateByteLen);
    }


    // Go through the record list and find the states that have not been recieved
    for(curData = recentRecord->head; curData != NULL; curData = curData->next)
    {
        // printf("curdata:%p, seq=%d, status=%d, bit:", curData, curData->sendSequence
        // ,netcon_getPacketState(con, curData->sendSequence)
        // ); printbit(curData->stateBm[0]); printf("\n");

        // If the packet carrying the record has been dropped
        if(netcon_getPacketState(con, curData->sendSequence) == NETCON_PACKET_DROPPED)
        {

            // Save the state from the lost record to the stateSent param
            for(int i = 0; i < recentRecord->stateBitLen; i++)  {
                if(bm_getBitVal(curData->stateBm, i)) {
                    if(!setFlag) setFlag = 1;
                    bm_setBitVal(stateBm, i, 1);
                }
            }

            
            //Mark the lost record with the current outgoing sequence
            curData->sendSequence = con->outgoingSequence;
        }
    }


    // If a new record is created, which means new state changes, save the
    // record in the record list
    if(newData)
    {
        // Add the record in the linked list 
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


    // Write the bits saved in the sentState param to the packet bitstream
    // for(int i = 0; i < recentRecord->stateBitLen; i++)
    // {
    //     stream_writeBit(bs, bm_getBitVal(stateBm, i));
    // }


    // Reset the state bits in the record
    zmemset(recentRecord->stateChangeBm, 0, recentRecord->stateByteLen);

    
    // Return value indicating whether state is sent or not
    return setFlag;
}


/*
===============
streamRecent_writePacket


Check if there is a new state, or if there are states that were dropped, and write the state bits
in the packet bitstream
===============
*/
void streamRecent_writePacket(recentStreamRecord_t *recentRecord, bitstream_t *bs, netcon_t *con)
{
    byte stateBm[8];
    recentStreamNode_t *curData, *newData = NULL;
    byte setFlag = 0;
    

    memset(stateBm, 0, recentRecord->stateByteLen);

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
        // printf("curdata:%p, seq=%d, status=%d, bit:", curData, curData->sendSequence
        // ,netcon_getPacketState(con, curData->sendSequence)
        // ); printbit(curData->stateBm[0]); printf("\n");
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


/*
===============
streamRecent_readStateBits


Check if there is a new state, or if there are states that were dropped, and write the state bits
in the packet bitstream
===============
*/
void streamRecent_readStateBits(int stateLen, bitstream_t *bs, byte *readStateBm)
{
    byte bit;


    // Sets the readState bitmap to zero
    zmemset(readStateBm, 0, stateLen);


    // The number if bits sent is the number of states.
    // Copy the states sent in the bitstream into the readState bitmap.
    for(int i = 0; i < stateLen; i++)
    {
        if((bit = stream_readBit(bs)) > 0)
        {
            bm_setBitVal(readStateBm, i, bit);
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
            bm_setBitVal(stateBm, i, bit);
        }
    }

    recentRecord->readFunc(bs, stateBm);
}


/*
===============
isStateEmpty


Check if there is a new state, or if there are states that were dropped, and write the state bits
in the packet bitstream
===============
*/
qbool isStateEmpty(recentStreamNode_t *node, int len)
{

    for(int i = 0; i < len; i++) {
        if(node->stateBm[i] != 0)
            return qfalse;
    }
    return qtrue;
}


/*
===============
streamRecent_acknowledge


Find out which records are recieved and acknowledged. Only the latest state matters, so if 
an earlier state A1 is not recieved, but another state change A2 is recieved later, then discard the 
older state change A1.
===============
*/
void streamRecent_acknowledge(recentStreamRecord_t *recentRecord, netcon_t *con)
{
    recentStreamNode_t *prevData, *curData, *nextData;
    byte stateBm[8];


    // If all sent records are already acknowledged, then return
    if(!recentRecord->head)
        return;
    

    // Set the readState bitmap to empty
    zmemset(stateBm, 0, recentRecord->stateByteLen);


    // Go backwards in the record list to see if the the latest record is sent. For eg.
    // there are states A1, B1, C1 sent in packet P1, and in packet P2 changed state A2
    // is sent. If P2 is recieved, then discard tracked state change A1. B1 and C1
    // is still set, and will get sent if there are no state changes to B and C,
    // and P1 is not recieved.
    for(prevData = recentRecord->tail; prevData != NULL; prevData = prevData->prev)
    {
        // If record associated with the packet is acknowledged
        if(netcon_getPacketState(con, prevData->sendSequence) == NETCON_PACKET_SUCCESS)
        {

            // set the state bitmap with the record state bits
            for(int i = 0; i < recentRecord->stateByteLen; i++)
            {
                stateBm[i] |= prevData->stateBm[i];
                // if(bm_getBitVal(prevData->stateBm, i)) {
                //     bm_setBitVal(stateBm, i, 1);
                // }
            }
        }
        // else if the record is not sent, recieved or dropped
        else {

            // Check if the state is already recieved, and unmark it
            // Only the latest state values matter, the older states are discarded
            for(int i = 0; i < recentRecord->stateBitLen; i++)
            {
                if(bm_getBitVal(stateBm, i) && bm_getBitVal(prevData->stateBm, i))
                {
                    // printf("resetting old bit i=%d\n", i);
                    bm_setBitVal(prevData->stateBm, i, 0);
                }
            }
        }
    }


    // Records which are successfully recieved are removed from the list
    curData = recentRecord->head;
    prevData = NULL;
    while(curData)
    {

        // If record is not acknowledged yet, then skip it
        if((netcon_getPacketState(con, curData->sendSequence) != NETCON_PACKET_SUCCESS)
            && (!isStateEmpty(curData, recentRecord->stateByteLen)))
        {
            prevData = curData;
            curData = curData->next;
            continue;
        }


        // Free the current record
        nextData = curData->next;
        zidfree(curData->stateBm);
        zidfree(curData);

        // If this is the first record, then set the head to the next record
        if(recentRecord->head == curData) {
            recentRecord->head = nextData;
        }
        // If this is the tail, then set the record to the previous record
        if(recentRecord->tail == curData) {
            recentRecord->tail = prevData;
        }
        

        //If there is a previous node, then connect it to the next node
        if(prevData) {
            prevData->next = nextData;
        }
        
        // If there is a next node, then connect it to the previous node
        if(nextData) {
            nextData->prev = prevData;
        }


        // go to the next record
        curData = nextData;
    }
}



/*
===============
streamRecent_setState


Set the state flags in the record
===============
*/
void streamRecent_setState(recentStreamRecord_t *recentRecord, int i, byte flag)
{
    bm_setBitVal(recentRecord->stateChangeBm, i, flag);
}


/*
===============
streamRecent_setState


Set the state flags in the record
===============
*/
void streamRecent_setStateBits(recentStreamRecord_t *recentRecord, byte *stateBm)
{
    for(int i = 0; i < recentRecord->stateBitLen; i++)
    {
        if(bm_getBitVal(stateBm, i))
        {
            bm_setBitVal(recentRecord->stateChangeBm, i, 1);
        }
    }
}


void streamRecent_close(recentStreamRecord_t *recentRecord)
{
    recentStreamNode_t *curData, *nextData;
    zidfree(recentRecord->stateChangeBm);


    for(curData = recentRecord->head; curData != NULL;)
    {
        nextData = curData->next;
        zidfree(curData->stateBm);
        zidfree(curData);
        curData = nextData;
    }
}
