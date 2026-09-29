#include "engine.h"
#include "../basic/world_def.h"
#include "entity.h"
#include "stealth.h"

static byte writeBuffer[MAX_MSGLEN];

static void cl_updateAim(void);

/********************CLIENT FRAME RUN********************/


void cl_addInputCmd()
{
    if(client.clRep.clState != SYS_RUN)
        return;

    inputCommandList_t *inputCommandList;


    inputCommandList = &client.clRep.inputCommandList;

    if(inpCmd_isFull(inputCommandList))
    {
        printf("inpCmd is full\n");
        return;
    }

    cl_updateAim();
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


static float rawMouseX = 1.0f, rawMouseY = 0.5f;

void cl_mouseEvent(float x, float y)
{
    rawMouseX = x;
    rawMouseY = y;
}

/* Cursor position normalized to the window, for the camera look-ahead. */
void cl_getMouse(float *x, float *y)
{
    *x = rawMouseX;
    *y = rawMouseY;
}

/* Aim from the local player's on-screen position toward the cursor, then
 * encode that direction on a circle around the viewport center, which is
 * what the server turns back into an angle. */
static void cl_updateAim(void)
{
    float botRadians;
    if(bot_aim(&botRadians)) {
        inpCmd_moveMouse(0.5f + 0.45f * cosf(botRadians), 0.5f + 0.45f * sinf(botRadians));
        return;
    }

    /* Test harness hook: aim at a fixed world angle in degrees. */
    const char *testAim = getenv("SHADOWHUNT_TEST_AIM");
    if(testAim != NULL && testAim[0] != '\0') {
        float radians = (float)deg2rad(atof(testAim));
        inpCmd_moveMouse(0.5f + 0.45f * cosf(radians), 0.5f + 0.45f * sinf(radians));
        return;
    }

    VectorEntity *local = stealth_localPlayer();
    float originX = 0.5f, originY = 0.5f;
    if(local != NULL && cameraRect.w > 0 && cameraRect.h > 0) {
        originX = (local->pos.x - cameraRect.x) / cameraRect.w;
        originY = (local->pos.y - cameraRect.y) / cameraRect.h;
    }
    float dx = (rawMouseX - originX) * engineParameters.windowWidth;
    float dy = (rawMouseY - originY) * engineParameters.windowHeight;
    float length = sqrtf(dx * dx + dy * dy);
    if(length < 1.0f)
        return;
    inpCmd_moveMouse(0.5f + 0.45f * dx / length, 0.5f + 0.45f * dy / length);
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
        startTimer(&client.clRep.sendTimer, 50);

        inputCommandList_t *inpCmdList = &client.clRep.inputCommandList;
        inpCmd_init(inpCmdList);

        if(com_verbose()) printf("setting sending timer \n");
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
    inputCommandList = &client.clRep.inputCommandList;

    int ackRecordID = stream_readInt(readStream);

    if(shTestLogs)
        printf("acked record ID %d \n", ackRecordID);


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


    }

    for(int i = 0; i < remLen; i++)
    {
        inpCmd_removeFirst(inputCommandList);
    }

    inpLen =  inpCmd_getLen(inputCommandList);
    for(int i = 0; i < inpLen; i++) {
        inputCommand_t *inpCmd = inpCmd_get(inputCommandList, i);
        inpCmd->isDone = false;
    }
}


void cl_acknowledge()
{

}


void cl_readEntities(bitstream_t *readStream)
{
    ent_readSerializerList(&client.clRep, readStream);   
}


void cl_readServerCmd(bitstream_t *readStream)
{
    byte cmd = 0;
    cl_acknowledge();

    while (stream_canRead(readStream, 8) &&
           (cmd = stream_readByte(readStream)) != SERVCMD_END)
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
            return;
        }

        if(stream_overflowed(readStream))
            return;
    }

    if(stream_overflowed(readStream) || cmd != SERVCMD_END)
        return;
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

    if(netcon_process(client.clRep.con, &readStream) < 0)
        return;

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
    if(client.clRep.clState != SYS_CONNECT)
        return;

    stream_writeByte(writeStream, CLCMD_SYS);
    stream_writeByte(writeStream, SYS_CONNECT);
    
    if(com_verbose()) printf("send connect packet\n");
    startTimer(&client.clRep.sendTimer, 50);
    client.conAttempts++;
}

