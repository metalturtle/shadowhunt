#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/time.h>
#include "basic.h"
#include<SDL3/SDL.h>
#include <math.h>

void sys_exit(char *fmt, ...)
{
   #define bufsize 256
    char buffer[bufsize];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, 256, fmt, args);
    va_end(args);

    printf("%s", buffer);
    #undef bufsize
    exit(1);
}

/********************TIMER********************/

unsigned long int getTimeMillis()
{
    struct timeval start;
    gettimeofday(&start, NULL);
    return start.tv_usec/1000 + start.tv_sec*1000;
}

void startTimer(endTimer_t *timer,unsigned int duration)
{
    timer->duration = duration;
    timer->startTime = getTimeMillis();
    timer->endTime = getTimeMillis() + duration;
}

qbool checkTimer(endTimer_t *timer)
{
    if(timer->endTime < getTimeMillis())
    {
        return qtrue;
    }
    return qfalse;
}

unsigned long int getTimeElapsed(endTimer_t *timer) 
{
    return getTimeMillis() - timer->startTime;
}

/********************PRINT********************/

void com_printf(const char *msg, ...)
{
    #define bufsize 256
    char buffer[bufsize];
    va_list args;
    va_start(args, msg);
    vsnprintf(buffer, 256, msg, args);
    va_end(args);

    printf("%s", buffer);
    #undef bufsize
}

void com_error(errorType_t etype, const char *msg, ...)
{
    #define bufsize 256
    va_list args;
    char buffer[bufsize];

    va_start(args, msg);
    vsnprintf(buffer, 256, msg, args);
    va_end(args);

    if(etype == ERR_FATAL)
    {
        sys_exit("%s", buffer);
    }
    #undef bufsize
}

/********************BITMAP********************/

int bm_getByte(int i)
{
    return i >> 3;
}

int bm_getBit(int i)
{
    return (1 << (i & 7));
}

int bm_getByteVal(byte *arr, int i)
{
    return arr[i >> 3];
}

int bm_getBitVal(byte *arr, int i)
{
    return (arr[i >> 3] & (1 << (i & 7))) ? 1 : 0;
}

void bm_setBitVal(byte *arr, int i, byte b)
{
    if(b) arr[i >> 3] |= bm_getBit(i);
    else arr[i >> 3] &= ~bm_getBit(i);
}

int bm_findEmpty(byte *arr, int size)
{
    for( int i = 0; i < size; i++)
    {
        if(!bm_getBitVal(arr, i))
        {
            return i;
        }
    }
    return -1;
}


/********************MEMORY ALLOCATION********************/

#define MEMBLOCK_USED 1
#define MEMBLOCK_FREE 0
#define MEM_MINFRAG 64
#define MEMCRASHTEST 23432423
// #define MEMDEBUG 1

int LAST_MEMZONEID = 0;

struct memblock {
    int zoneid;
    int tag;
    int size;
    struct memblock* prev, *next;
};

struct memzone {
    int id;
    int used;
    struct memblock blocklist;
    struct memblock* prev, *next;
    struct memblock* rover;
};

struct memzone* zoneList[32];

void zonecheck(int zoneid)
{
    struct memblock *block, * start;
    struct memzone *zone;

    zone = zoneList[zoneid];
    start = &zone->blocklist;
    block = start->next;
    int i = 0;

    com_printf("zonecheck: size=%d, used=%d,blocklist=%p, rover=%p \n",zone->blocklist.size, zone->used,&zone->blocklist,zone->rover);
    while(block != start) {
        com_printf("block %d: zoneid: %d, total size: %d, content size: %d, tag: %d, block=%p, prev=%p,next=%p\n",
        i,
        block->zoneid,
        block->size,
        (int)(block->size - sizeof(struct memblock) -4),
         block->tag
         ,block
         ,block->prev, block->next);
        block = block->next;
        i+=1;
    }
}

static void clearMemzone(struct memzone* zone)
{
    struct memblock* block;

    #ifdef MEMDEBUG
    printf("clearing memzone %d\n",zone->id);
    #endif
    zone->used = 0;
    block = (struct memblock*)((char*)zone + sizeof(struct memzone));
    block->zoneid = zone->id;
    block->tag = MEMBLOCK_FREE;
    block->prev = block->next = &zone->blocklist;
    block->size = zone->blocklist.size - sizeof(struct memzone);

    zone->blocklist.prev = zone->blocklist.next = zone->rover = block;
}

