#include "../core/stealth_rules.h"
// #include "../basic/world_def.h"
// #include "../engine/entity.h"

typedef struct worldRect_st
{
    float rect[4];
} worldRect_t;

typedef struct worldPoint_st
{
    float x, y;
} worldPoint_t;

typedef struct world_st
{
    worldRect_t *worldWallArray;
    int worldWallSize;

    /* Stealth layout: static lamps, role spawns and power pellet sites. */
    sh_light_t lights[SH_MAX_LIGHTS];
    int lightCount;
    worldPoint_t hunterSpawns[SH_MAX_SPAWNS];
    int hunterSpawnCount;
    worldPoint_t hiderSpawns[SH_MAX_SPAWNS];
    int hiderSpawnCount;
    worldPoint_t pellets[SH_MAX_PELLETS];
    int pelletCount;

} world_t;


// typedef struct moveList_st
// {
//     vector(entityMove_t) list;
//     vector(byte) bitmap;
// } moveList_t;


// extern void initPhysics();
extern world_t world;
// extern entityMoveList_t newMoveList;
// extern moveList_t moveList;

// extern void physics_init();
// extern void physics_run();
// extern int physics_addBody(entityMove_t *entMove);
// extern void physics_setBody(entityMove_t *moveObj, int moveID);
// extern entityMove_t* physics_get(int moveID);

// extern void physics_removeBody(int moveID);
