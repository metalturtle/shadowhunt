#ifndef SHADOWHUNT_STEALTH_H
#define SHADOWHUNT_STEALTH_H

#include "../basic/basic.h"
#include "../core/stealth_rules.h"
#include "engine.h"
#include "entity.h"

/* Tuning. Distances are world units, times are milliseconds. */
#define SH_ROUND_MS            150000
#define SH_RELEASE_MS          4000
#define SH_RESULTS_MS          5000
#define SH_LOBBY_MS            3000
#define SH_POWER_MS            7000
#define SH_FREEZE_MS           5000
#define SH_PELLET_RESPAWN_MS   20000
#define SH_HUNTER_SPEED        58.0f
#define SH_HIDER_SPEED         62.0f
#define SH_POWERED_SPEED       80.0f
#define SH_LANTERN_RADIUS      30.0f
#define SH_PICKUP_REACH        9.0f
#define SH_TAG_REACH           11.0f
#define SH_SHOT_DAMAGE         34
#define SH_HUNTER_AMMO         90
#define SH_MAX_MOVE_BUDGET     0.25f

/* Per-entity snapshot flags. */
#define SH_FLAG_EXPOSED    0x01
#define SH_FLAG_POWERED    0x02
#define SH_FLAG_FROZEN     0x04
#define SH_FLAG_HIDDEN     0x08  /* position withheld from this viewer */
#define SH_FLAG_SPECTATOR  0x10

#define SH_MAX_SHOTS 16
#define SH_SHOT_SEND_MS 400
#define SH_ENTITY_SNAPSHOT_INTS 9

typedef struct {
    int id;
    float x, y, endX, endY;
    unsigned long time;
} sh_shot_t;

typedef struct {
    /* Replicated round state. */
    int round;
    int winner;
    int hidersAlive;
    int hidersTotal;
    int huntersTotal;
    long roundRemainingMs;
    long releaseRemainingMs;
    bool pelletActive[SH_MAX_PELLETS];

    /* Server-only timing. */
    unsigned long roundEndAt;
    unsigned long lobbyEndAt;
    unsigned long releaseAt;
    unsigned long pelletRespawnAt[SH_MAX_PELLETS];

    /* Shots fired recently (server) or received recently (client). */
    sh_shot_t shots[SH_MAX_SHOTS];
    int nextShotID;
    int lastSeenShotID;

    /* Client bookkeeping. */
    int localHiddenCount;
    unsigned long lastUpdateAt;
} StealthGame;

/* Client: where prediction had the local player just before a snapshot
 * reset it, so the replayed result can be compared (misprediction). */
typedef struct {
    float beforeX, beforeY;
    bool pending;
} PredictionCheck;

extern StealthGame shGame;
extern PredictionCheck shPrediction;
extern bool shTestLogs;

/* Shared. */
extern PlayerData *stealth_player(VectorEntity *vecEnt);
extern bool stealth_isHunter(VectorEntity *vecEnt);
extern bool stealth_isAliveHider(VectorEntity *vecEnt);
extern bool stealth_isPowered(VectorEntity *vecEnt);
extern bool stealth_isFrozen(VectorEntity *vecEnt);
extern float stealth_moveSpeed(VectorEntity *vecEnt);
extern void stealth_init(void);

/* Server. */
extern void stealth_startRound(void);
extern void stealth_resetForWaiting(void);
extern int stealth_evaluateRound(void);
extern void stealth_serverTick(void);
extern void stealth_onPlayerJoined(VectorEntity *vecEnt);
extern bool stealth_canShoot(VectorEntity *vecEnt);
extern bool stealth_canBeShot(VectorEntity *vecEnt);
extern void stealth_applyShotHit(int fromEntID, int toEntID);
extern void stealth_recordShot(float x, float y, float endX, float endY);
extern void stealth_refillMoveBudgets(float elapsedSeconds);
extern void stealth_consumeMoveBudget(VectorEntity *vecEnt, inputCommand_t *inpCmd, float *deltaTime);
extern void stealth_writeSnapshotHeader(bitstream_t *bs);
extern void stealth_writeEntity(bitstream_t *bs, VectorEntity *vecEnt, int viewerConID);

/* Client. */
extern void stealth_readSnapshotHeader(bitstream_t *bs);
extern void stealth_readEntity(bitstream_t *bs, VectorEntity *vecEnt);
extern void stealth_skipEntity(bitstream_t *bs);
extern void stealth_finishSnapshot(void);
extern void stealth_interpolate(VectorEntity *vecEnt, unsigned long renderTime);
extern VectorEntity *stealth_localPlayer(void);
extern int stealth_localRole(void);

#endif
