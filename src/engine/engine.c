#include "../basic/basic.h"
#include "../basic/world_def.h"
#include "engine.h"
#include "entity.h"
#include "../movement/movement.h"

camera_t worldCamera;

byte keyMap[256];

struct inputCmdConfig_st inpCmdConfig;

#define ENTCHILD_SIZE 3
#define ENTCHILD_SERIALIZE 0
#define ENTCHILD_SPRITE 1
#define ENTCHILD_MOVE 2

void inpConfig_storeUsedKeys(char *usedKeys, int usedKeyLen)
{
    memset(inpCmdConfig.usedKeyMap, -1, 256);

    for(int i = 0; i < usedKeyLen; i++)
    {
        inpCmdConfig.usedKeyMap[usedKeys[i]] = i;
    }

    inpCmdConfig.keyBitLen = usedKeyLen;
    inpCmdConfig.keyByteLen = (byte) CEIL(((float) usedKeyLen)/8.0);
}


void inpCmd_init(inputCommandList_t *inpCmdList)
{
    int totalKeyLen;
    void *ptr;

    totalKeyLen = inpCmdConfig.keyByteLen * INPCMD_MAX_SIZE;

    ptr =  zidmalloc(GENERALZONE, totalKeyLen);

    for(int i = 0; i < INPCMD_MAX_SIZE; i++)
    {
        inpCmdList->inpCmdArr[i].key = ptr;
        ptr += inpCmdConfig.keyByteLen;
    }

    inpCmdList->start = 0;
    inpCmdList->end = 0;
    inpCmdList->lastRecordID = 0;

}

void inpCmd_clearPressed()
{
    inpCmdConfig.pressedLen = 0;
}

void inpCmd_pressKey(char key)
{

    if(inpCmdConfig.pressedLen == 255)
        return;

    
    if(key >= 'A' && key <= 'Z')
        key = (key - 'A') + 'a';

    if(inpCmdConfig.usedKeyMap[key] < 0)
        return;

    inpCmdConfig.keysPressed[inpCmdConfig.pressedLen] = key;
    inpCmdConfig.pressedLen++;
}

qbool inpCmd_isEmpty(inputCommandList_t *inpCmdList)
{
    if(inpCmdList->start == inpCmdList->end)
        return qtrue;
    return qfalse;
}

int inpCmd_getLen(inputCommandList_t *inpCmdList)
{
    return inpCmdList->end - inpCmdList->start;
}

qbool inpCmd_isFull(inputCommandList_t *inpCmdList)
{
    if((inpCmdList->end - inpCmdList->start) == INPCMD_MAX_SIZE)
    {
        return qtrue;
    }
    return qfalse;
}

inputCommand_t *inpCmd_getLast(inputCommandList_t *inpCmdList)
{
    int lastid = (INPCMD_MAX_SIZE + inpCmdList->end - 1) & (INPCMD_MAX_SIZE - 1);
    return &inpCmdList->inpCmdArr[lastid];
}

qbool inpCmd_isPressed(inputCommand_t *inpCmd, char key)
{
    if(bm_getBitVal(inpCmd->key, inpCmdConfig.usedKeyMap[key])) {
        return qtrue;
    }
    return qfalse;
}

void inpCmd_addFromInput(inputCommandList_t *inpCmdList, int sequence)
{
    int ind = (inpCmdList->end & (INPCMD_MAX_SIZE - 1));

    inputCommand_t *inpCmd = &inpCmdList->inpCmdArr[ind];

    for(int i = 0; i < inpCmdConfig.keyByteLen; i++)
    {
        inpCmd->key[i] = 0;
    }

    for(int i = 0; i <inpCmdConfig.pressedLen; i++)
    {
        char key = inpCmdConfig.keysPressed[i];
        bm_setBitVal(inpCmd->key, inpCmdConfig.usedKeyMap[key], 1); 
    }

    inpCmd->sequence = sequence;
    inpCmd->recordID = ++inpCmdList->lastRecordID;

    inpCmdList->end++;
}

