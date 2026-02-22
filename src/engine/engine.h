#ifndef ENGINE_H
#define ENGINE_H

#include "../basic/basic.h"
#include "../basic/world_def.h"
#include <SDL3/SDL.h>
#include <chipmunk/chipmunk.h>
// #include <box2d/box2d.h>

#define RELSTREAMWINDSIZE 4

extern int STREAM_REC;

typedef struct quickStreamNode_st
{
    struct quickStreamNode_st *next;
    byte *data;
    int bitLen;
    int sendSequence;
} quickStreamNode_t;

typedef struct quickStreamRecord_t
{
    quickStreamNode_t *head;
    quickStreamNode_t *tail;
    bitstream_t bs;
    void *data;
    int recordCount;
    int (*readFunc)(bitstream_t *, void *);
    int (*writeFunc)(bitstream_t *, void *);
} quickStreamRecord_t;

typedef struct relStreamNode_st
{
    struct relStreamNode_st *next;
    byte *data;
    int bitLen;
    int recordID;
    int sendSequence;
    byte isSent;
} relStreamNode_t;

typedef struct relStreamRecord_st
{
    relStreamNode_t *head;
    relStreamNode_t *tail;
    int windowStartRecordID;
    int lastRecordID;
    int recordCount;
    int (*readFunc)(bitstream_t *);
    int (*writeFunc)(bitstream_t *);
} relStreamRecord_t;

typedef struct unrelStreamRecord_st
{
    int (*readFunc)(bitstream_t *);
    int (*writeFunc)(bitstream_t *);
} unrelStreamRecord_t;

typedef struct recentStreamNode_st
{
    struct recentStreamNode_st *next;
    struct recentStreamNode_st *prev;
    byte *stateBm;
    int sendSequence;
} recentStreamNode_t;

typedef struct recentStreamRecord_st
{
    recentStreamNode_t *head;
    recentStreamNode_t *tail;
    byte *stateChangeBm;
    int stateBitLen;
    int stateByteLen;
    int (*readFunc)(bitstream_t *, byte *);
    int (*writeFunc)(bitstream_t *,byte *);
} recentStreamRecord_t;

extern void streamQuick_begin(quickStreamRecord_t *quickRecord, netcon_t *con, bitstream_t *bs);
extern void streamQuick_end(quickStreamRecord_t *quickRecord, netcon_t *con, bitstream_t *bs);

extern void streamQuick_init(quickStreamRecord_t *quickRecord, int (*readFunc)(bitstream_t *, void *), int (*writeFunc)(bitstream_t *, void *));
extern int streamQuick_callWriteFunc(quickStreamRecord_t *quickRecord, netcon_t *con, void *);
extern int streamQuick_getRecordCount(quickStreamRecord_t *quickRecord);
extern void streamQuick_writePacket(quickStreamRecord_t *quickRecord, bitstream_t *bs, netcon_t *con);
extern void streamQuick_readPacket(quickStreamRecord_t *quickRecord, bitstream_t *bs);
extern void streamQuick_acknowledge(quickStreamRecord_t *quickRecord, netcon_t *con);
extern void streamQuick_addPayload(quickStreamRecord_t *quickRecord, void *data);
extern int streamQuick_readCount(quickStreamRecord_t *quickRecord, bitstream_t *bs);
extern void streamQuick_close(quickStreamRecord_t *quickRecord);

extern void streamReliable_init(relStreamRecord_t *relRecord, int (*readFunc)(bitstream_t *), int (*writeFunc)(bitstream_t *));
extern int streamReliable_callWriteFunc(relStreamRecord_t *relRecord, netcon_t *con);
extern void streamReliable_writePacket(relStreamRecord_t *relRecord ,bitstream_t *bs, netcon_t *con);
extern void streamReliable_readPacket(relStreamRecord_t *relRecord, bitstream_t *bs);
extern void streamReliable_acknowledge(relStreamRecord_t *relRecord, netcon_t *con);
extern void streamRecent_close(recentStreamRecord_t *recentRecord);

