#ifndef BASIC_H
#define BASIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/time.h>
#include "../lib/cJSON/cJSON.h"


#define MAX(a,b) (a)>(b)?(a):(b)
#define MIN(a,b) (a)<(b)?(a):(b)
// #define CEIL(a) ((a) - ((int)(a))) > 0 ? ((int)(a) + 1) : (a)
#define FRACT(a) ((a) - (int)(a))
#define CEIL(a) ((float)(int)((a) + (1 - FRACT(a))))
#define FLOOR(a) ((float)(int)(a))
#define ABS(a) (((a) < 0)? -(a) : (a))

typedef enum
{
    qfalse,
    qtrue
} qbool;

typedef unsigned char byte;

/********************BITMAP********************/

extern int bm_getByte(int i);
extern int bm_getBit(int i);
extern int bm_getByteVal(byte *arr, int i);
extern int bm_getBitVal(byte *arr, int i);
extern int bm_setBitVal(byte *arr, int i, byte b);
extern int bm_findEmpty(byte *arr, int size);

/********************BITSTREAM********************/

typedef struct bitstream_st {
    int bufsize;
    int datalen;
    char *buf;
    unsigned int curbyte;
    unsigned int curbit;
} bitstream_t;

extern void stream_init(bitstream_t *bs,char *buf, int size);
extern qbool stream_isWritten(bitstream_t *bs);
extern void stream_writeBit(bitstream_t* bs, int val);
extern void stream_writeByte(bitstream_t* bs, unsigned char b);
extern void stream_writeInt(bitstream_t* bs, unsigned int b);
extern void stream_writeLong(bitstream_t* bs, unsigned long int b);
extern void stream_writeDouble(bitstream_t* bs, long double f);
extern void stream_writeData(bitstream_t *bs, byte *data, int len);
extern void stream_writeString(bitstream_t *bs, char *data, int len);
extern void stream_writeLongBits(bitstream_t *bs, unsigned long int b, int bits);
extern void stream_writeVarLong(bitstream_t *bs, unsigned long int b);
extern void stream_writeBitsData(bitstream_t *bs, byte *data, int bitLen);
extern int stream_readBit(bitstream_t* bs);
extern unsigned char stream_readByte(bitstream_t* bs);
extern unsigned int stream_readInt(bitstream_t* bs);
extern unsigned int stream_readLong(bitstream_t* bs);
extern long double stream_readDouble(bitstream_t* bs);
extern void stream_readString(bitstream_t *bs, char *str, int *len);
extern unsigned long int stream_readLongBits(bitstream_t *bs, int bits);
extern unsigned long int stream_readVarLong(bitstream_t *bs);
extern void stream_copyBitsData(bitstream_t *bs, byte *data, int bitLen);
extern void stream_skipBits(bitstream_t *bs, int bitLen);

/********************NETWORK SYSTEM********************/

typedef struct netaddr_st
{
    byte ip[4];
    int port;
} netaddr_t;

extern qbool netAddrCmp(netaddr_t a, netaddr_t b);
extern char *net_errorString();
extern const char *netAddrToString(netaddr_t net);
extern void net_getNetAddr(netaddr_t *a);
extern int net_init(int port);
extern int net_sendPacket(netaddr_t *a, bitstream_t *msg);
extern int net_getPacket(netaddr_t *fromaddr, bitstream_t *msg);
extern void net_sleep(int msec);

/********************CONNECTION LAYER********************/

#define	MAX_MSGLEN 16384
#define MAX_SEGMENTLEN 4096
#define SENDWINDOW_SIZE 4
typedef enum
{
    NETCON_WAITTIME,
    NETCON_SLIDWIND_FULL,
    NETCON_FRAGMENT,
    NETCON_READY
} netcon_sendstate_e;

typedef enum
{
    NETCON_PACKET_SENT,
    NETCON_PACKET_FRAGMENTED,
    NETCON_PACKET_SUCCESS,
    NETCON_PACKET_DROPPED
} netcon_packetstate_e;

typedef struct netcon_st
{
    netaddr_t remoteAddress;
    int incomingSequence;
    int outgoingSequence;
    int lastAckSequence;

    int recvFragID;
    int recvFullFragLength;
    int recvFragSequence;
    int recvFragLength;
    byte recvFragBuffer[MAX_MSGLEN];
    netcon_sendstate_e recvState;

    int sendFragID;
    int sendFullFragLength;
    int sendFragSequence;
    int sendFragLength;
    byte sendFragBuffer[MAX_MSGLEN];
    netcon_sendstate_e sendState;

    int windowStartSequence;
    netcon_packetstate_e sentPacketStates[SENDWINDOW_SIZE];

} netcon_t;

extern void netcon_init();
extern void netcon_setup(netcon_t *con);
extern void netAddrSet(netaddr_t *a, int ip1, int ip2, int ip3, int ip4, int port);
extern netcon_packetstate_e netcon_getPacketState(netcon_t *con, int sequence);
extern qbool netcon_shouldSend(netcon_t *con);
extern int netcon_transmit(netcon_t * con, int length, byte *data);
extern int netcon_transmitFragment(netcon_t *con);
extern int netcon_process(netcon_t *con, bitstream_t *bs);
extern void printbit(char byte);

