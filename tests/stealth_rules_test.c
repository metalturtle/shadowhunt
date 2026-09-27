#include "stealth_rules.h"
#include <stdio.h>

static int failures;

static void expect_int(const char *name, int actual, int expected)
{
    if(actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", name, expected, actual);
        failures++;
    }
}

static int hunters_in_round(int round, int players)
{
    int hunters = sh_hunter_count(players);
    int count = 0;
    for(int i = 0; i < players; i++)
        count += sh_is_hunter_slot(round, i, players, hunters);
    return count;
}

int main(void)
{
    expect_int("no hunt alone", sh_hunter_count(1), 0);
    expect_int("duel", sh_hunter_count(2), 1);
    expect_int("four players", sh_hunter_count(4), 1);
    expect_int("five players", sh_hunter_count(5), 2);
    expect_int("eight players", sh_hunter_count(8), 2);

    for(int players = 2; players <= 8; players++) {
        for(int round = 0; round < 10; round++)
            expect_int("exact hunter count", hunters_in_round(round, players),
                       sh_hunter_count(players));
    }

    /* Every player hunts once within `players` rounds of a 1-hunter game. */
    int hunted[4] = {0};
    for(int round = 0; round < 4; round++) {
        for(int i = 0; i < 4; i++)
            hunted[i] += sh_is_hunter_slot(round, i, 4, 1);
    }
    for(int i = 0; i < 4; i++)
        expect_int("rotation fairness", hunted[i], 1);

    const sh_light_t lights[2] = {{100, 100, 20}, {300, 100, 10}};
    expect_int("inside light", sh_point_lit(110, 100, 0, lights, 2), 1);
    expect_int("light edge", sh_point_lit(120, 100, 0, lights, 2), 1);
    expect_int("outside light", sh_point_lit(125, 100, 0, lights, 2), 0);
    expect_int("margin widens", sh_point_lit(125, 100, 6, lights, 2), 1);
    expect_int("second light", sh_point_lit(305, 102, 0, lights, 2), 1);

    const float hunter[2] = {200, 200};
    expect_int("hidden in dark", sh_hider_exposed(200, 240, 0, lights, 2, hunter, 1, 30), 0);
    expect_int("seen in light", sh_hider_exposed(100, 105, 0, lights, 2, hunter, 1, 30), 1);
    expect_int("red hot glows", sh_hider_exposed(200, 240, 1, lights, 2, hunter, 1, 30), 1);
    expect_int("lantern reveals", sh_hider_exposed(200, 225, 0, lights, 2, hunter, 1, 30), 1);
    expect_int("no hunters no lantern", sh_hider_exposed(200, 225, 0, lights, 2, hunter, 0, 30), 0);

    expect_int("touch", sh_touching(0, 0, 6, 8, 10), 1);
    expect_int("no touch", sh_touching(0, 0, 6, 8.5f, 10), 0);

    expect_int("round continues", sh_round_outcome(1, 3, 2, 5000), SH_WINNER_NONE);
    expect_int("all hiders shot", sh_round_outcome(1, 3, 0, 5000), SH_WINNER_HUNTERS);
    expect_int("hiders survive clock", sh_round_outcome(1, 3, 1, 0), SH_WINNER_HIDERS);
    expect_int("hunter left", sh_round_outcome(0, 3, 3, 5000), SH_WINNER_HIDERS);
    expect_int("hiders left", sh_round_outcome(1, 0, 0, 5000), SH_WINNER_HUNTERS);

    if(failures)
        return 1;
    puts("Stealth rule checks passed.");
    return 0;
}