void inpCmd_removeFirst(inputCommandList_t *inpCmd)
{
    if(inpCmd_isEmpty(inpCmd))
        return;
    
    inpCmd->start++;
}

inputCommand_t *inpCmd_get(inputCommandList_t *inpCmd, int i)
{
    return &inpCmd->inpCmdArr[(inpCmd->start + i) & (INPCMD_MAX_SIZE - 1)];
}

inputCommand_t *inpCmd_add(inputCommandList_t *inpCmdList, int sequence)
{
    int ind = (inpCmdList->end & (INPCMD_MAX_SIZE - 1));

    inputCommand_t *inpCmd = &inpCmdList->inpCmdArr[ind];

    inpCmd->sequence = sequence;
    inpCmd->recordID = ++inpCmdList->lastRecordID;

    inpCmdList->end++; 

    return inpCmd;
}

void inpCmd_clear(inputCommandList_t *inpCmdList)
{
    inpCmdList->start = inpCmdList->end = 0;
}

void think(entityThink_t *entThink)
{
    int entID;
    inputCommand_t *inpCmd;
    int inpLen;
    entitySerialize_t *entSerialize;
    inputCommandList_t *inputCommandList;
    animatedSprite_t *entSprite;
    entVec_t *entPos;
    entityMove_t *entMove;

    entID = entThink->entID;
    entPos = ent_getPos(entID);
    entSerialize = ent_getSerialize(ent_getChildID(entID, ENTCHILD_SERIALIZE));
    entMove = ent_getMove(ent_getChildID(entID, ENTCHILD_MOVE));

    inputCommandList = entThink->inputCommandList;
    inpLen = inpCmd_getLen(inputCommandList);

    vec3xyz(entMove->dir, 0, 0, 0);
    entMove->speed = 0;

    for(int i = 0; i < inpLen; i++)
    {
        inpCmd = inpCmd_get(inputCommandList, i);

        byte up = bm_getBitVal(inpCmd->key, 0);
        byte down = bm_getBitVal(inpCmd->key, 1);
        byte left = bm_getBitVal(inpCmd->key, 2);
        byte right = bm_getBitVal(inpCmd->key, 3);

        vec3xyz(entMove->dir,
            right - left
            ,up - down
            ,0);

        bm_setBitVal(entSerialize->stateFlags, 0,
            bm_getBitVal(entSerialize->stateFlags, 0) | ABS(up - down));
        bm_setBitVal(entSerialize->stateFlags, 1,
            bm_getBitVal(entSerialize->stateFlags, 1) | ABS(right - left));
        
        entMove->speed += 40;
    }

    // if(entMove->dir[0] > 0)
    //     printf("think dir (%f, %f), speed %f\n", entMove->dir[0], entMove->dir[1], entMove->speed);

    entSprite = ent_getAnimSprite(ent_getChildID(entID, ENTCHILD_SPRITE));

    if(vec3dist(entSprite->pos, entMove->pos) > 0.1f)
    {
        entSprite->curSprite += 0.01f;
        entSprite->curSprite = FRACT(entSprite->curSprite);
    }
    else {
        entSprite->curSprite = 0;
    }

    vec3set(entPos->vec,  entMove->pos);
}

void writeEnt(entitySerialize_t *entSerialize, bitstream_t *bs, byte *bm)
{
    entVec_t *entVec = ent_getPos(entSerialize->entID);

    int xval = (int)(entVec->vec[0] * 100);
    int yval = (int)(entVec->vec[1] * 100);

    for(int i = 0; i < 2; i++)
    {
        if(bm_getBitVal(bm, i))
        {
            switch(i)
            {
                case 0:
                    printf("writing yval %f\n", entVec->vec[1]);
                    stream_writeInt(bs, yval);
                    break;
                case 1:
                    printf("writing xval %f\n", entVec->vec[0]);
                    stream_writeInt(bs, xval);
                    break;
            }
        }
    }
}

