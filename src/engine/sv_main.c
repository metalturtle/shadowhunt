#include <string.h>
#include "../basic/basic.h"
#include "../basic/world_def.h"
#include "engine.h"
#include "entity.h"

static netcon_t *nextCon;

static char writeBuffer[MAX_MSGLEN];

/********************CLIENT UTILS********************/

int serv_checkClientRepMap(netaddr_t *clAddr)
{
    const char *netStr = netAddrToString(*clAddr);

    int conid = s2imap_get(server.clRepMap, netStr);
    return conid;
}

/********************ADDING CLIENT********************/

void serv_setupNewClient(serv_clrep_t *newClRep)
{
    entitySerialize_t *entSerialize;
    serv_clrep_t *addRep;
    bitstream_t bs;

    ent_initRecordList(&newClRep->entStateRecordList);

    newClRep->inputCommandList = (inputCommandList_t *) zidmalloc(GENERALZONE, sizeof(inputCommandList_t));
    inpCmd_init(newClRep->inputCommandList);

    int entID = ent_createEnt(0, newClRep->inputCommandList);

    entSerialize = ent_getSerializeFromEntID(entID);

    for(int i = 0; i < vecsize(server.clRepList); i++)
    {
        if(!bm_getBitVal(server.clRepBitMap.arr, i))
            continue;

        addRep = &vecget(server.clRepList, i);

        ent_addPermanent(entID, addRep->con, &addRep->entStateRecordList);
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

        freeid = server.clRepList.size - 1;
        clRep = &(vecget(server.clRepList, freeid));
    }
    else
    {
        bm_setBitVal(server.clRepBitMap.arr, freeid, 1);
        clRep = &(vecget(server.clRepList, freeid));
    }

    clRep->con = nextCon;
    clRep->clState = SYS_IDLE;
    printf("adding client. conid=%d, clreplist=%d\n", freeid);
    s2imap_put(server.clRepMap, netAddrToString(nextCon->remoteAddress), freeid);
    startTimer(&clRep->lastRecvTimer, 3000);

    nextCon = (netcon_t *) zidmalloc(GENERALZONE, sizeof(netcon_t));
    netcon_setup(nextCon);

    serv_setupNewClient(clRep);

    return clRep;
}

/********************DISCONNECT CLIENT********************/

void serv_disconnectClient(int conid)
{
    serv_clrep_t *clRep;
    clRep = &vecget(server.clRepList, conid);
    
    zidfree(clRep->con);
    bm_setBitVal(server.clRepBitMap.arr, conid, 0);
    s2imap_remove(server.clRepMap, netAddrToString(clRep->con->remoteAddress));
}

void serv_checkTimeout()
{
    serv_clrep_t *clRep;
    for(int i = 0; i < server.clRepList.size; i++)
    {
        if(!bm_getByteVal(server.clRepBitMap.arr, i))
            continue;

        clRep = &vecget(server.clRepList, i);
        
        if(checkTimer(&clRep->lastRecvTimer))
        {
            serv_disconnectClient(i);
            printf("server disconnected client %d\n", i);
        }
    }
}

/********************READ CLIENT PACKET********************/

void serv_acknowledge(serv_clrep_t *clRep)
{
    ent_ackSerialize(clRep->con, &clRep->entStateRecordList);
}

void serv_readSysCmd(serv_clrep_t *clRep, bitstream_t *readStream)
{
    return;
}

