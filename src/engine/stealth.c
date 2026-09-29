#include "stealth.h"
#include "../basic/world_def.h"
#include "../movement/movement.h"
#include "../basic/cJSON.h"
#include <sys/stat.h>

/* Hunters versus hiders.
 *
 * The server owns roles, health, pellets, red-hot power, freezes and the
 * round clock. Hiders standing in shadow are withheld from hunters' snapshots
 * entirely, so the darkness mask is enforced by the server rather than only
 * drawn by the client. */

StealthGame shGame;
sh_tuning_t shTuning;
PredictionCheck shPrediction;
bool shTestLogs;

extern i2imap_t *mainEntMap;

static bool isServerProcess(void)
{
    return cvar_getInt("isServer") != 0;
}

/********************SHARED********************/

/********************TUNING AND HOT RELOAD********************/

extern char *findAssetPath(const char *relative);

void stealth_setDefaultTuning(sh_tuning_t *tuning)
{
    tuning->roundMs = 150000;
    tuning->releaseMs = 4000;
    tuning->resultsMs = 5000;
    tuning->lobbyMs = 3000;
    tuning->powerMs = 7000;
    tuning->freezeMs = 5000;
    tuning->pelletRespawnMs = 20000;
    tuning->hunterSpeed = 58.0f;
    tuning->hiderSpeed = 62.0f;
    tuning->poweredSpeed = 80.0f;
    tuning->lanternRadius = 30.0f;
    tuning->pickupReach = 9.0f;
    tuning->tagReach = 11.0f;
    tuning->shotDamage = 34;
    tuning->hunterAmmo = 90;
    tuning->shotIntervalMs = 160;
    tuning->maxMoveBudget = 0.25f;
}

static char *tuningPath(void)
{
    const char *override = getenv("SHADOWHUNT_TUNING");
    return findAssetPath(override != NULL && override[0] != '\0' ?
                         override : "levels/config/tuning.json");
}

static void readNumber(cJSON *json, const char *key, float *out, float minimum, float maximum)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(json, key);
    if(item == NULL)
        return;
    if(!cJSON_IsNumber(item)) {
        printf("tuning: %s must be a number; keeping %g\n", key, *out);
        return;
    }
    float value = (float)cJSON_GetNumberValue(item);
    if(value < minimum || value > maximum) {
        printf("tuning: %s=%g outside %g..%g; keeping %g\n", key, value, minimum, maximum, *out);
        return;
    }
    *out = value;
}

static void readInteger(cJSON *json, const char *key, int *out, int minimum, int maximum)
{
    float value = (float)*out;
    readNumber(json, key, &value, (float)minimum, (float)maximum);
    *out = (int)value;
}

static void applyTuning(void)
{
    rayWeaponHandle.weaponType.nextShootDelay = shTuning.shotIntervalMs;
}

bool stealth_loadTuning(const char *path)
{
    char *text = getFileString(path, TEMPORARYZONE);
    if(text == NULL)
        return false;
    cJSON *json = cJSON_Parse(text);
    zidfree(text);
    if(json == NULL || !cJSON_IsObject(json)) {
        printf("tuning: %s is not valid JSON; keeping previous values\n", path);
        cJSON_Delete(json);
        return false;
    }

    /* Validate into a copy so one bad value never leaves half a load. */
    sh_tuning_t next = shTuning;
    float roundSeconds = next.roundMs / 1000.0f;
    readNumber(json, "round_seconds", &roundSeconds, 1, 3600);
    next.roundMs = (int)(roundSeconds * 1000.0f);
    readInteger(json, "release_ms", &next.releaseMs, 0, 60000);
    readInteger(json, "results_ms", &next.resultsMs, 0, 60000);
    readInteger(json, "lobby_ms", &next.lobbyMs, 0, 60000);
    readInteger(json, "power_ms", &next.powerMs, 0, 60000);
    readInteger(json, "freeze_ms", &next.freezeMs, 0, 60000);
    readInteger(json, "pellet_respawn_ms", &next.pelletRespawnMs, 0, 600000);
    readNumber(json, "hunter_speed", &next.hunterSpeed, 1, 400);
    readNumber(json, "hider_speed", &next.hiderSpeed, 1, 400);
    readNumber(json, "powered_speed", &next.poweredSpeed, 1, 400);
    readNumber(json, "lantern_radius", &next.lanternRadius, 0, 200);
    readNumber(json, "pickup_reach", &next.pickupReach, 1, 100);
    readNumber(json, "tag_reach", &next.tagReach, 1, 100);
    readInteger(json, "shot_damage", &next.shotDamage, 1, 1000);
    readInteger(json, "hunter_ammo", &next.hunterAmmo, 0, 10000);
    readInteger(json, "shot_interval_ms", &next.shotIntervalMs, 20, 5000);
    cJSON_Delete(json);

    shTuning = next;
    applyTuning();
    return true;
}

