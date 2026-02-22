// #include "../basic/world_def.h"
// #include "../engine/entity.h"

typedef struct worldRect_st
{
    float rect[4];
} worldRect_t;

typedef struct world_st
{
    worldRect_t *worldWallArray;
    int worldWallSize;

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