int initMemzone(void *ptr, int size)
{
    struct memzone* zone;
    zone = (struct memzone*) ptr;
    zone->blocklist.size = size;
    zone->blocklist.tag = MEMBLOCK_USED;
    zone->id = LAST_MEMZONEID++;
    zoneList[zone->id] = zone;
    clearMemzone(zone);
    
    #ifdef MEMDEBUG
    printf("created memzone, id=%d, size=%d\n",zone->id,zone->blocklist.size);
    #endif
    return zone->id;
}

int createMemzone(int size)
{
    void *ptr = malloc(size);
    return initMemzone(ptr, size);
}

void freeMemzone(int zoneid)
{
    free(zoneList[zoneid]);
    zoneList[zoneid] = NULL;
}

void* zidmalloc(int zoneid, int size)
{
    #ifdef MEMDEBUG
    printf("\nzonemalloc \n");
    #endif
    struct memzone* zone;
    struct memblock* base, *rover, *start, *newblock;
    int extra;

    zone = zoneList[zoneid];

    size += sizeof(struct memblock) + sizeof(int) + 3;
    size = (size & ~3);

    base = rover = zone->rover;
    start = rover->prev;
    
    do {
        if(rover == start) {
            // printf("Error: no free memory found\n");
            com_error(ERR_FATAL, "ERROR: no free memory found\n");
            return NULL;
        }
        if(rover->tag) {
            base = rover = rover->next;
        } else {
            rover = rover->next;
        }
    } while( base->tag || base->size < size);

    extra = base->size - size;
    #ifdef MEMDEBUG
    printf("size req %d, base size %d, extra space %d \n",size, base->size,extra);
    #endif
    if(extra > MEM_MINFRAG) {
        newblock = (struct memblock*)((char*)base + size);
        newblock->zoneid = zone->id;
        newblock->tag = MEMBLOCK_FREE;
        newblock->size = extra;
        newblock->prev = base;
        newblock->next = base->next;
        base->next->prev = newblock;
        base->next = newblock;
        base->size = size;
    }

    *((int*)((char*)base + base->size - sizeof(int))) = MEMCRASHTEST;
    base->tag = MEMBLOCK_USED;

    zone->used += base->size;
    zone->rover = base->next;

    return ((char*)base + sizeof(struct memblock));
}

void zidfree(void* ptr)
{
    #ifdef MEMDEBUG
    printf("\nzfree\n");
    #endif

    struct memzone* zone;
    struct memblock* block, *other;
    
    block = (struct memblock*)((char*)ptr - sizeof(struct memblock));
    zone = zoneList[block->zoneid];

    if (*((int*)((char*)block + block->size - sizeof(int))) != MEMCRASHTEST) {
        printf("Error: memory boundary has been crossed\n");
        zonecheck(zone->id);
        return;
    }

    block->tag = MEMBLOCK_FREE;
    zone->used -= block->size;

    other = block->prev;
    if(!other->tag) {
        #ifdef MEMDEBUG
        printf("prev is free\n");
        #endif
        other->next = block->next;
        block->next->prev = other;
        other->size += block->size;
        if(zone->rover == block) {
            zone->rover = other;
        }
        block = other;
    }

    other = block->next;
    if(!other->tag) {
        #ifdef MEMDEBUG
        printf("next is free %d %d %d\n",block->size, other->size, block->size + other->size);
        #endif
        block->next = other->next;
        other->next->prev = block;
        block->size += other->size;
        if(zone->rover == other) {
            zone->rover = block;
        }
    }
}

void* zidrealloc(void* ptr, int newSize)
{
    struct memblock* block;
    void* newptr;

    block = (struct memblock*)((char*)ptr - sizeof(struct memblock));
    int contentSize = block->size - sizeof(struct memblock) - sizeof(int);

    newptr = zidmalloc(block->zoneid, newSize);
    memcpy(newptr, ptr, MIN(contentSize,newSize));
    zidfree(ptr);
    return newptr;
}