typedef struct {
    time_t mtime;
    long size;
} fileStamp_t;

static bool fileChanged(const char *path, fileStamp_t *stamp)
{
    struct stat info;
    if(path == NULL || stat(path, &info) != 0)
        return false;
    bool changed = info.st_mtime != stamp->mtime || (long)info.st_size != stamp->size;
    stamp->mtime = info.st_mtime;
    stamp->size = (long)info.st_size;
    return changed;
}

/* Poll twice a second: cheap, and editors that save by rename still work. */
void stealth_pollHotReload(void)
{
    static fileStamp_t tuningStamp, levelStamp;
    static unsigned long nextPoll;
    static bool primed;
    unsigned long now = getTimeMillis();
    if(getenv("SHADOWHUNT_NO_HOT_RELOAD") != NULL || now < nextPoll)
        return;
    nextPoll = now + 500;

    char *tuningFile = tuningPath();
    char *levelFile = world_levelPath();
    bool tuningChanged = fileChanged(tuningFile, &tuningStamp);
    bool levelChanged = fileChanged(levelFile, &levelStamp);

    /* Clients receive tuning from the server, so only the server reloads it. */
    if(primed && tuningChanged && cvar_getInt("isServer") && stealth_loadTuning(tuningFile))
        printf("hot reload: tuning from %s\n", tuningFile);
    if(primed && levelChanged && world_reloadLayout()) {
        /* Newly added pellet sites start active; existing ones keep state. */
        for(int i = 0; i < world.pelletCount; i++) {
            if(shGame.pelletRespawnAt[i] == 0)
                shGame.pelletActive[i] = true;
        }
        printf("hot reload: level layout from %s\n", levelFile);
    }
    primed = true;
    SDL_free(tuningFile);
    SDL_free(levelFile);
}

void stealth_init(void)
{
    memset(&shGame, 0, sizeof(shGame));
    shGame.lastSeenShotID = -1;
    shTestLogs = getenv("SHADOWHUNT_TEST_LOGS") != NULL;

    stealth_setDefaultTuning(&shTuning);
    char *path = tuningPath();
    if(stealth_loadTuning(path))
        printf("tuning: loaded %s\n", path);
    else
        printf("tuning: %s not found, using defaults\n", path);
    SDL_free(path);
    applyTuning();
}

PlayerData *stealth_player(VectorEntity *vecEnt)
{
    /* Unused entity slots keep a stale (often zero) externalID; mapping them
     * would alias player 0's state and clobber it every tick. */
    if(vecEnt == NULL || !vecEnt->active ||
       vecEnt->externalID < 0 || vecEnt->externalID >= 8)
        return NULL;
    return &playerDataList[vecEnt->externalID];
}

bool stealth_isHunter(VectorEntity *vecEnt)
{
    PlayerData *player = stealth_player(vecEnt);
    return vecEnt->active && player != NULL && player->role == SH_ROLE_HUNTER;
}

bool stealth_isAliveHider(VectorEntity *vecEnt)
{
    PlayerData *player = stealth_player(vecEnt);
    return vecEnt->active && player != NULL && player->role == SH_ROLE_HIDER &&
           !player->spectating && vecEnt->health > 0;
}

bool stealth_isPowered(VectorEntity *vecEnt)
{
    PlayerData *player = stealth_player(vecEnt);
    return player != NULL && player->powerUntil > getTimeMillis();
}

bool stealth_isFrozen(VectorEntity *vecEnt)
{
    PlayerData *player = stealth_player(vecEnt);
    return player != NULL && player->frozenUntil > getTimeMillis();
}

float stealth_moveSpeed(VectorEntity *vecEnt)
{
    PlayerData *player = stealth_player(vecEnt);
    if(player == NULL || player->spectating || vecEnt->health <= 0)
        return 0;
    if(stealth_isFrozen(vecEnt))
        return 0;
    if(player->role == SH_ROLE_HUNTER)
        return SH_HUNTER_SPEED;
    if(stealth_isPowered(vecEnt))
        return SH_POWERED_SPEED;
    return SH_HIDER_SPEED;
}

