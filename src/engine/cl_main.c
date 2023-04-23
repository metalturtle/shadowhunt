#include "engine.h"
#include "../basic/world_def.h"
#include "entity.h"

static char writeBuffer[MAX_MSGLEN];

/********************CLIENT FRAME RUN********************/

void cl_addInputCmd()
{
    if(client.clRep.clState != SYS_RUN)
        return;

    inputCommandList_t *inputCommandList;

    inputCommandList = client.clRep.inputCommandList;

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
    }
}

void cl_ackInput()
{
    int remLen = 0;
    int inpLen = 0;
    inputCommandList_t *inputCommandList;

    inputCommandList = client.clRep.inputCommandList;

    inpLen =  inpCmd_getLen(inputCommandList);

    for(int i = 0; i < inpLen; i++)
    {
        inputCommand_t *inpCmd = inpCmd_get(inputCommandList, i);

        if(netcon_getPacketState(client.clRep.con, inpCmd->sequence) == NETCON_PACKET_SUCCESS)
        {
            remLen = i + 1;
        }
    }

    for(int i = 0; i < remLen; i++)
    {
        inpCmd_removeFirst(inputCommandList);
    }
}

void cl_acknowledge()
{
    cl_ackInput();
}

void cl_readEntities(bitstream_t *readStream)
{
    ent_readSerialize(readStream, &client.clRep.entStateRecordList, client.clRep.inputCommandList);

    entVec_t *vec = ent_getPos(0);

    float diff[] = {-30, -30};
    rect2xywh(worldCamera.window, diff[0], diff[1], getScreenWidth(), getScreenHeight());
    vec2add(worldCamera.window, worldCamera.window, vec->vec);
    
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
            printf("processing syscmd\n");
            cl_processSysCmd(readStream);
        }
        else if(cmd == SERVCMD_ENT)
        {
            cl_readEntities(readStream);
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
    if(client.clRep.clState == SYS_IDLE)
        return;

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

    inputCommandList = client.clRep.inputCommandList;

    inpLen = inpCmd_getLen(inputCommandList);

    if(inpLen == 0)
        return;

    stream_writeByte(writeStream, CLCMD_INPUT);

    inpCmd = inpCmd_get(inputCommandList, 0);

    stream_writeInt(writeStream, inpCmd->recordID);
    stream_writeInt(writeStream, inpLen);

    for(int i = 0; i < inpLen; i++)
    {
        inpCmd = inpCmd_get(inputCommandList, i);
        stream_writeBitsData(writeStream, inpCmd->key, inpCmdConfig.keyBitLen);
    }

    startTimer(&client.clRep.sendTimer, 50);
}

void cl_sendPacket()
{
    bitstream_t writeStream;

    if(client.clRep.clState == SYS_IDLE) {
        client.clRep.clState = SYS_CONNECT;
    }

    stream_init(&writeStream, writeBuffer, MAX_MSGLEN);

    cl_writeSysCmd(&writeStream);

    cl_writeInput(&writeStream);

    cl_send(&writeStream);
}

/********************INIT CLIENT********************/

void cl_init()
{
    zmemset(&client, 0, sizeof(client));

    client.clRep.con = (netcon_t *) zidmalloc(GENERALZONE, sizeof(netcon_t));

    netcon_setup(client.clRep.con);
    netAddrSet(&client.clRep.con->remoteAddress, 127, 0, 0, 1, 8000);

    ent_initRecordList(&client.clRep.entStateRecordList);

    client.clRep.inputCommandList = (inputCommandList_t *) zidmalloc(GENERALZONE, sizeof(inputCommandList_t));
    inpCmd_init(client.clRep.inputCommandList);

    client.clRep.clState = SYS_IDLE;
}

void cl_frame()
{
    cl_addInputCmd();

    // ent_setAllSpritePos();
    ent_handleSprites();

    cl_sendPacket();
}