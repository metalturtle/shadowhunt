# Shadowhunt

Shadowhunt is a top-down multiplayer stealth shooter for 2–8 players on a LAN,
built on SDL3 with an authoritative UDP server. Hotline Miami is the general
inspiration: a night-time harbor, neon lamp pools, fast rounds, one-screen
readable violence.

## How to play

Every round splits the players into two teams:

| | Hunters | Hiders |
|---|---|---|
| Count | 1 (2–4 players) or 2 (5–8 players) | everyone else |
| Weapon | hitscan machine gun, 90 rounds | none |
| Sees | only what is lit: lamp pools and their own small lantern | the whole (dim) map and every player |
| Goal | shoot every hider before the clock runs out | survive until the clock runs out |

- **The dark is a mask.** A hider in shadow is invisible to hunters. The server
  does not even send that hider's position to hunter clients. Hiders are exposed
  while they stand in a lamp's light or within a hunter's lantern radius. Your
  HUD shows `HIDDEN` or `EXPOSED`. Hunters can still fire blind into the dark.
- **Red pellets.** Glowing red pellets sit around the map. A hider who takes one
  turns *red hot* for 7 seconds. A red-hot hider glows (so hunters can see them),
  is bulletproof, and moves faster.
  Touching a hunter while red hot **burns** them. The hunter drops back to the
  hunter spawn, frozen for 5 seconds, like a ghost in Pac-Man. Pellets respawn
  after 20 seconds.
- **Rounds.** A 3-second lobby countdown starts once 2 players are connected.
  Hunters are frozen for the first 4 seconds while hiders scatter. A round lasts
  2:30. Hunters win by shooting every hider (3 hits each); hiders win if anyone
  survives the clock. If every hunter leaves, hiders win. Roles rotate every
  round, so everyone gets to hunt.
- **Late joiners** who arrive during the hunters' release countdown join as
  hiders. Anyone later watches until the next round.

Controls: **WASD** move, **mouse** aim, **left click** (or **T**) shoot.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j8
```

The game needs the `levels/` and `res/` asset folders. Launch it from the
repository root, or from the build directory, where CMake copies them.

## Host a match

Start the server on the host machine (it listens on UDP port 8000):

```sh
./build/Debug/unix_main
```

Start each client with its own local UDP port. Replace `SERVER_IP` with the
host's IPv4 address:

```sh
./build/Debug/unix_main 8001 SERVER_IP 8000
./build/Debug/unix_main 8002 SERVER_IP 8000
```

The host firewall and router must allow UDP port 8000 for remote machines. The
client window is resizable.

## Level data

`levels/level.json` holds the stealth layout alongside the art and collision:

- `light.object`: lamps as `[x, y, radius]`.
- `stealth.hunter_spawns`, `stealth.hider_spawns`: `[x, y]` points.
- `stealth.pellets`: red-pellet sites as `[x, y]`.

Set `SHADOWHUNT_LEVEL=path/to/level.json` to load a different layout, as the
test fixtures in `tests/fixtures/` do. `SHADOWHUNT_OVERVIEW=1` frames the
whole map in a client, which is useful when placing lights.

## Automated checks

```sh
ctest --test-dir build --output-on-failure
```

| Test | What it proves |
|---|---|
| `raycast_geometry` | ray/wall geometry |
| `stealth_rules` | hunter counts, role rotation, light exposure, lantern, touch reach, round outcomes |
| `multiplayer_smoke_4_clients` | join/sync, authoritative movement, malformed packets, disconnect propagation |
| `multiplayer_round_rematch` | one hunter shoots every hider, hunters win, rematch rotates roles |
| `stealth_visibility_4_clients` | the hunter's snapshots withhold shadowed hiders; hiders see everyone |
| `stealth_full_server_8_clients` | a full 8-player server with 2 hunters stays synchronized |
| `stealth_pellet_tag_duel` | a red-hot hider burns and freezes the hunter; hiders survive the clock |

For a longer local run under the dummy SDL driver:

```sh
python3 tests/multiplayer_smoke.py \
  --executable build/Debug/unix_main \
  --clients 8 --timeout 20 --verify-stealth --hold-seconds 60
```
