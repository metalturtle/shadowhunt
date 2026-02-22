#include <string.h>
#include "../basic/basic.h"
#include "../basic/world_def.h"
#include "engine.h"
#include "entity.h"

static netcon_t *nextCon;

static byte writeBuffer[MAX_MSGLEN];

inputCommandList_t readCmdList;

/********************CLIENT UTILS********************/

int serv_checkClientRepMap(netaddr_t *clAddr)
{
    const char *netStr = netAddrToString(*clAddr);

    int conid = s2imap_get(server.clRepMap, netStr);
    return conid;
}

/********************ADDING CLIENT********************/

// void serv_addSyncedEnt(int entID, int entType)
// {

//     // ent_addSyncedEntState(entID, entType);


//     serv_clrep_t *addRep;
//     for(int i = 0; i < vecsize(server.clRepList); i++)
//     {
//         if(!bm_getBitVal(server.clRepBitMap.arr, i))
//             continue;


//         addRep = &vecget(server.clRepList, i);
//         printf("adding synced entity for the client %d \n", i);
//         ent_addSyncedEntToClient(entID, addRep, entType);
//     }
// }

// void serv_setupEntitiesForSerializer(int entID, int entTY

void serv_setupNewClient(serv_clrep_t *newClRep, int conID)
{
    ent_initializeClient(newClRep);
   // ent_handleClientJoin(conID, newClRep);
    intPair_t pair = ent_setupEntityForClient(conID, newClRep->con);

    int entID = pair.a;
    int entType = pair.b;
    if(entID < 0)
        return;
    
    NetEntity *netEnt = &netEntityList[entID];
    netEnt->clientOwner = newClRep->conID;

    // printf("adding synced entity %d\n", entID);
    // serv_addSyncedEnt(entID, entType);
}


void serv_removeSyncedEnt(int entID, int entType)
{

    ent_removeSyncedEntState(entID, entType);


    serv_clrep_t *addRep;
    for(int i = 0; i < vecsize(server.clRepList); i++)
    {
        if(!bm_getBitVal(server.clRepBitMap.arr, i))
            continue;


        addRep = &vecget(server.clRepList, i);
        printf("removing synced entity for the client %d \n", i);
        ent_removeSyncedEntFromClient(entID, addRep, entType);
    }
}

serv_clrep_t *serv_addClient()
{
    int fset;
    serv_clrep_t *clRep;
    char addrStr[64];

    int freeid = bm_findEmpty(server.clRepBitMap.arr, server.clRepBitMap.size);
    
    if(freeid < 0)
    {
        vecpush(server.clRepBitMap, byte, 1);
        vecpushempty(server.clRepList, serv_clrep_t);
        // vecpushempty(cl_inputList.list, inputCommandList_t);

        freeid = server.clRepList.size - 1;
        clRep = &(vecget(server.clRepList, freeid));
        // inpCmdList = &vecget(cl_inputList.list, freeid);
    }
    else
    {
        clRep = &(vecget(server.clRepList, freeid));
        // inpCmdList = &vecget(cl_inputList.list, freeid);
    }

    inputCommandList_t *inpCmdList = &clRep->inputCommandList;
    clRep->con = nextCon;
    clRep->clState = SYS_IDLE;
    clRep->conID = freeid;
    bm_setBitVal(server.clRepBitMap.arr, freeid, 1);
    printf("\n\n\nadding client. conid=%d\n", freeid);
    s2imap_put(server.clRepMap, netAddrToString(nextCon->remoteAddress), freeid);
    startTimer(&clRep->lastRecvTimer, 3000);
    inpCmd_init(inpCmdList);


    nextCon = (netcon_t *) zidmalloc(GENERALZONE, sizeof(netcon_t));
    netcon_setup(nextCon);
    int wid = neEntSys_initSnapshot();
    clRep->worldSnapshotID = wid;

    serv_setupNewClient(clRep, freeid);

    return clRep;
}