/********************SERVER: ROUND FLOW********************/

typedef struct {
    int conID;
    VectorEntity *vecEnt;
} connectedPlayer_t;

static int collectPlayers(connectedPlayer_t *out, int max)
{
    int count = 0;
    for(int conID = 0; conID < server.clRepList.size && count < max; conID++) {
        if(!bm_getBitVal(server.clRepBitMap.arr, conID))
            continue;
        int entID = i2imap_get(mainEntMap, conID);
        if(entID < 0 || entID >= VECTOR_ENTITY_COUNT || !vectorEntityList[entID].active)
            continue;
        out[count].conID = conID;
        out[count].vecEnt = &vectorEntityList[entID];
        count++;
    }
    return count;
}

static void placeAt(VectorEntity *vecEnt, worldPoint_t point)
{
    vecEnt->pos.x = point.x;
    vecEnt->pos.y = point.y;
    vecEnt->dir.x = vecEnt->dir.y = 0;
    vecEnt->animSprite.pos[0] = point.x;
    vecEnt->animSprite.pos[1] = point.y;
}

static const char *roleName(int role)
{
    return role == SH_ROLE_HUNTER ? "hunter" : role == SH_ROLE_HIDER ? "hider" : "none";
}

static long roundDurationMs(void)
{
    const char *override = getenv("SHADOWHUNT_ROUND_SECONDS");
    if(override != NULL && atoi(override) > 0)
        return atoi(override) * 1000L;
    return SH_ROUND_MS;
}

void stealth_startRound(void)
{
    connectedPlayer_t players[VECTOR_ENTITY_COUNT];
    int count = collectPlayers(players, VECTOR_ENTITY_COUNT);
    int hunters = sh_hunter_count(count);
    unsigned long now = getTimeMillis();
    int hunterIndex = 0, hiderIndex = 0;

    shGame.round++;
    shGame.winner = SH_WINNER_NONE;
    shGame.releaseAt = now + SH_RELEASE_MS;
    shGame.roundEndAt = shGame.releaseAt + roundDurationMs();
    for(int i = 0; i < world.pelletCount; i++) {
        shGame.pelletActive[i] = true;
        shGame.pelletRespawnAt[i] = 0;
    }

    for(int i = 0; i < count; i++) {
        VectorEntity *vecEnt = players[i].vecEnt;
        PlayerData *player = stealth_player(vecEnt);
        if(player == NULL)
            continue;

        bool hunter = sh_is_hunter_slot(shGame.round - 1, i, count, hunters);
        player->role = hunter ? SH_ROLE_HUNTER : SH_ROLE_HIDER;
        player->spectating = false;
        player->exposed = false;
        player->powerUntil = 0;
        player->tags = 0;
        player->kills = 0;
        player->moveBudget = 0;
        player->weaponShot = 0;
        player->weaponOnHand.ammoCount = hunter ? SH_HUNTER_AMMO : 0;
        vecEnt->health = 100;

        if(hunter) {
            /* Hunters wait at their spawn while hiders scatter. */
            player->frozenUntil = shGame.releaseAt;
            placeAt(vecEnt, world.hunterSpawns[hunterIndex++ % world.hunterSpawnCount]);
        } else {
            player->frozenUntil = 0;
            int spawn = (hiderIndex++ + shGame.round * 3) % world.hiderSpawnCount;
            placeAt(vecEnt, world.hiderSpawns[spawn]);
        }
        printf("round %d: conid=%d entity=%d role=%s at %.0f,%.0f\n", shGame.round,
               players[i].conID, vecEnt->entID, roleName(player->role),
               vecEnt->pos.x, vecEnt->pos.y);
    }
}

void stealth_resetForWaiting(void)
{
    connectedPlayer_t players[VECTOR_ENTITY_COUNT];
    int count = collectPlayers(players, VECTOR_ENTITY_COUNT);
    shGame.winner = SH_WINNER_NONE;
    for(int i = 0; i < count; i++) {
        PlayerData *player = stealth_player(players[i].vecEnt);
        if(player == NULL)
            continue;
        player->role = SH_ROLE_NONE;
        player->spectating = false;
        player->powerUntil = 0;
        player->frozenUntil = 0;
        players[i].vecEnt->health = 100;
    }
}

