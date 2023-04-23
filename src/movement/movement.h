#include "../basic/world_def.h"
#include "../engine/entity.h"

typedef struct worldRect_st
{
    float rect[4];
} worldRect_t;

typedef struct world_st
{
    worldRect_t *worldWallArray;
    int worldWallSize;

} world_t;

extern void initPhysics();
extern world_t world;
extern entityMoveList_t newMoveList;

extern void physics_init();
extern void physics_run();
extern void physics_addBody(entityMove_t *entMove);