/********************DISCONNECT CLIENT********************/

void serv_disconnectClient(int conID)
{
    serv_clrep_t *clRep;
    clRep = &vecget(server.clRepList, conID);

    intPair_t entIDPair = ent_removeEntityFromClient(conID, clRep->con);


    ent_handleClientLeave(clRep);


    zidfree(clRep->con);


    bm_setBitVal(server.clRepBitMap.arr, conID, 0);


    s2imap_remove(server.clRepMap, netAddrToString(clRep->con->remoteAddress));


    inputCommandList_t *inpCmdList = &clRep->inputCommandList;
    inpCmd_free(inpCmdList);

    serv_removeSyncedEnt(entIDPair.a, entIDPair.b);
}

void serv_checkTimeout()
{
    serv_clrep_t *clRep;
    for(int i = 0; i < server.clRepList.size; i++)
    {
        if(!bm_getBitVal(server.clRepBitMap.arr, i))
            continue;

        clRep = &vecget(server.clRepList, i);
        
        if(checkTimer(&clRep->lastRecvTimer))
        {
            // serv_disconnectClient(i);
            // com_error(ERR_FATAL, "disconnect\n");
        }
    }
}

/********************READ CLIENT PACKET********************/

void serv_acknowledge(int conID, serv_clrep_t *clRep, bitstream_t *bs)
{
    ent_ackSerializerList(clRep, bs);
}

void serv_ackEntities(int conID, serv_clrep_t *clRep, bitstream_t *bs)
{    
    ent_ackSerializerList(clRep, bs);
}

void serv_readSysCmd(serv_clrep_t *clRep, bitstream_t *readStream)
{
    return;
}

void serv_readInputCmd(int conID, serv_clrep_t *clRep, bitstream_t *readStream)
{
    byte key[32];
    inputCommand_t *inpCmd;
    int recordID;
    int arrLen;
    inputCommandList_t *inputCommandList;


    inputCommandList = &clRep->inputCommandList;

    recordID = stream_readInt(readStream);
    arrLen = stream_readInt(readStream);

    // printf("read input cmd %d %d\n", conID, arrLen);
    // int lastRecordID = stream_readInt(readStream);


    // if(recordID > (inputCommandList->lastRecordID + 1))
    // {
    //     printf("recordID: %d, lastRecordID=%d \n", recordID, inputCommandList->lastRecordID);
    //     com_error(ERR_FATAL, "Error: missed some input commands from the client\n");
    // }


    inpCmd_clear(&readCmdList);


    for(int i = 0; i < arrLen; i++)
    {
        stream_copyBitsData(readStream, key, inpCmdConfig.keyBitLen);
        int mouseXInt = stream_readInt(readStream);
        int mouseYInt = stream_readInt(readStream);
        float deltaTime = stream_readInt( readStream);
        deltaTime /= (float)(1000 * 1000);
        // printf("server received delta time %f \n", deltaTime);

        if((recordID + i) < inputCommandList->end)
        {
            continue;
        }
            

        inpCmd = inpCmd_add(&readCmdList, 0);
        
        zmemcpy(inpCmd->key, key, inpCmdConfig.keyByteLen);


        inpCmd->mouseX = ((float)mouseXInt)/1000.0;
        inpCmd->mouseY = ((float)mouseYInt)/1000.0;
        inpCmd->deltaTime = deltaTime;
    }


    // if(inputCommandList->lastRecordID >= lastRecordID)
    // {
    //     printf("returned \n");
    //     return;
    // }


    inputCommand_t *readInpCmd;
    for(int i = 0; i < inpCmd_getLen(&readCmdList); i++)
    {
        readInpCmd = inpCmd_get(&readCmdList, i);
        inpCmd = inpCmd_add(inputCommandList, 0);


        zmemcpy(inpCmd->key, readInpCmd->key, inpCmdConfig.keyByteLen);

        inpCmd->mouseX = readInpCmd->mouseX;
        inpCmd->mouseY = readInpCmd->mouseY;
        inpCmd->deltaTime = readInpCmd->deltaTime;
        inpCmd->posCheck.x = inpCmd->posCheck.y = 0;
        // printf("input check %p %p %p %d %f,%f\n", inputCommandList, inpCmd, inpCmd_get(inputCommandList, inpCmd_getLen(inputCommandList) - 1), inpCmd_getLen(inputCommandList), inpCmd->mouseX, inpCmd->mouseY);
    }


    // inputCommandList->lastRecordID = lastRecordID;
}


