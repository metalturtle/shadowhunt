#include "stealth.h"
#include "../basic/world_def.h"
#include "../movement/movement.h"
#include <unistd.h>

/* Development bots: a client started with SHADOWHUNT_BOT=1 plays by itself,
 * so one person can test a full match. Bots press the same keys a player
 * would and know only what their client knows: hunter bots never see hiders
 * the server withholds. */

static bool botEnabled = false;
static bool botAimActive = false;
static float botAimRadians = 0;

static float targetX, targetY;
static bool hasTarget;
static unsigned long nextDecisionAt, stuckCheckAt;
static float stuckX, stuckY;

bool bot_isActive(void)
{
    return botEnabled;
}

bool bot_aim(float *radians)
{
    if(!botAimActive)
        return false;
    *radians = botAimRadians;
    return true;
}

void bot_init(void)
{
    botEnabled = getenv("SHADOWHUNT_BOT") != NULL && !cvar_getInt("isServer");
    if(botEnabled) {
        srand((unsigned int)(getTimeMillis() ^ (unsigned long)getpid()));
        printf("bot: enabled\n");
    }
}

static float distanceSquared(float ax, float ay, float bx, float by)
{
    return (ax - bx) * (ax - bx) + (ay - by) * (ay - by);
}

static void pickWanderTarget(int role)
{
    /* Hiders drift between hider spawns (placed in the dark); hunters sweep
     * between lamps, where hiders are exposed. */
    if(role == SH_ROLE_HUNTER && world.lightCount > 0) {
        sh_light_t *light = &world.lights[rand() % world.lightCount];
        targetX = light->x;
        targetY = light->y;
    } else if(world.hiderSpawnCount > 0) {
        worldPoint_t *spot = &world.hiderSpawns[rand() % world.hiderSpawnCount];
        targetX = spot->x;
        targetY = spot->y;
    } else {
        targetX = 50 + rand() % 400;
        targetY = 280 + rand() % 150;
    }
    hasTarget = true;
}

static VectorEntity *nearestOther(VectorEntity *self, int role, bool visibleOnly)
{
    VectorEntity *best = NULL;
    float bestDistance = 0;
    for(int i = 0; i < VECTOR_ENTITY_COUNT; i++) {
        VectorEntity *other = &vectorEntityList[i];
        PlayerData *data = stealth_player(other);
        if(other == self || data == NULL || data->role != role || data->spectating)
            continue;
        if(other->health <= 0 || (visibleOnly && data->hiddenFromViewer))
            continue;
        float d = distanceSquared(self->pos.x, self->pos.y, other->pos.x, other->pos.y);
        if(best == NULL || d < bestDistance) {
            best = other;
            bestDistance = d;
        }
    }
    return best;
}

static void steerToward(VectorEntity *self, float x, float y)
{
    float dx = x - self->pos.x, dy = y - self->pos.y;
    const float deadzone = 3.0f;
    engineParameters.KEYPRESSED['d'] = dx > deadzone;
    engineParameters.KEYPRESSED['a'] = dx < -deadzone;
    engineParameters.KEYPRESSED['s'] = dy > deadzone;
    engineParameters.KEYPRESSED['w'] = dy < -deadzone;
}

static void releaseKeys(void)
{
    engineParameters.KEYPRESSED['w'] = engineParameters.KEYPRESSED['a'] = false;
    engineParameters.KEYPRESSED['s'] = engineParameters.KEYPRESSED['d'] = false;
    engineParameters.KEYPRESSED['t'] = false;
}

void bot_update(void)
{
    if(!botEnabled)
        return;

    VectorEntity *self = stealth_localPlayer();
    PlayerData *me = self != NULL ? stealth_player(self) : NULL;
    unsigned long now = getTimeMillis();
    releaseKeys();
    botAimActive = false;
    if(self == NULL || me == NULL || me->spectating || self->health <= 0 ||
       MATCH_STATE != MATCH_RUNNING)
        return;

    int role = me->role;
    bool powered = me->powerUntil > now;

    /* Re-plan periodically, and whenever progress stalls against a wall. */
    if(now >= stuckCheckAt) {
        if(hasTarget && distanceSquared(self->pos.x, self->pos.y, stuckX, stuckY) < 4.0f)
            hasTarget = false;
        stuckX = self->pos.x;
        stuckY = self->pos.y;
        stuckCheckAt = now + 1200;
    }
    if(!hasTarget || now >= nextDecisionAt ||
       distanceSquared(self->pos.x, self->pos.y, targetX, targetY) < 25.0f) {
        pickWanderTarget(role);
        nextDecisionAt = now + 3000 + rand() % 3000;
    }

    float goalX = targetX, goalY = targetY;
    if(role == SH_ROLE_HIDER) {
        VectorEntity *hunter = nearestOther(self, SH_ROLE_HUNTER, false);
        if(powered && hunter != NULL) {
            goalX = hunter->pos.x;              /* red hot: hunt the hunter */
            goalY = hunter->pos.y;
        } else {
            for(int i = 0; i < world.pelletCount; i++) {
                if(!shGame.pelletActive[i])
                    continue;
                if(distanceSquared(self->pos.x, self->pos.y,
                                   world.pellets[i].x, world.pellets[i].y) < 140.0f * 140.0f) {
                    goalX = world.pellets[i].x;
                    goalY = world.pellets[i].y;
                    break;
                }
            }
        }
    } else if(role == SH_ROLE_HUNTER) {
        VectorEntity *prey = nearestOther(self, SH_ROLE_HIDER, true);
        if(prey != NULL &&
           distanceSquared(self->pos.x, self->pos.y, prey->pos.x, prey->pos.y) < 160.0f * 160.0f) {
            PlayerData *preyData = stealth_player(prey);
            float dx = prey->pos.x - self->pos.x, dy = prey->pos.y - self->pos.y;
            botAimRadians = atan2f(dy, dx);
            botAimActive = true;
            if(preyData->powerUntil > now) {
                /* Red-hot hiders are bulletproof: back away instead. */
                goalX = self->pos.x - dx;
                goalY = self->pos.y - dy;
            } else {
                engineParameters.KEYPRESSED['t'] = true;
                goalX = prey->pos.x;
                goalY = prey->pos.y;
            }
        } else if(rand() % 90 == 0) {
            /* Occasional blind shot into the dark. */
            botAimRadians = (rand() % 628) / 100.0f;
            botAimActive = true;
            engineParameters.KEYPRESSED['t'] = true;
        }
    }

    if(!botAimActive) {
        botAimRadians = atan2f(goalY - self->pos.y, goalX - self->pos.x);
        botAimActive = true;
    }
    steerToward(self, goalX, goalY);
}