void stealth_onPlayerJoined(VectorEntity *vecEnt)
{
    PlayerData *player = stealth_player(vecEnt);
    if(player == NULL)
        return;

    player->role = SH_ROLE_NONE;
    player->spectating = MATCH_STATE == MATCH_RUNNING;
    player->exposed = false;
    player->powerUntil = 0;
    player->frozenUntil = 0;
    player->moveBudget = 0;
    player->tags = 0;
    player->kills = 0;
    player->weaponOnHand.ammoCount = 0;
    if(world.hiderSpawnCount > 0)
        placeAt(vecEnt, world.hiderSpawns[vecEnt->externalID % world.hiderSpawnCount]);
    if(player->spectating)
        printf("late join: entity %d watches until the next round\n", vecEnt->entID);
}

int stealth_evaluateRound(void)
{
    int hunters = 0, hiders = 0, alive = 0;
    for(int i = 0; i < VECTOR_ENTITY_COUNT; i++) {
        VectorEntity *vecEnt = &vectorEntityList[i];
        PlayerData *player = stealth_player(vecEnt);
        if(!vecEnt->active || player == NULL || player->spectating)
            continue;
        if(player->role == SH_ROLE_HUNTER)
            hunters++;
        else if(player->role == SH_ROLE_HIDER) {
            hiders++;
            if(vecEnt->health > 0)
                alive++;
        }
    }

    long remaining = (long)shGame.roundEndAt - (long)getTimeMillis();
    shGame.huntersTotal = hunters;
    shGame.hidersTotal = hiders;
    shGame.hidersAlive = alive;
    shGame.winner = sh_round_outcome(hunters, hiders, alive, remaining);
    return shGame.winner;
}

/********************SERVER: PER-TICK RULES********************/

static void freezeHunter(VectorEntity *hunter, VectorEntity *hider, unsigned long now)
{
    PlayerData *hunterData = stealth_player(hunter);
    PlayerData *hiderData = stealth_player(hider);
    static int respawnCursor;

    hunterData->frozenUntil = now + SH_FREEZE_MS;
    placeAt(hunter, world.hunterSpawns[respawnCursor++ % world.hunterSpawnCount]);
    hiderData->tags++;
    printf("hunter tagged: hider entity %d burned hunter entity %d\n",
           hider->entID, hunter->entID);
}

void stealth_serverTick(void)
{
    unsigned long now = getTimeMillis();
    float hunterXY[VECTOR_ENTITY_COUNT * 2];
    int hunterCount = 0;

    for(int i = 0; i < world.pelletCount; i++) {
        if(!shGame.pelletActive[i] && now >= shGame.pelletRespawnAt[i])
            shGame.pelletActive[i] = true;
    }

    for(int i = 0; i < VECTOR_ENTITY_COUNT; i++) {
        VectorEntity *vecEnt = &vectorEntityList[i];
        if(stealth_isHunter(vecEnt)) {
            hunterXY[hunterCount * 2] = vecEnt->pos.x;
            hunterXY[hunterCount * 2 + 1] = vecEnt->pos.y;
            hunterCount++;
        }
    }

    bool running = MATCH_STATE == MATCH_RUNNING;
    for(int i = 0; i < VECTOR_ENTITY_COUNT; i++) {
        VectorEntity *hider = &vectorEntityList[i];
        PlayerData *hiderData = stealth_player(hider);
        if(!stealth_isAliveHider(hider)) {
            if(hiderData != NULL)
                hiderData->exposed = hider->active && hider->health <= 0;
            continue;
        }

        if(running && !stealth_isPowered(hider)) {
            for(int p = 0; p < world.pelletCount; p++) {
                if(!shGame.pelletActive[p] ||
                   !sh_touching(hider->pos.x, hider->pos.y, world.pellets[p].x,
                                world.pellets[p].y, SH_PICKUP_REACH))
                    continue;
                shGame.pelletActive[p] = false;
                shGame.pelletRespawnAt[p] = now + SH_PELLET_RESPAWN_MS;
                hiderData->powerUntil = now + SH_POWER_MS;
                printf("pellet taken: hider entity %d is red hot\n", hider->entID);
                break;
            }
        }

        bool powered = stealth_isPowered(hider);
        if(running && powered) {
            for(int h = 0; h < VECTOR_ENTITY_COUNT; h++) {
                VectorEntity *hunter = &vectorEntityList[h];
                if(!stealth_isHunter(hunter) || stealth_isFrozen(hunter))
                    continue;
                if(sh_touching(hider->pos.x, hider->pos.y, hunter->pos.x,
                               hunter->pos.y, SH_TAG_REACH))
                    freezeHunter(hunter, hider, now);
            }
        }

        hiderData->exposed = sh_hider_exposed(hider->pos.x, hider->pos.y, powered,
                                              world.lights, world.lightCount,
                                              hunterXY, hunterCount,
                                              SH_LANTERN_RADIUS);
    }

    long remaining = (long)shGame.roundEndAt - (long)now;
    long release = (long)shGame.releaseAt - (long)now;
    /* While waiting, the round clock field carries the lobby countdown. */
    long lobby = shGame.lobbyEndAt ? (long)shGame.lobbyEndAt - (long)now : 0;
    shGame.roundRemainingMs = running ? MAX(0, remaining) : MAX(0, lobby);
    shGame.releaseRemainingMs = running ? MAX(0, release) : 0;
}