void serv_readClientMessage(int conID, serv_clrep_t *clRep, bitstream_t *readStream)
{
    byte cmd;
    netcon_t *con;

    con = clRep->con;

    // serv_acknowledge(conID, clRep, readStream);

    while((cmd = stream_readByte(readStream)) != CLCMD_END)
    {
        if(cmd == CLCMD_SYS) {
            serv_readSysCmd(clRep, readStream);
        }
        else if(cmd == CLCMD_INPUT) {
            serv_readInputCmd(conID, clRep, readStream);
        }
        else if (cmd == CLCMD_ENTACK) {
            // ent_ackSerializerList(conID, clRep->con, readStream);
            serv_ackEntities(conID, clRep, readStream);
        }
        else {
            break;
        }
    }

    if(cmd != CLCMD_END)
    {
        cmd = stream_readByte(readStream);
        printf("checking last cmd=%d, netcmd_end=%d \n", cmd, CLCMD_END);
        com_error(ERR_FATAL, "error: last cmd is not equal to CLCMD_END\n");
    }

}

int serv_readNewConnection(netcon_t *con, bitstream_t *readStream)
{
    byte cmd;
    byte state;

    cmd = stream_readByte(readStream);

    if(cmd != CLCMD_SYS)
        return -1;

    state = stream_readByte(readStream);

    if(state != SYS_CONNECT)
        return -1;

    printf("connect %p %p \n", con, nextCon);
    serv_addClient();

    return 0;
}


void serv_packetEvent(netaddr_t *fromAddress, byte *data, int len)
{
    netcon_t *con;
    serv_clrep_t *clRep;
    int conid;
    bitstream_t bs;

    conid = serv_checkClientRepMap(fromAddress);
    
    if(conid == -1)
    {
        zmemcpy(&nextCon->remoteAddress, fromAddress, sizeof(netaddr_t));

        stream_init(&bs, data, len);
        bs.datalen = len;

        netcon_process(nextCon, &bs);

        if(nextCon->recvState != NETCON_FRAGMENT)
        {
            serv_readNewConnection(nextCon, &bs);
        }
    }
    else
    {
        clRep = &vecget(server.clRepList, conid);
        startTimer(&clRep->lastRecvTimer, 3000);
        con = clRep->con;

        stream_init(&bs, data, len);
        bs.datalen = len;

        netcon_process(con, &bs);

        if(con->recvState != NETCON_FRAGMENT)
        {
            serv_readClientMessage(conid, clRep, &bs);
        }
    } 
}

/********************HANDLE ENTITIES********************/



/********************SEND PACKET TO CLIENT********************/

void serv_send(serv_clrep_t *clRep, bitstream_t *writeStream)
{

    if(writeStream->curbyte == 0)
        return;

    stream_writeByte(writeStream, SERVCMD_END);

    netcon_transmit(clRep->con, writeStream->curbyte + 1, (byte *)writeStream->buf);

    while(clRep->con->sendState == NETCON_FRAGMENT)
    {
        netcon_transmitFragment(clRep->con);
    }
}

void serv_writeSysCmd(serv_clrep_t *clRep, bitstream_t *writeStream)
{
    if(clRep->clState == SYS_IDLE)
    {
        printf("sending connect state\n");

        stream_writeByte(writeStream, SERVCMD_SYS);
        stream_writeByte(writeStream, SYS_CONNECT);

        clRep->clState = SYS_CONNECT;
    }
}