void readEnt(entitySerialize_t *entSerialize, bitstream_t *bs, byte *bm)
{
    int xval, yval;

    xval = yval = 0;

    entVec_t *entVec = ent_getPos(entSerialize->entID);

    animatedSprite_t *entSprite = ent_getAnimSprite(entSerialize->entID);

    for(int i = 0; i < 2; i++)
    {
        if(bm_getBitVal(bm, i))
        {
            switch(i)
            {
                case 0:
                    printf("pressed yval\n");
                    yval = stream_readInt(bs);
                    entVec->vec[1] = ((float)yval)/100.0;
                    entSprite->pos[1] = entVec->vec[1];
                    break;
                case 1:
                    printf("pressed xval\n");
                    xval = stream_readInt(bs);
                    entVec->vec[0] = ((float)xval)/100.0;
                    entSprite->pos[0] = entVec->vec[0];
                    break;
            }
        }
    }
    printf("client received val x=%f, y=%f\n", entVec->vec[0] , entVec->vec[1] );
}

int add_testEnt(void *data)
{
    vec3_t vec;
    int entID;
    int thinkID;
    int serializeID;
    int spriteID;
    int moveID;
    vec3_t dist;
    float pos[2] = {50.0f, 10.0f};
    // float rect[4] = {0.0f, 0.0f, 5.0f, 5.0f};
    rect2_t rect;

    inputCommandList_t *inputCommandList = (inputCommandList_t *) data;
    
    printf("before adding entity\n");
    vec3xyz(vec, 250, 270, 0);
    rect2xywh(rect, -5, -5, 10, 10);

    entID = ent_addEnt(vec, ENTCHILD_SIZE);
    thinkID = ent_addThink(entID, inputCommandList, think);
    serializeID = ent_addSerialize(entID, 2, readEnt, writeEnt);
    spriteID = ent_addAnimSprite(entID, "actor_torso_attack_machgun", pos, rect, 0);
    moveID = ent_addMove(entID, vec, dist, rect, 0);

    ent_setChildID(entID, ENTCHILD_SERIALIZE, serializeID);
    ent_setChildID(entID, ENTCHILD_SPRITE, spriteID);
    ent_setChildID(entID, ENTCHILD_MOVE, moveID);
    printf("after adding entity\n");

    return entID;
}


void eng_init()
{
    cvar_t *cv_isServer;

    int isServer = cvar_getInt("isServer");

    if(isServer)
    {
        serv_init();
    }
    else {
        cl_init();
    }

    // world_load();
    physics_init();

    char keys[] = {'w', 's', 'a', 'd'};

    inpConfig_storeUsedKeys(keys, sizeof(keys));

    ent_init();

    ent_addCreator(add_testEnt);
    
}

void eng_handleEvents()
{
    sysEvent_t *ev;
    cvar_t *cv_isServer;
    byte *buf;
    int len;

    int isServer = cvar_getInt("isServer");

    // clearKey();

    inpCmd_clearPressed();
    
    int evNum = 0;
    while(!isSysEventEmpty())
    {
        ev = getSysEvent();
        switch(ev->type)
        {
            case SYSEVENT_KEY:
                if(!isServer) {
                    cl_keyEvent(ev->value);
                }
                break;
            case SYSEVENT_PACKET:
                buf = (byte *) ev->ptr;
                len = ev->value;
                netaddr_t *fromAddr = (netaddr_t *) buf;
                buf += sizeof(netaddr_t);
                len -= sizeof(netaddr_t);
                // printf("buf %p len=%d\n", buf, len);
                if(isServer) {
                    // printf("server reading packet\n");
                    serv_packetEvent(fromAddr, buf, len);
                }
                else {
                    // printf("client reading packet\n");
                    cl_packetEvent(fromAddr, buf, len);
                }
                zidfree(ev->ptr);
                break;
        }
    }

    if(isServer) {
        // serv_sendPacketAll();
        serv_frame();
    }
    else {
        cl_frame();
    }
}


void eng_runFrame()
{
    eng_handleEvents();
}