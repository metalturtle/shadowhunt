#include "../basic/basic.h"
#include "engine.h"
#include "../movement/movement.h"

void inpConfig_storeUsedKeys(char *usedKeys, int usedKeyLen)
{
    memset(inpCmdConfig.usedKeyMap, -1, 256);

    for(int i = 0; i < usedKeyLen; i++)
    {
        inpCmdConfig.usedKeyMap[usedKeys[i]] = i;
    }

    inpCmdConfig.keyBitLen = usedKeyLen;
    inpCmdConfig.keyByteLen = (byte) CEIL(((float) usedKeyLen)/8.0);
    printf("calculating total key bytes %d \n", inpCmdConfig.keyByteLen);
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
        inpCmdList->inpCmdArr[i].isDone = false;
    }

    inpCmdList->start = 1;
    inpCmdList->end = 1;
    // inpCmdList->lastRecordID = 0;

}

void inpCmd_setup(inputCommandList_t *inpCmdList) {

}


void inpCmd_clearPressed()
{
    inpCmdConfig.pressedLen = 0;
}


void inpCmd_moveMouse(float x, float y)
{
    inpCmdConfig.mouseX = x;
    inpCmdConfig.mouseY = y;
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
        // if(!killCmd) bm_setBitVal(inpCmd->key, inpCmdConfig.usedKeyMap[key], 1);
        // else bm_setBitVal(inpCmd->key, inpCmdConfig.usedKeyMap[key], 0);
        bm_setBitVal(inpCmd->key, inpCmdConfig.usedKeyMap[key], 1);
    }

    inpCmd->mouseX = inpCmdConfig.mouseX;
    inpCmd->mouseY = inpCmdConfig.mouseY;
    inpCmd->isDone = false;
    inpCmd->deltaTime = engineParameters.gameDeltaTime;
    inpCmd->posCheck.x = 0;
    inpCmd->posCheck.y = 0;


    inpCmd->sequence = sequence;
    inpCmd->recordID = inpCmdList->end;

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
    inpCmd->recordID = inpCmdList->end;
    inpCmd->isDone = false;

    inpCmdList->end++; 

    return inpCmd;
}

void inpCmd_clear(inputCommandList_t *inpCmdList)
{
    inpCmdList->start = inpCmdList->end;
}


void inpCmd_free(inputCommandList_t *inpCmdList)
{

    zidfree(inpCmdList->inpCmdArr[0].key);

    for(int i = 0; i < INPCMD_MAX_SIZE; i++)
    {
        inpCmdList->inpCmdArr[i].key = NULL;
    }

    inpCmdList->start = 0;
    inpCmdList->end = 0;
}