void stealth_consumeMoveBudget(VectorEntity *vecEnt, inputCommand_t *inpCmd, float *deltaTime)
{
    /* A client may not simulate more time than has actually passed, no
     * matter how many input commands it sends. */
    PlayerData *player = stealth_player(vecEnt);
    if(player == NULL) {
        *deltaTime = 0;
        return;
    }
    float granted = MIN(*deltaTime, player->moveBudget);
    if(granted < 0)
        granted = 0;
    player->moveBudget -= granted;
    *deltaTime = granted;
}

void stealth_refillMoveBudgets(float elapsedSeconds)
{
    for(int i = 0; i < 8; i++) {
        PlayerData *player = &playerDataList[i];
        if(!player->active)
            continue;
        player->moveBudget = MIN(SH_MAX_MOVE_BUDGET, player->moveBudget + elapsedSeconds);
    }
}

bool stealth_canShoot(VectorEntity *vecEnt)
{
    PlayerData *player = stealth_player(vecEnt);
    return MATCH_STATE == MATCH_RUNNING && player != NULL &&
           player->role == SH_ROLE_HUNTER && !player->spectating &&
           !stealth_isFrozen(vecEnt);
}

bool stealth_canBeShot(VectorEntity *vecEnt)
{
    /* Bullets pass through hunters and corpses; red-hot hiders absorb them. */
    return stealth_isAliveHider(vecEnt);
}

void stealth_applyShotHit(int fromEntID, int toEntID)
{
    if(toEntID < 0 || toEntID >= VECTOR_ENTITY_COUNT)
        return;
    VectorEntity *target = &vectorEntityList[toEntID];
    if(!stealth_isAliveHider(target) || MATCH_STATE != MATCH_RUNNING)
        return;
    if(stealth_isPowered(target)) {
        printf("shot absorbed: hider entity %d is red hot\n", toEntID);
        return;
    }

    target->health -= SH_SHOT_DAMAGE;
    if(target->health <= 0) {
        target->health = 0;
        PlayerData *shooter = fromEntID >= 0 && fromEntID < VECTOR_ENTITY_COUNT ?
            stealth_player(&vectorEntityList[fromEntID]) : NULL;
        if(shooter != NULL)
            shooter->kills++;
        printf("hider eliminated: entity %d by hunter entity %d\n", toEntID, fromEntID);
    }
}

void stealth_recordShot(float x, float y, float endX, float endY)
{
    sh_shot_t *shot = &shGame.shots[shGame.nextShotID % SH_MAX_SHOTS];
    shot->id = shGame.nextShotID++;
    shot->x = x;
    shot->y = y;
    shot->endX = endX;
    shot->endY = endY;
    shot->time = getTimeMillis();
}

/********************SNAPSHOT WRITE********************/

static void writeInt(bitstream_t *bs, int value)
{
    stream_writeInt(bs, (unsigned int)value);
}

static int readInt(bitstream_t *bs)
{
    return (int)stream_readInt(bs);
}