void serv_readInputCmd(serv_clrep_t *clRep, bitstream_t *readStream)
{
    byte key[32];
    inputCommand_t *inpCmd;
    int recordID;
    int arrLen;
    inputCommandList_t *inputCommandList;
    
    inputCommandList = clRep->inputCommandList;

    recordID = stream_readInt(readStream);
    arrLen = stream_readInt(readStream);

    if(recordID > (inputCommandList->lastRecordID + 1))
    {
        printf("recordID: %d, lastRecordID=%d \n", recordID, inputCommandList->lastRecordID);
        com_error(ERR_FATAL, "Error: missed some input commands from the client\n");
    }

    if(inputCommandList->lastRecordID > (recordID + arrLen))
    {
        return;
    }

    for(int i = 0; i < arrLen; i++)
    {
        stream_copyBitsData(readStream, key, inpCmdConfig.keyBitLen);

        if((recordID + i) <= inputCommandList->lastRecordID)
        {
            continue;
        }
            

        inpCmd = inpCmd_add(inputCommandList, 0);
        
        zmemcpy(inpCmd->key, key, inpCmdConfig.keyByteLen);
    }

    inputCommandList->lastRecordID = recordID + arrLen;
}


void serv_readClientMessage(serv_clrep_t *clRep, bitstream_t *readStream)
{
    byte cmd;
    netcon_t *con;

    con = clRep->con;

    serv_acknowledge(clRep);

    while((cmd = stream_readByte(readStream)) != CLCMD_END)
    {
        if(cmd == CLCMD_SYS) {
            serv_readSysCmd(clRep, readStream);
        }
        else if(cmd == CLCMD_INPUT) {
            serv_readInputCmd(clRep, readStream);
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
            serv_readClientMessage(clRep, &bs);
        }
    }
    
}

/********************HANDLE ENTITIES********************/



/********************SEND PACKET TO CLIENT********************/

void serv_send(serv_clrep_t *clRep, bitstream_t *writeStream)
{
    if(clRep->clState == SYS_IDLE)
        return;

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
    byte state;

    if(clRep->clState == SYS_IDLE)
    {
        printf("sending connect state\n");

        stream_writeByte(writeStream, SERVCMD_SYS);
        stream_writeByte(writeStream, SYS_CONNECT);

        clRep->clState = SYS_CONNECT;
    }
}

void serv_writeEntities(serv_clrep_t *clRep, bitstream_t *writeStream)
{
    if(clRep->clState != SYS_CONNECT) {
        return;        
    }

    stream_writeByte(writeStream, SERVCMD_ENT);

    ent_writeAllSerialize(writeStream, clRep->con, &clRep->entStateRecordList);

}

void serv_writeToClient(serv_clrep_t* clRep)
{
    bitstream_t writeStream;

    stream_init(&writeStream, writeBuffer, MAX_MSGLEN);

    serv_writeSysCmd(clRep, &writeStream);

    serv_writeEntities(clRep, &writeStream);
    
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
        if(!bm_getByteVal(server.clRepBitMap.arr, i))
            continue;

        clRep = &vecget(server.clRepList, i);

        serv_writeToClient(clRep);
    }

    startTimer(&server.sendTimer, 100);
}

/********************SERVER RUN COMMAND********************/

void serv_runCmd()
{
    serv_clrep_t *clRep;
    inputCommand_t *inpCmd;
    float speed = 0.75;
    int inpLen;

    ent_runAllThink();

    for(int i = 0; i < server.clRepList.size; i++)
    {
        if(!bm_getByteVal(server.clRepBitMap.arr, i))
            continue;

        clRep = &vecget(server.clRepList, i);

        inpCmd_clear(clRep->inputCommandList);
    }

}

/********************INIT SERVER AND FRAME FUNCTION********************/

void serv_init()
{
    zmemset(&server, 0, sizeof(server));
    
    vecinit(GENERALZONE, server.clRepList, serv_clrep_t, 8);

    vecinit(GENERALZONE, server.clRepBitMap, byte, 8);

    server.clRepMap = s2imap_create(GENERALZONE);
    server.lastConID = 0;

    nextCon = (netcon_t *) zidmalloc(GENERALZONE, sizeof(netcon_t));
    netcon_setup(nextCon);

}

void serv_frame()
{
    serv_checkTimeout();

    serv_runCmd();
    
    serv_sendPacketAll();

}