void zmemset(void* ptr, int val, int size)
{
    memset(ptr,val,size);
}

void zmemcpy(void *dest, void *src, int n)
{
    memcpy(dest, src, n);
}

void createThreeZones(int gensize, int permsize, int tempsize)
{
    createMemzone(gensize);
    createMemzone(permsize);
    createMemzone(tempsize);
}

char* copyString(char *str, int len)
{
    char *ret;
    ret = (char *) zidmalloc(GENERALZONE, len);
    zmemcpy(ret, str, len);
    ret[len-1] = '\0';
    return ret;
}

/********************INT TO INT HASH MAP********************/

static int i2imap_hash(int key)
{
    return key%HASH_ARRAYSIZE;
}

i2imap_t *i2imap_init(int zoneid)
{
    i2imap_t *imap = (i2imap_t *)zidmalloc(zoneid, sizeof(i2imap_t));
    imap->zoneid = zoneid;
    for(int i = 0; i < 31; i++) {
        imap->hashlist[i] = NULL;
    }
    return imap;
}

void i2imap_put(i2imap_t *imap, int key, int val)
{
    int bucket = i2imap_hash(key);
    i2inode_t *node = imap->hashlist[bucket];

    if(node == NULL) {
        i2inode_t *newnode = (i2inode_t*)zidmalloc(imap->zoneid, sizeof(i2inode_t));
        newnode->key = key;
        newnode->val = val;
        newnode->next = NULL;
        imap->hashlist[bucket] = newnode;
        return; 
    }

    while(node->next) {
        if(node->key == key) {
            node->val = val;
            return;
        }
        node = node->next;
    }

    i2inode_t *newnode = (i2inode_t*)zidmalloc(imap->zoneid, sizeof(i2inode_t));
    newnode->key = key;
    newnode->val = val;
    newnode->next = NULL;
    node->next = newnode;
}

int i2imap_contains(i2imap_t *imap, int key)
{
    int bucket;
    i2inode_t *node;

    bucket = i2imap_hash(key);
    node = imap->hashlist[bucket];
    
    while(node) {
        if(node->key == key) {
            return 1;
        }
        node = node->next;
    }
    return 0;
}

int i2imap_get(i2imap_t *imap, int key)
{
    int bucket;
    i2inode_t *node;

    bucket = i2imap_hash(key);
    node = imap->hashlist[bucket];

    while(node) {
        if(node->key == key) {
            return node->val;
        }
        node = node->next;
    }

    return -1;
}

void i2imap_remove(i2imap_t *imap, int key)
{    
    int bucket;
    i2inode_t *node, *prev;

    bucket = i2imap_hash(key);
    node = imap->hashlist[bucket];

    if(!node) {
        printf("ERROR: bucket is empty \n");
        return;
    }
    if(node->key == key) {
        imap->hashlist[bucket] = node->next;
        zidfree(node);
        return;
    }

    prev = NULL;
    while(node) {
        if(node->key == key) {
            prev->next = node->next;
            zidfree(node);
            return;
        }
        prev = node;
        node = node->next;
    }
}

/********************STRING TO INT HASH MAP********************/

#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL

#define HASHS2I_LISTSIZE 32

static u_int64_t hash_key(const char *key)
{
    u_int64_t hash = FNV_OFFSET;
    for (const char *p = key; *p; p++) {
        hash ^= (u_int64_t)(unsigned char)(*p);
        hash *= FNV_PRIME;
    }
    return hash;
}

// static inline s2inode_t *s2imap_getnode(s2imap_t *smap, const char *key)
// {
//     u_int64_t hash = hash_key(key);
//     int index = (int)(hash & (u_int64_t)(smap->capacity - 1));
    
//     while(smap->list[index].key != NULL) {
//         if(strcmp(smap->list[index].key,key) == 0) {
//             return &smap->list[index];
//         }
//         index += 1;
//         if(index == smap->capacity) {
//             index = 0;
//         }
//     }
// }

int s2imap_get(s2imap_t *smap, const char *key)
{
    u_int64_t hash = hash_key(key);
    int index = (int)(hash & (u_int64_t)(smap->capacity - 1));
    
    while(smap->list[index].key != NULL) {
        if(strcmp(smap->list[index].key,key) == 0) {
            return smap->list[index].val;
        }
        index += 1;
        if(index == smap->capacity) {
            index = 0;
        }
    }
    return -1;
}

