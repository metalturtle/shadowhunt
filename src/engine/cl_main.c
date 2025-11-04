#include "engine.h"
#include "../basic/world_def.h"
#include "entity.h"

static byte writeBuffer[MAX_MSGLEN];

/********************CLIENT FRAME RUN********************/


void cl_addInputCmd()
{
    if(client.clRep.clState != SYS_RUN)
        return;

    inputCommandList_t *inputCommandList;


    inputCommandList = &vecget(cl_inputList.list, 0);

    if(inpCmd_isFull(inputCommandList))
    {
        printf("inpCmd is full\n");
        return;
    }

    inpCmd_addFromInput(inputCommandList, client.clRep.con->outgoingSequence);

    inputCommand_t *inpCmd = inpCmd_getLast(inputCommandList);
}


void cl_keyEvent(int key)
{
    float x = 0, y = 0;
    float speed = 0.75f;
    bitstream_t bs;


    inpCmd_pressKey(key);
}


void cl_mouseEvent(float x, float y)
{
    inpCmd_moveMouse(x, y);
}


void cl_checkTimeout()
{
    if(checkTimer(&client.clRep.lastRecvTimer))
    {
        com_error(ERR_FATAL, "Error: got disconnected from the server\n");
    }
}

/********************READ SERVER PACKET********************/

void cl_processSysCmd(bitstream_t *readStream)
{
    byte state;

    state = stream_readByte(readStream);
    if(state == SYS_CONNECT)
    {
        printf("client setting state to run\n");
        client.clRep.clState = SYS_RUN;
        startTimer(&client.clRep.sendTimer, 100);

        inputCommandList_t *inpCmdList = &vecget(cl_inputList.list, 0);
        inpCmd_init(inpCmdList);

        printf("setting sending timer \n");
    }
}


void cl_ackInput(bitstream_t *readStream)
{
    int remLen = 0;
    int inpLen = 0;
    inputCommandList_t *inputCommandList;
    inputCommand_t *inpCmd;
    inputCommand_t *lastInp = NULL;
    inputCommand_t *firstInp = NULL;

    // inputCommandList = client.clRep.inputCommandList;
    inputCommandList = &vecget(cl_inputList.list, 0);

    int ackRecordID = stream_readInt(readStream);


    // if(ackRecordID <= inputCommandList->lastRecordID)
    //     return;


    inpLen =  inpCmd_getLen(inputCommandList);

    for(int i = 0; i < inpLen; i++)
    {
        inputCommand_t *inpCmd = inpCmd_get(inputCommandList, i);

        if(inpCmd->recordID == ackRecordID)
        {
            remLen = i + 1;
            lastInp = inpCmd;
            break;
        }
        // if(netcon_getPacketState(client.clRep.con, inpCmd->sequence) == NETCON_PACKET_SUCCESS)
        // {
        //     remLen = i + 1;
        //     lastInp = inpCmd;
        // }
    }

    // if(lastInp != NULL)
    //     printf("acked input %d %d\n", lastInp->recordID, lastInp->sequence);

    if(lastInp != NULL) {
        // if(ABS(P_X - lastInp->inpX) > 0.1 || ABS(P_Y - lastInp->inpY) > 0.1 ) {
        //     printf("acked input %d %d\n", lastInp->recordID, ackRecordID);
        //     printf("mismatch of position %f,%f  %f,%f %p\n", P_X, P_Y, lastInp->inpX, lastInp->inpY, lastInp);
        // }


        if(lastInp->recordID > 10000)
            com_error(ERR_FATAL, "error: incorect input recordID found\n");
    }
    else {
        inpCmd = inpCmd_get(inputCommandList, 0);
        // printf("couldn't find record ID %d %d %d\n", ackRecordID, inputCommandList->lastRecordID, inpCmd->recordID);
        return;
    }

    for(int i = 0; i < remLen; i++)
    {
        inpCmd_removeFirst(inputCommandList);
    }
}


void cl_acknowledge()
{

}


void cl_readEntities(bitstream_t *readStream)
{
    ent_readSerializerList(0, client.clRep.con, readStream);   
}


void cl_readServerCmd(bitstream_t *readStream)
{
    byte cmd;
    int count = 1;

    cl_acknowledge();

    while ((cmd = stream_readByte(readStream)) != SERVCMD_END)
    {
        if(cmd == SERVCMD_SYS)
        {
            cl_processSysCmd(readStream);
        }
        else if(cmd == SERVCMD_ENT)
        {
            cl_readEntities(readStream);
        }
        else if (cmd == SERVCMD_INPUTACK)
        {
            cl_ackInput(readStream);
        }
        else {
            break;
        }
    }


    if(cmd != SERVCMD_END)
    {
        for(int i = readStream->curbyte - 5; i < readStream->curbyte + 2; i++)
        {
            printbit(readStream->buf[i]); printf("\n");
        }
        com_error(ERR_FATAL, "error: last cmd is not equal to NETCMD_END\n");
    }
}