/********************EVENTS********************/

typedef enum
{
    SYSEVENT_KEY,
    SYSEVENT_PACKET
} sysEventType_e;

typedef struct sysEvent_st
{
    sysEventType_e type;
    int value;
    int value2;
    void *ptr;
} sysEvent_t;

extern qbool isSysEventEmpty();
extern void addSysEvent(sysEventType_e type, int value1, int value2, void *ptr);
extern sysEvent_t *getSysEvent();

/********************CVAR********************/

typedef struct cvar_st
{
    char *name;
    char *string;
    float floatval;
    int intval;
    int modifiedCount;
    int isModified;
    struct cvar_st *next;
    struct cvar_st *hashNext;
} cvar_t;

extern int cvar_getFloat(const char* name);
extern int cvar_getInt(const char* name);
extern char *cvar_getString(const char *name);
extern cvar_t *cvar_get(const char *name, char *value);
extern void cvar_init();


/********************SYSTEM********************/

extern void sys_exit(char *msg, ...);

/********************TIMER********************/

typedef struct endTimer_st {
    unsigned long int endTime;
    unsigned long int startTime;
    unsigned int duration;
} endTimer_t;

extern unsigned long int getTimeMillis();
extern void startTimer(endTimer_t *timer,unsigned int duration);
extern qbool checkTimer(endTimer_t *timer);
extern unsigned long int getTimeElapsed(endTimer_t *timer);

/********************PRINT********************/

typedef enum {
    ERR_FATAL
} errorType_t;

extern void com_printf(const char *msg, ...);
extern void com_error(errorType_t etype, const char *msg, ...);

/********************MEMORY ALLOCATION********************/

#define GENERALZONE 0
#define PERMANENTZONE 1
#define TEMPORARYZONE 2

extern void zonecheck(int zoneid);
extern int initMemzone(void *ptr, int size);
extern int createMemzone(int size);
extern void freeMemzone(int zoneid);
extern void *zidmalloc(int zoneid, int size);
extern void zidfree(void* ptr);
extern void* zidrealloc(void* ptr, int newSize);
extern void createThreeZones(int gensize, int permsize, int tempsize);
extern char *copyString(char *str, int len);
extern void zmemset(void* ptr, int val, int size);
extern void zmemcpy(void* dest, void *src, int n);

/********************INT TO INT HASH MAP********************/

#define HASH_ARRAYSIZE 127

typedef struct i2inode_st
{
    int key, val;
    struct i2inode_st *next;
} i2inode_t;

typedef struct i2imap_st
{
    int zoneid;
    i2inode_t *hashlist[HASH_ARRAYSIZE];
} i2imap_t;

extern i2imap_t *i2imap_init(int zoneid);
extern void i2imap_put(i2imap_t *imap, int key, int val);
extern int i2imap_contains(i2imap_t *imap, int key);
extern int i2imap_get(i2imap_t *imap, int key);
extern void i2imap_remove(i2imap_t *imap, int key);


/********************STRING TO INT HASH MAP********************/

typedef struct s2inode_st {
    char *key;
    int val;
} s2inode_t;

typedef struct s2imap_st {
    int zoneid;
    int length;
    int capacity;
    s2inode_t *list;
    int maxstrsize;
} s2imap_t;

extern int s2imap_get(s2imap_t *smap, const char *key);
extern s2imap_t *s2imap_create(int zoneid);
extern void s2imap_put(s2imap_t *smap, const char *key, int val);
extern void s2imap_remove(s2imap_t *smap, const char *key);

/********************GENERAL VECTOR********************/

#define vector(T) struct {int zoneid; int size; int capacity; T* arr;}

#define vecinit(z,v,t,c) { \
    (v).zoneid = z; \
    (v).size=0; \
    (v).capacity=0; \
    (v).arr=0; \
    (v).capacity = (c); \
    (v).arr = (t*)zidmalloc((v).zoneid, sizeof(t)*(c)); \
} \

#define vecpushempty(v, t) { \
    if((v).size == (v).capacity) { \
        (v).capacity *= 2; \
        (v).arr = (t*)zidrealloc((v).arr,sizeof(t) * (v).capacity ); \
    } \
    (v).size+=1; \
} \

#define vecpush(v, t, val) { \
    if((v).size == (v).capacity) { \
        (v).capacity *= 2; \
        (v).arr = (t*)zidrealloc((v).arr,sizeof(t) * (v).capacity ); \
    } \
    (v).arr[(v).size] = val; \
    (v).size+=1; \
} \

#define vecget(v,i) (v).arr[(i)]
#define vecset(v,i,val) (v).arr[(i)] = val
#define vecsize(v) (v).size
#define vecreset(v) (v).size = 0;

/********************CORE FUNCTIONS********************/

extern void eng_init();
extern void eng_runFrame();

/********************FILES********************/

extern char *getFileString(const char *filename, int zoneid);

extern void openLevelFile();
extern char *getLevelFileString();
extern cJSON *getLevelJSON();
extern void closeLevelFile();

extern float getScreenWidth();
extern float getScreenHeight();

#ifdef __cplusplus
}
#endif

#endif