void cl_didConnectFail() {
    if(client.clRep.clState == SYS_CONNECT && client.conAttempts >= 3)
    {
        com_error(ERR_FATAL, "failed to connect to server");
    }
}

void cl_writeInput(bitstream_t *writeStream)
{
    inputCommandList_t *inputCommandList;
    inputCommand_t *inpCmd;
    int inpLen;

    if(client.clRep.clState != SYS_RUN)
        return;


    // inputCommandList = client.clRep.inputCommandList;
    inputCommandList = &client.clRep.inputCommandList;

    inpLen = inpCmd_getLen(inputCommandList);

    if(inpLen == 0)
        return;

    stream_writeByte(writeStream, CLCMD_INPUT);

    inpCmd = inpCmd_get(inputCommandList, 0);

    stream_writeInt(writeStream, inpCmd->recordID);
    stream_writeInt(writeStream, inpLen);
    // stream_writeInt(writeStream, inputCommandList->lastRecordID );

    // printf("sending last record id %d \n", inputCommandList->lastRecordID);


    for(int i = 0; i < inpLen; i++)
    {
        inpCmd = inpCmd_get(inputCommandList, i);
        stream_writeBitsData(writeStream, inpCmd->key, inpCmdConfig.keyBitLen);

        int mouseX = (inpCmd->mouseX * 1000);
        int mouseY = (inpCmd->mouseY * 1000);
        int deltaTimeInt = inpCmd->deltaTime * 1000 * 1000;

        stream_writeInt(writeStream, mouseX);
        stream_writeInt(writeStream, mouseY);
        stream_writeInt(writeStream, deltaTimeInt);
    }

}


void cl_writeEntityACK(bitstream_t *writeStream)
{
    if(client.clRep.clState != SYS_RUN)
        return;


    stream_writeByte(writeStream, CLCMD_ENTACK);
}


void cl_sendPacket()
{
    bitstream_t writeStream;

    if(!checkTimer(&client.clRep.sendTimer)) return;
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



void cl_frame()
{
    cl_addInputCmd();

    
    eng_processClientEntities();

    // ent_setAllSpritePos();
    // ent_handleSprites();

    cl_sendPacket();


    if(checkTimer(&client.clRep.sendTimer))
    {
        startTimer(&client.clRep.sendTimer, 50);
    }
}

void cl_init()
{
    zmemset(&client, 0, sizeof(client));

    client.clRep.con = (netcon_t *) zidmalloc(GENERALZONE, sizeof(netcon_t));

    netcon_setup(client.clRep.con);
    int ip1, ip2, ip3, ip4;
    const char *serverHost = cvar_getString("serverHost");
    int serverPort = cvar_getInt("serverPort");
    if(sscanf(serverHost, "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4) != 4 ||
       ip1 < 0 || ip1 > 255 || ip2 < 0 || ip2 > 255 ||
       ip3 < 0 || ip3 > 255 || ip4 < 0 || ip4 > 255 ||
       serverPort <= 0 || serverPort > 65535) {
        com_error(ERR_FATAL, "Invalid server address %s:%d\n", serverHost, serverPort);
    }
    netAddrSet(&client.clRep.con->remoteAddress, ip1, ip2, ip3, ip4, serverPort);

    // ent_initRecordList(&client.clRep.entStateRecordList);

    // vecinit(GENERALZONE, cl_inputList.list, inputCommandList_t, 1);


    client.clRep.clState = SYS_IDLE;
}

void cl_setup() {

}

void cl_update() {
    // cl_addInputCmd();

    if(client.clRep.clState == SYS_IDLE && net_isReady()) {
        client.clRep.clState = SYS_CONNECT;
    }

    cl_didConnectFail();
    
    // eng_processClientEntities();

    // ent_setAllSpritePos();
    // ent_handleSprites();

    cl_sendPacket();


    if(checkTimer(&client.clRep.sendTimer))
    {
        startTimer(&client.clRep.sendTimer, 50);
    }
}

void cl_cleanup() {

}

void cl_close() {

}