void cl_packetEvent(netaddr_t *fromAddress, byte *data, int len)
{
    bitstream_t readStream;

    if(!netAddrCmp(client.clRep.con->remoteAddress, *fromAddress))
    {
        printf("got packet other than server packet %s\n", netAddrToString(*fromAddress));
        return;
    }

    stream_init(&readStream, data, len);
    readStream.datalen = len;

    netcon_process(client.clRep.con, &readStream);

    if(client.clRep.con->recvState != NETCON_FRAGMENT)
    {
        startTimer(&client.clRep.lastRecvTimer, 3000);

        cl_readServerCmd(&readStream);
    }
}

/********************SEND PACKET********************/


void cl_send(bitstream_t *writeStream)
{

    if(writeStream->curbyte == 0)
        return;


    stream_writeByte(writeStream, CLCMD_END);

    netcon_transmit(client.clRep.con, writeStream->curbyte + 1, (byte *)writeStream->buf);

    while(client.clRep.con->sendState == NETCON_FRAGMENT)
    {
        netcon_transmitFragment(client.clRep.con);
    }
}


void cl_writeSysCmd(bitstream_t *writeStream)
{
    if(client.clRep.clState == SYS_CONNECT)
    {
        if(client.conAttempts < 3)
        {
            if(checkTimer(&client.clRep.sendTimer))
            {
                stream_writeByte(writeStream, CLCMD_SYS);
                stream_writeByte(writeStream, SYS_CONNECT);
                
                printf("send connect packet\n");
                startTimer(&client.clRep.sendTimer, 100);
                client.conAttempts++;
            }
        }
        else
        {
            com_error(ERR_FATAL, "failed to connect to server");
        }
    }
}

void cl_writeInput(bitstream_t *writeStream)
{
    inputCommandList_t *inputCommandList;
    inputCommand_t *inpCmd;
    int inpLen;

    if(client.clRep.clState != SYS_RUN)
        return;
    
    if(!checkTimer(&client.clRep.sendTimer))
        return;

    // inputCommandList = client.clRep.inputCommandList;
    inputCommandList = &vecget(cl_inputList.list, 0);

    inpLen = inpCmd_getLen(inputCommandList);

    if(inpLen == 0)
        return;

    stream_writeByte(writeStream, CLCMD_INPUT);

    inpCmd = inpCmd_get(inputCommandList, 0);

    stream_writeInt(writeStream, inpCmd->recordID);
    stream_writeInt(writeStream, inpLen);
    stream_writeInt(writeStream, inputCommandList->lastRecordID );

    // printf("sending last record id %d \n", inputCommandList->lastRecordID);


    for(int i = 0; i < inpLen; i++)
    {
        inpCmd = inpCmd_get(inputCommandList, i);
        stream_writeBitsData(writeStream, inpCmd->key, inpCmdConfig.keyBitLen);

        int mouseX = (inpCmd->mouseX * 1000);
        int mouseY = (inpCmd->mouseY * 1000);

        stream_writeInt(writeStream, mouseX);
        stream_writeInt(writeStream, mouseY);
    }

}


void cl_writeEntityACK(bitstream_t *writeStream)
{
    if(!checkTimer(&client.clRep.sendTimer))
        return;

    if(client.clRep.clState != SYS_RUN)
        return;


    stream_writeByte(writeStream, CLCMD_ENTACK);
}


void cl_sendPacket()
{
    bitstream_t writeStream;

    if(client.clRep.clState == SYS_IDLE) {
        client.clRep.clState = SYS_CONNECT;
    }
    

    if(!netcon_shouldSend(client.clRep.con)) {
        printf("window limit \n");
        return;
    }



    stream_init(&writeStream, writeBuffer, MAX_MSGLEN);

    cl_writeSysCmd(&writeStream);

    cl_writeInput(&writeStream);

    cl_writeEntityACK(&writeStream);

    cl_send(&writeStream);
}

/********************INIT CLIENT********************/

void cl_init()
{
    zmemset(&client, 0, sizeof(client));

    client.clRep.con = (netcon_t *) zidmalloc(GENERALZONE, sizeof(netcon_t));

    netcon_setup(client.clRep.con);
    netAddrSet(&client.clRep.con->remoteAddress, 127, 0, 0, 1, 8000);

    // ent_initRecordList(&client.clRep.entStateRecordList);

    vecinit(GENERALZONE, cl_inputList.list, inputCommandList_t, 1);


    client.clRep.clState = SYS_IDLE;
}

void cl_frame()
{
    cl_addInputCmd();

    
    eng_processClientEntities();

    // ent_setAllSpritePos();
    // ent_handleSprites();

    cl_sendPacket();


    if(checkTimer(&client.clRep.sendTimer))
    {
        startTimer(&client.clRep.sendTimer, 100);
    }
}