static void s2imap_setval(int zoneid, s2inode_t *list, const char *key, int val, int capacity, int *length)
{
    char *allockey;
    u_int64_t hash = hash_key(key);
    int index = (int)(hash & (u_int64_t)(capacity - 1));

    while(list[index].key != NULL) {
        if(strcmp(list[index].key,key) == 0) {
            list[index].val = val;
            return;
        }
        index += 1;
        if(index == capacity) {
            index = 0;
        }
    }
    allockey = (char*)zidmalloc(zoneid, strlen(key) + 1);
    strcpy(allockey, key);
    list[index].key = allockey;
    list[index].val = val;
    *length += 1;
}

static void s2imap_expand(s2imap_t *smap, int newCapacity)
{
    int i,length;
    s2inode_t *newList, *oldList;
    
    if(newCapacity < 0) {
        return;
    }

    newList = (s2inode_t*)zidmalloc(smap->zoneid, sizeof(s2inode_t)*newCapacity);
    zmemset(newList, 0, sizeof(s2inode_t)*newCapacity);

    oldList = smap->list;
    length = 0;

    for(i = 0; i < smap->capacity; i++) {
        if(oldList[i].key != NULL) {
            s2imap_setval(smap->zoneid, newList, oldList[i].key, oldList[i].val, newCapacity, &length);
            zidfree(oldList[i].key);
        }
    }
    
    zidfree(oldList);
    smap->list = newList;
    smap->capacity = newCapacity;
    smap->length = length;
}

s2imap_t *s2imap_create(int zoneid)
{
    s2imap_t *smap;
    smap = (s2imap_t*)zidmalloc(zoneid,sizeof(s2imap_t));
    smap->capacity = HASHS2I_LISTSIZE;
    smap->length = 0;
    smap->list = (s2inode_t*)zidmalloc(zoneid, sizeof(s2inode_t)*smap->capacity);
    zmemset(smap->list, 0, sizeof(s2inode_t)*smap->capacity);
    return smap;
}

void s2imap_put(s2imap_t *smap, const char *key, int val)
{
    s2imap_setval(smap->zoneid, smap->list, key, val, smap->capacity, &smap->length);

    if(smap->length > smap->capacity/2) {
        s2imap_expand(smap,smap->capacity*2);
    }
}

void s2imap_remove(s2imap_t *smap, const char *key)
{
    u_int64_t hash = hash_key(key);
    int index = (int)(hash & (u_int64_t)(smap->capacity - 1));

    while(smap->list[index].key != NULL) {
        if(strcmp(smap->list[index].key,key) == 0) {
            zidfree(smap->list[index].key);
            smap->list[index].key = NULL;
            smap->list[index].val = 0;
        }
        index += 1;
        if(index == smap->capacity) {
            index = 0;
        }
    }
}




float func_absFloat(float a)
{
    return ABS(a);
}



void vec_normalize(SDL_FPoint *point) {
    float length = sqrt(point->x * point->x + point->y * point->y);
    if(length > 0) {
        point->x /= length;
        point->y /= length;
    }
}

void vec_multiply(SDL_FPoint *point, float multiplier) {
    point->x *= multiplier;
    point->y *= multiplier;
}

void vec_subtract(SDL_FPoint *a, SDL_FPoint *b) {
    a->x -= b->x;
    a->y -= b->y;
}

void vec_add(SDL_FPoint *a, SDL_FPoint *b) {
    a->x += b->x;
    a->y += b->y;
}

float vec_length(SDL_FPoint *a) {
    return sqrt(a->x * a->x + a->y * a->y);
}

void vec_copy(SDL_FPoint *a, SDL_FPoint *b) {
    a->x = b->x;
    a->y = b->y;
}

void vec_copyRectPos(SDL_FPoint *a, SDL_FRect *rect) {
    a->x = rect->x;
    a->y = rect->y;
}

float vec_getAngle(SDL_FPoint *a) {
    return atan2(a->y, a->x);
}

void vec_getVecFromAngle(SDL_FPoint *a, float angle) {
    a->x = sin(angle);
    a->y = cos(angle);
}