void stealth_writeSnapshotHeader(bitstream_t *bs)
{
    unsigned long now = getTimeMillis();
    int pelletMask = 0;
    for(int i = 0; i < world.pelletCount && i < 31; i++) {
        if(shGame.pelletActive[i])
            pelletMask |= 1 << i;
    }

    writeInt(bs, shGame.round);
    writeInt(bs, shGame.winner);
    writeInt(bs, (int)shGame.roundRemainingMs);
    writeInt(bs, (int)shGame.releaseRemainingMs);
    writeInt(bs, shGame.hidersAlive);
    writeInt(bs, shGame.hidersTotal);
    writeInt(bs, shGame.huntersTotal);
    writeInt(bs, pelletMask);
    /* Tuning the client needs for prediction and presentation. */
    writeInt(bs, (int)(shTuning.hunterSpeed * 100));
    writeInt(bs, (int)(shTuning.hiderSpeed * 100));
    writeInt(bs, (int)(shTuning.poweredSpeed * 100));
    writeInt(bs, (int)(shTuning.lanternRadius * 100));
    writeInt(bs, shTuning.powerMs);
    writeInt(bs, shTuning.freezeMs);

    int shotCount = 0;
    for(int i = 0; i < SH_MAX_SHOTS; i++) {
        sh_shot_t *shot = &shGame.shots[i];
        if(shot->time != 0 && now - shot->time <= SH_SHOT_SEND_MS)
            shotCount++;
    }
    writeInt(bs, shotCount);
    for(int i = 0; i < SH_MAX_SHOTS; i++) {
        sh_shot_t *shot = &shGame.shots[i];
        if(shot->time == 0 || now - shot->time > SH_SHOT_SEND_MS)
            continue;
        writeInt(bs, shot->id);
        writeInt(bs, (int)(shot->x * 100));
        writeInt(bs, (int)(shot->y * 100));
        writeInt(bs, (int)(shot->endX * 100));
        writeInt(bs, (int)(shot->endY * 100));
    }
}

static int viewerRole(int viewerConID)
{
    int entID = i2imap_get(mainEntMap, viewerConID);
    if(entID < 0 || entID >= VECTOR_ENTITY_COUNT)
        return SH_ROLE_NONE;
    PlayerData *viewer = stealth_player(&vectorEntityList[entID]);
    if(viewer == NULL || viewer->spectating)
        return SH_ROLE_NONE;
    return viewer->role;
}

void stealth_writeEntity(bitstream_t *bs, VectorEntity *vecEnt, int viewerConID)
{
    PlayerData *player = stealth_player(vecEnt);
    unsigned long now = getTimeMillis();
    int flags = 0;
    int powerMs = 0, frozenMs = 0;

    if(player != NULL) {
        if(player->exposed) flags |= SH_FLAG_EXPOSED;
        if(player->powerUntil > now) {
            flags |= SH_FLAG_POWERED;
            powerMs = (int)(player->powerUntil - now);
        }
        if(player->frozenUntil > now) {
            flags |= SH_FLAG_FROZEN;
            frozenMs = (int)(player->frozenUntil - now);
        }
        if(player->spectating) flags |= SH_FLAG_SPECTATOR;
    }

    bool hidden = player != NULL && player->role == SH_ROLE_HIDER &&
                  !player->spectating && !player->exposed &&
                  MATCH_STATE == MATCH_RUNNING &&
                  viewerRole(viewerConID) == SH_ROLE_HUNTER;
    if(hidden)
        flags |= SH_FLAG_HIDDEN;

    writeInt(bs, hidden ? 0 : (int)(vecEnt->pos.x * 1000.0f));
    writeInt(bs, hidden ? 0 : (int)(vecEnt->pos.y * 1000.0f));
    writeInt(bs, hidden ? 0 : (int)(vecEnt->animSprite.angle * 1000.0f));
    writeInt(bs, hidden ? 100 : (int)vecEnt->health);
    writeInt(bs, player != NULL ? player->role : SH_ROLE_NONE);
    writeInt(bs, flags);
    writeInt(bs, powerMs);
    writeInt(bs, frozenMs);
    writeInt(bs, player != NULL && player->role == SH_ROLE_HUNTER ?
                 player->weaponOnHand.ammoCount : player != NULL ? player->tags : 0);
}

/********************SNAPSHOT READ (CLIENT)********************/

