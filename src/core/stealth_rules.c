#include "stealth_rules.h"

int sh_hunter_count(int players)
{
    if(players < 2)
        return 0;
    return players >= 5 ? 2 : 1;
}

int sh_is_hunter_slot(int round, int index, int players, int hunters)
{
    if(players <= 0 || hunters <= 0 || index < 0 || index >= players)
        return 0;
    if(round < 0)
        round = 0;

    int first = (int)(((long)round * hunters) % players);
    int offset = (index - first + players) % players;
    return offset < hunters;
}

int sh_point_lit(float x, float y, float margin,
                 const sh_light_t *lights, int lightCount)
{
    for(int i = 0; i < lightCount; i++) {
        float dx = x - lights[i].x;
        float dy = y - lights[i].y;
        float reach = lights[i].radius + margin;
        if(reach > 0 && dx * dx + dy * dy <= reach * reach)
            return 1;
    }
    return 0;
}

int sh_hider_exposed(float x, float y, int powered,
                     const sh_light_t *lights, int lightCount,
                     const float *hunterXY, int hunterCount,
                     float lanternRadius)
{
    if(powered)
        return 1;
    if(sh_point_lit(x, y, 0.0f, lights, lightCount))
        return 1;
    for(int i = 0; i < hunterCount; i++) {
        if(sh_touching(x, y, hunterXY[i * 2], hunterXY[i * 2 + 1], lanternRadius))
            return 1;
    }
    return 0;
}

int sh_touching(float ax, float ay, float bx, float by, float reach)
{
    float dx = ax - bx;
    float dy = ay - by;
    return dx * dx + dy * dy <= reach * reach;
}

int sh_round_outcome(int huntersConnected, int hidersConnected,
                     int hidersAlive, long remainingMs)
{
    if(huntersConnected <= 0)
        return SH_WINNER_HIDERS;
    if(hidersConnected <= 0 || hidersAlive <= 0)
        return SH_WINNER_HUNTERS;
    if(remainingMs <= 0)
        return SH_WINNER_HIDERS;
    return SH_WINNER_NONE;
}
