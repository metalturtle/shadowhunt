#include "../basic/basic.h"
#include "../basic/world_def.h"
#include "engine.h"
#include "entity.h"

camera_t worldCamera;

byte keyMap[256];

struct inputCmdConfig_st inpCmdConfig;

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
    inputCommand_t *inpCmd;
    float speed = 0.75;
    int inpLen;
    entitySerialize_t *entSerialize;
    inputCommandList_t *inputCommandList;
    entitySprite_t *entSprite;

    entVec_t *entVec = ent_getPos(entThink->entID);
    entSerialize = ent_getSerializeFromEntID(entThink->entID);

    inputCommandList = entThink->inputCommandList;

    inpLen = inpCmd_getLen(inputCommandList);

    for(int i = 0; i < inpLen; i++)
    {
        inpCmd = inpCmd_get(inputCommandList, i);

        byte up = bm_getBitVal(inpCmd->key, 0);
        byte down = bm_getBitVal(inpCmd->key, 1);
        byte left = bm_getBitVal(inpCmd->key, 2);
        byte right = bm_getBitVal(inpCmd->key, 3);

        if(up) {
            entVec->vec[1] += speed;
            bm_setBitVal(entSerialize->stateFlags, 0, 1);
            // printf("pressed up\n");
        }
        if(down) {
            entVec->vec[1] -= speed;
            bm_setBitVal(entSerialize->stateFlags, 0, 1);
            // printf("pressed down\n");
        }
        if(left) {
            entVec->vec[0] -= speed;
            bm_setBitVal(entSerialize->stateFlags, 1, 1);
            // printf("pressed left\n");
        }
        if(right) {
            entVec->vec[0] += speed;
            bm_setBitVal(entSerialize->stateFlags, 1, 1);
            // printf("pressed right\n");
        }
    }

    // animatedSprite_t *animSprite = ent_getAnimSprite(entSprite->animID);
    // animSprite->isRunning = 1;
    // printf("entSprite pos %f %f\n", entSprite->pos[0], entSprite->pos[1]);
}

void renderEnt(entitySprite_t *entSprite)
{
    entVec_t *entVec = ent_getPos(entSprite->entID);

    animatedSprite_t *animSprite = ent_getAnimSprite(entSprite->animID);

    if(vec3dist(entSprite->pos, entVec->vec) > 0.1f)
    {
        animSprite->isRunning = 1;
    }
    else {
        animSprite->isRunning = 0;
    }

    vec3set(entSprite->pos, entVec->vec);
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
                    break;
                case 1:
                    printf("pressed xval\n");
                    xval = stream_readInt(bs);
                    entVec->vec[0] = ((float)xval)/100.0;
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

    inputCommandList_t *inputCommandList = (inputCommandList_t *) data;

    vec3xyz(vec, 0, 0, 0);
    entID = ent_addEnt(vec);
    ent_addThink(entID, inputCommandList, think);
    ent_addSerialize(entID, 2, readEnt, writeEnt);
    float pos[2] = {50.0f, 10.0f};
    float rect[4] = {0.0f, 0.0f, 5.0f, 5.0f};
    ent_addSprite(entID, "actor_torso_attack_machgun", SPRITE_TYPE_ANIM, pos, rect, 0, renderEnt);
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

    char keys[] = {'w', 's', 'a', 'd'};

    inpConfig_storeUsedKeys(keys, sizeof(keys));

    ent_init();

    ent_addCreator(add_testEnt);
}

void eng_handleEvents()
{
    sysEvent_t *ev;
    cvar_t *cv_isServer;

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
                // printf("got packet \n");
                byte *buf = (byte *) ev->ptr;
                int len = ev->value;
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