void serv_writeInputACK(int conID, serv_clrep_t *clRep, bitstream_t *writeStream)
{
    inputCommandList_t *inputCommandList;

    if(clRep->clState != SYS_CONNECT) {
        return;        
    }

    inputCommandList = &clRep->inputCommandList;

    printf("writing acked input %d \n", inputCommandList->start - 1);
    stream_writeByte(writeStream, SERVCMD_INPUTACK);
    stream_writeInt(writeStream, inputCommandList->start - 1);
}


void serv_writeEntities(int conID, serv_clrep_t *clRep, bitstream_t *writeStream)
{
    if(clRep->clState != SYS_CONNECT) {
        return;        
    }

    stream_writeByte(writeStream, SERVCMD_ENT);

    // ent_writeAllSerialize(writeStream, clRep->con, &clRep->entStateRecordList);

    ent_writeSerializerList(clRep, writeStream);
}


void serv_writeToClient(int conID, serv_clrep_t* clRep)
{
    bitstream_t writeStream;

    stream_init(&writeStream, writeBuffer, MAX_MSGLEN);


    serv_writeSysCmd(clRep, &writeStream);


    serv_writeEntities(conID, clRep, &writeStream);


    serv_writeInputACK(conID, clRep, &writeStream);


    serv_send(clRep, &writeStream);

}


void serv_sendPacketAll()
{
    serv_clrep_t *clRep;

    if(!checkTimer(&server.sendTimer)) {
        return;
    }

    for(int i = 0; i < server.clRepList.size; i++)
    {
        if(!bm_getBitVal(server.clRepBitMap.arr, i))
            continue;

        clRep = &vecget(server.clRepList, i);


            
        if(!netcon_shouldSend(clRep->con)) {
            continue;
        }


        serv_writeToClient(i, clRep);
    }

    resetNetEnt();

    // ent_cleanupSerializerState();

    ent_settleStateDiff();

    startTimer(&server.sendTimer, 100);
}

/********************SERVER RUN COMMAND********************/

void serv_clearInputs()
{
    serv_clrep_t *clRep;
    // inputCommand_t *inpCmd;
    inputCommandList_t *inpCmdList;
    float speed = 0.75;
    int inpLen;

    // eng_processServerEntities();
    // ent_runMove();
    
    // ent_runAllThink();

    for(int i = 0; i < server.clRepList.size; i++)
    {
        if(!bm_getBitVal(server.clRepBitMap.arr, i))
            continue;

        clRep = &vecget(server.clRepList, i);
        // clRep = &vecget(server.clRepList, i);
        inpCmdList = &clRep->inputCommandList;


        // inpCmd_clear(clRep->inputCommandList);
        inpCmd_clear(inpCmdList);
    }

}

/********************INIT SERVER AND FRAME FUNCTION********************/

void sv_init() {
    zmemset(&server, 0, sizeof(server));
    
    vecinit(GENERALZONE, server.clRepList, serv_clrep_t, 8);

    vecinit(GENERALZONE, server.clRepBitMap, byte, 8);

    // vecinit(GENERALZONE, cl_inputList.list, inputCommandList_t, 8);

    server.clRepMap = s2imap_create(GENERALZONE);
    server.lastConID = 0;

    nextCon = (netcon_t *) zidmalloc(GENERALZONE, sizeof(netcon_t));
}

void serv_init()
{
    inpCmd_init(&readCmdList);
    sv_init();  // Initialize server state including nextCon
}

void serv_frame()
{
    netcon_setup(nextCon);
}



void sv_setup() {
    netcon_setup(nextCon);
}

void sv_update() {
    // printf("serv_frame\n");
    // serv_checkTimeout();

    // serv_runCmd();
    
    serv_sendPacketAll();
}

void sv_cleanup() {

}

void sv_close() {

}