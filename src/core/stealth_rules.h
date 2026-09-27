#ifndef SHADOWHUNT_STEALTH_RULES_H
#define SHADOWHUNT_STEALTH_RULES_H

/* Pure hunters-versus-hiders rules shared by the server, the client and the
 * native tests. Nothing here touches engine globals, time or the network. */

#define SH_MAX_LIGHTS 32
#define SH_MAX_SPAWNS 16
#define SH_MAX_PELLETS 16

typedef enum {
    SH_ROLE_NONE = 0,
    SH_ROLE_HUNTER = 1,
    SH_ROLE_HIDER = 2
} sh_role_e;

typedef enum {
    SH_WINNER_NONE = 0,
    SH_WINNER_HUNTERS = 1,
    SH_WINNER_HIDERS = 2
} sh_winner_e;

typedef struct {
    float x, y, radius;
} sh_light_t;

/* One hunter for up to four players, two hunters for five or more. */
int sh_hunter_count(int players);

/* Hunters rotate every round: in round `round` the players at indices
 * round*hunters .. round*hunters+hunters-1 (mod players) hunt. */
int sh_is_hunter_slot(int round, int index, int players, int hunters);

/* True when (x, y), widened by `margin`, lies inside any light circle. */
int sh_point_lit(float x, float y, float margin,
                 const sh_light_t *lights, int lightCount);

/* A hider is visible to hunters while it glows red hot, stands in a static
 * light, or is inside the small lantern radius carried by any hunter. */
int sh_hider_exposed(float x, float y, int powered,
                     const sh_light_t *lights, int lightCount,
                     const float *hunterXY, int hunterCount,
                     float lanternRadius);

int sh_touching(float ax, float ay, float bx, float by, float reach);

/* Round outcome once the round has started. Returns SH_WINNER_NONE while the
 * round should continue. */
int sh_round_outcome(int huntersConnected, int hidersConnected,
                     int hidersAlive, long remainingMs);

#endif