void stealth_readSnapshotHeader(bitstream_t *bs)
{
    unsigned long now = getTimeMillis();

    if(!stream_canRead(bs, 15U * 32U)) {
        bs->overflowed = qtrue;
        return;
    }

    shGame.round = readInt(bs);
    shGame.winner = readInt(bs);
    shGame.roundRemainingMs = readInt(bs);
    shGame.releaseRemainingMs = readInt(bs);
    shGame.hidersAlive = readInt(bs);
    shGame.hidersTotal = readInt(bs);
    shGame.huntersTotal = readInt(bs);
    int pelletMask = readInt(bs);
    for(int i = 0; i < SH_MAX_PELLETS; i++)
        shGame.pelletActive[i] = i < 31 && (pelletMask & (1 << i));
    shTuning.hunterSpeed = readInt(bs) / 100.0f;
    shTuning.hiderSpeed = readInt(bs) / 100.0f;
    shTuning.poweredSpeed = readInt(bs) / 100.0f;
    shTuning.lanternRadius = readInt(bs) / 100.0f;
    /* Read into locals: MAX() would evaluate readInt() twice. */
    int powerMs = readInt(bs);
    int freezeMs = readInt(bs);
    shTuning.powerMs = MAX(1, powerMs);
    shTuning.freezeMs = MAX(1, freezeMs);
    static float reportedHunterSpeed = -1;
    if(shTuning.hunterSpeed != reportedHunterSpeed) {
        printf("tuning from server: hunter_speed=%g hider_speed=%g lantern=%g\n",
               shTuning.hunterSpeed, shTuning.hiderSpeed, shTuning.lanternRadius);
        reportedHunterSpeed = shTuning.hunterSpeed;
    }
    shGame.lastUpdateAt = now;

    int shotCount = readInt(bs);
    if(shotCount < 0 || shotCount > SH_MAX_SHOTS ||
       !stream_canRead(bs, (unsigned int)shotCount * 5U * 32U)) {
        bs->overflowed = qtrue;
        return;
    }
    for(int i = 0; i < shotCount; i++) {
        sh_shot_t incoming;
        incoming.id = readInt(bs);
        incoming.x = readInt(bs) / 100.0f;
        incoming.y = readInt(bs) / 100.0f;
        incoming.endX = readInt(bs) / 100.0f;
        incoming.endY = readInt(bs) / 100.0f;
        incoming.time = now;
        if(incoming.id <= shGame.lastSeenShotID)
            continue;
        shGame.lastSeenShotID = incoming.id;
        shGame.shots[incoming.id % SH_MAX_SHOTS] = incoming;
    }

    shGame.localHiddenCount = 0;
}

static void resetInterpolation(VectorEntity *vecEnt)
{
    memset(&vecEnt->posInterpolate, 0, sizeof(vecEnt->posInterpolate));
    memset(&vecEnt->angleInterpolate, 0, sizeof(vecEnt->angleInterpolate));
}

static void pushSample(VectorEntity *vecEnt, float x, float y, float angle, unsigned long now)
{
    positionInterpolate_t *posIntp = &vecEnt->posInterpolate;
    angleInterpolate_t *angIntp = &vecEnt->angleInterpolate;

    if(posIntp->last > 0) {
        int prev = (posIntp->last - 1) % INTERP_SAMPLES;
        float dx = posIntp->pos[prev][0] - x;
        float dy = posIntp->pos[prev][1] - y;
        /* Respawns and round resets teleport; never slide across the map. */
        if(dx * dx + dy * dy > 40.0f * 40.0f)
            resetInterpolation(vecEnt);
    }

    int slot = posIntp->last % INTERP_SAMPLES;
    posIntp->pos[slot][0] = x;
    posIntp->pos[slot][1] = y;
    posIntp->timestamp[slot] = (long)now;
    posIntp->last++;
    angIntp->angle[slot] = angle;
    angIntp->timestamp[slot] = (long)now;
    angIntp->last = posIntp->last;
}

void stealth_readEntity(bitstream_t *bs, VectorEntity *vecEnt)
{
    unsigned long now = getTimeMillis();
    float x = readInt(bs) / 1000.0f;
    float y = readInt(bs) / 1000.0f;
    float angle = readInt(bs) / 1000.0f;
    float health = (float)readInt(bs);
    int role = readInt(bs);
    int flags = readInt(bs);
    int powerMs = readInt(bs);
    int frozenMs = readInt(bs);
    int counter = readInt(bs);

    PlayerData *player = stealth_player(vecEnt);
    if(player == NULL)
        return;

    bool wasHidden = player->hiddenFromViewer;
    player->role = role;
    player->exposed = (flags & SH_FLAG_EXPOSED) != 0;
    player->spectating = (flags & SH_FLAG_SPECTATOR) != 0;
    player->hiddenFromViewer = (flags & SH_FLAG_HIDDEN) != 0;
    player->powerUntil = (flags & SH_FLAG_POWERED) ? now + (unsigned long)MAX(0, powerMs) : 0;
    player->frozenUntil = (flags & SH_FLAG_FROZEN) ? now + (unsigned long)MAX(0, frozenMs) : 0;
    if(role == SH_ROLE_HUNTER)
        player->weaponOnHand.ammoCount = counter;
    else
        player->tags = counter;

    if(player->hiddenFromViewer) {
        shGame.localHiddenCount++;
        resetInterpolation(vecEnt);
        return;
    }

    if(health < player->lastHealth && health >= 0)
        player->hitFlashUntil = now + 180;
    player->lastHealth = health;
    vecEnt->health = health;

    if(!netEntityList[vecEnt->entID].isPuppet) {
        /* The owner is predicted locally; adopt the authoritative position
         * and let the client replay inputs the server has not yet seen. */
        shPrediction.beforeX = vecEnt->pos.x;
        shPrediction.beforeY = vecEnt->pos.y;
        shPrediction.pending = true;
        vecEnt->pos.x = x;
        vecEnt->pos.y = y;
        return;
    }

    if(wasHidden)
        resetInterpolation(vecEnt);
    pushSample(vecEnt, x, y, angle, now);
}