// extern void streamRecent_init(recentStreamRecord_t *recentRecord, int stateSize, int (*readFunc)(bitstream_t *, byte *), int (*writeFunc)(bitstream_t *, byte *));
extern void streamRecent_init(
    recentStreamRecord_t *recentRecord,
    int stateSize,
    int (*readFunc)(bitstream_t *, byte *),
    int (*writeFunc)(bitstream_t *, byte *),
    byte *stateChangeBm
    );
extern void streamRecent_writePacket(recentStreamRecord_t *recentRecord, bitstream_t *bs, netcon_t *con);
extern void streamRecent_readPacket(recentStreamRecord_t *recentRecord, bitstream_t *bs);
extern void streamRecent_acknowledge(recentStreamRecord_t *recentRecord, netcon_t *con);
extern void streamRecent_setState(recentStreamRecord_t *recentRecord, int i, byte flag);
extern int streamRecent_writeStateBits(recentStreamRecord_t *recentRecord, bitstream_t *bs, netcon_t *con, byte *stateBm);
extern void streamRecent_readStateBits(int stateLen, bitstream_t *bs, byte *stateBm);
extern void streamRecent_setStateBits(recentStreamRecord_t *recentRecord, byte *stateBm);

/********************ENTITY********************/


// extern camera_t worldCamera;

/********************ENGINE********************/

extern void eng_handleEvents();

/********************CLIENT INPUT********************/

#define INPCMD_MAX_SIZE 32

struct vec2_st
{
    float vec[2];
};

struct inputCmdConfig_st
{
    int keyByteLen;
    int keyBitLen;

    char usedKeyMap[256];

    char keysPressed[256];
    int pressedLen;


    float mouseX;
    float mouseY;

};

typedef struct inputCommand_st
{
    byte *key;
    float mouse[2];
    int sequence;
    int recordID;
    short timeTaken;
    float inpX;
    float inpY;
    float mouseX;
    float mouseY;
    bool isDone;
    float deltaTime;
    SDL_FPoint posCheck;
    unsigned long timestamp;

} inputCommand_t;

extern float P_X,P_Y;

typedef struct inputCommandList_st
{
    inputCommand_t inpCmdArr[INPCMD_MAX_SIZE];
    // unsigned int lastRecordID;
    unsigned int start;
    unsigned int end;

} inputCommandList_t;

extern struct inputCmdConfig_st inpCmdConfig;

extern void inpConfig_storeUsedKeys(char *usedKeys, int usedKeyLen);
extern void inpCmd_init(inputCommandList_t *inpCmdList);
extern void inpCmd_clearPressed();
extern void inpCmd_moveMouse(float x, float y);
extern void inpCmd_pressKey(char key);
extern qbool inpCmd_isEmpty(inputCommandList_t *inpCmdList);
extern int inpCmd_getLen(inputCommandList_t *inpCmdList);
extern qbool inpCmd_isFull(inputCommandList_t *inpCmdList);
extern inputCommand_t *inpCmd_getLast(inputCommandList_t *inpCmdList);
extern qbool inpCmd_isPressed(inputCommand_t *inpCmd, char key);
extern void inpCmd_addFromInput(inputCommandList_t *inpCmdList, int sequence);
extern inputCommand_t *inpCmd_add(inputCommandList_t *inpCmdList, int sequence);
extern void inpCmd_removeFirst(inputCommandList_t *inpCmd);
extern inputCommand_t *inpCmd_get(inputCommandList_t *inpCmd, int i);
extern void inpCmd_clear(inputCommandList_t *inpCmdList);
extern void inpCmd_free(inputCommandList_t *inpCmdList);


/********************SERVER********************/

typedef enum
{
    SYS_IDLE,
    SYS_CONNECT,
    SYS_AUTH,
    SYS_READY,
    SYS_RUN,
    SYS_DISCONNECTED
} sysState_e;

typedef enum
{
    SERVCMD_NULL,
    SERVCMD_STR,
    SERVCMD_SYS,
    SERVCMD_ENT,
    SERVCMD_INPUTACK,
    SERVCMD_END
} sv_netcmd_e;