void stealth_skipEntity(bitstream_t *bs)
{
    for(int i = 0; i < SH_ENTITY_SNAPSHOT_INTS; i++)
        (void)stream_readInt(bs);
}

void stealth_finishSnapshot(void)
{
    static int lastReported = -1;
    static int lastRole = -1;
    int role = stealth_localRole();
    if(role != lastRole) {
        printf("local role: %s\n", roleName(role));
        lastRole = role;
    }
    if(shGame.localHiddenCount != lastReported) {
        printf("stealth hidden hiders: %d\n", shGame.localHiddenCount);
        lastReported = shGame.localHiddenCount;
    }
}

static float lerpAngle(float from, float to, float t)
{
    float diff = to - from;
    while(diff > M_PI) diff -= 2 * M_PI;
    while(diff < -M_PI) diff += 2 * M_PI;
    return from + diff * t;
}

void stealth_interpolate(VectorEntity *vecEnt, unsigned long renderTime)
{
    positionInterpolate_t *posIntp = &vecEnt->posInterpolate;
    angleInterpolate_t *angIntp = &vecEnt->angleInterpolate;
    int count = MIN(posIntp->last, INTERP_SAMPLES);
    if(count == 0)
        return;

    int newest = (posIntp->last - 1) % INTERP_SAMPLES;
    if(count == 1 || (unsigned long)posIntp->timestamp[newest] <= renderTime) {
        vecEnt->pos.x = posIntp->pos[newest][0];
        vecEnt->pos.y = posIntp->pos[newest][1];
        vecEnt->animSprite.angle = angIntp->angle[newest];
        return;
    }

    /* Walk back from the newest sample to the pair bracketing renderTime. */
    for(int i = 1; i < count; i++) {
        int next = (posIntp->last - i) % INTERP_SAMPLES;
        int prev = (posIntp->last - i - 1) % INTERP_SAMPLES;
        if((unsigned long)posIntp->timestamp[prev] > renderTime)
            continue;
        long span = posIntp->timestamp[next] - posIntp->timestamp[prev];
        float t = span > 0 ? (float)((long)renderTime - posIntp->timestamp[prev]) / span : 1.0f;
        t = MAX(0.0f, MIN(1.0f, t));
        vecEnt->pos.x = posIntp->pos[prev][0] + (posIntp->pos[next][0] - posIntp->pos[prev][0]) * t;
        vecEnt->pos.y = posIntp->pos[prev][1] + (posIntp->pos[next][1] - posIntp->pos[prev][1]) * t;
        vecEnt->animSprite.angle = lerpAngle(angIntp->angle[prev], angIntp->angle[next], t);
        return;
    }

    int oldest = (posIntp->last - count) % INTERP_SAMPLES;
    vecEnt->pos.x = posIntp->pos[oldest][0];
    vecEnt->pos.y = posIntp->pos[oldest][1];
    vecEnt->animSprite.angle = angIntp->angle[oldest];
}

VectorEntity *stealth_localPlayer(void)
{
    for(int i = 0; i < VECTOR_ENTITY_COUNT; i++) {
        if(vectorEntityList[i].active && !netEntityList[i].isPuppet)
            return &vectorEntityList[i];
    }
    return NULL;
}

int stealth_localRole(void)
{
    VectorEntity *local = stealth_localPlayer();
    PlayerData *player = local != NULL ? stealth_player(local) : NULL;
    if(player == NULL || player->spectating)
        return SH_ROLE_NONE;
    return player->role;
}