typedef enum
{
    CLCMD_NULL,
    CLCMD_SYS,
    CLCMD_INPUT,
    CLCMD_ENTACK,
    CLCMD_END
} cl_netcmd_e;

// typedef struct cl_entStateRecord_st
// {
//     struct cl_entStateRecord_st *next;
//     struct cl_entStateRecord_st *prev;
    
//     int entID;

//     recentStreamRecord_t stateRecord;

// } cl_entStateRecord_t;

// typedef struct cl_entStateRecordList_st
// {
//     cl_entStateRecord_t *viewHead;
//     cl_entStateRecord_t *viewTail;
//     int viewEntLen;
    
//     cl_entStateRecord_t *permanentHead;
//     cl_entStateRecord_t *permanentTail;
//     int permEntLen;
    
//     int clientEntID;

//     quickStreamRecord_t newEntRecord;
    
// } cl_entStateRecordList_t;



typedef struct serv_clrep_st
{
    int conID;

    endTimer_t lastRecvTimer;
    endTimer_t sendTimer;

    sysState_e clState;

    netcon_t *con;

    // cl_entStateRecordList_t entStateRecordList;

    inputCommandList_t inputCommandList;

    // clientEntityRecordList_t clientEntRecordList;

    int worldSnapshotID;

} serv_clrep_t;


// typedef struct cl_inputList_st
// {
//     vector(inputCommandList_t) list;
//     vector(byte) bitmap;

// } cl_inputList_t;

typedef struct server_st
{
    vector(serv_clrep_t) clRepList;
    vector(byte) clRepBitMap;

    s2imap_t *clRepMap;

    int lastConID;

    endTimer_t sendTimer;

} server_t;

extern void serv_init();
extern void serv_frame();
extern void serv_packetEvent(netaddr_t *fromAddress, byte *data, int len);
// extern void serv_addSyncedEnt(int, int);
extern void serv_removeSyncedEnt(int entID, int entType);

/********************CLIENT********************/

typedef struct client_st
{
    serv_clrep_t clRep;

    netaddr_t servAddr;
    netaddr_t clAddr;

    // netcon_t servCon;

    // cl_entStateRecordList_t entStateRecordList;

    // sysState_e clState;

    // endTimer_t sendTimer;
    // endTimer_t lastRecvTimer;

    int conID;
    int conAttempts;

    // inputCommandList_t *inputCommandList;

} client_t;

extern void cl_init();
extern void cl_frame();
extern void cl_keyEvent(int key);
extern void cl_packetEvent(netaddr_t *fromAddress, byte *data, int len);
extern void cl_mouseEvent(float x, float y);

extern server_t server;
extern client_t client;
// extern cl_inputList_t cl_inputList;

extern void world_load();

extern void eng_processServerEntities();
extern void eng_processClientEntities();

struct EngineParameters_st {
    // SDL_FPoint cameraPos;
    float aspectRatio;
    float windowWidth;;
    float windowHeight;
    float screenFPS;
    float tickRate;
    float absoluteDeltaTime;
    float gameDeltaTime;
    float toWindowRatioX;
    float toWindowRatioY;
    float toWorldRatioX;
    float toWorldRatioY;
    Uint64 currentAbsoluteTick;
    Uint64 currentGameTick;
    double bulletTimeRate;
    bool isPaused;
    bool KEYPRESSED[256];
    SDL_Window *window;
    SDL_Renderer *renderer;
    // sqlite3 *database;
};

typedef struct EngineParameters_st EngineParameters;
extern EngineParameters engineParameters;

extern SDL_FRect cameraRect;
extern qbool isStateEmpty(recentStreamNode_t *node, int len);

// extern b2WorldId worldId;
extern cpSpace *worldId;

extern void sv_setup();
extern void entSys_setup();
extern void world_setup();

extern void cl_addInputCmd();

extern void cl_update();

extern void eng_setup();
#endif