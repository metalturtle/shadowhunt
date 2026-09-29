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

## Quick start

```sh
./play.sh                 # build, then a local server + 2 windowed clients
./play.sh -c 1 -b 3       # you against three bots
./play.sh -c 0 -b 6       # headless bot match; follow the round log
./play.sh --fast          # short lobby/release/results timers
./play.sh --web -c 0 -b 1 # also serve the browser build on http://localhost:8080/
```

`play.sh` stops any previous local match first, writes every process log to
`.play-logs/`, and Ctrl-C stops everything it started.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j8
```

Clone with `--recursive` (Chipmunk2D is a submodule, used header-only). The
`res/` art folder is not in the repository: the game builds and all automated
tests run without it, but windowed clients need it to draw anything. Launch
from the repository root or the build directory. If `ccache` is installed,
CMake uses it automatically.

## Host a LAN match

Start the server on the host machine (UDP port 8000, or
`SHADOWHUNT_SERVER_PORT`):

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

## Play in a browser

The same client compiles to WebAssembly. Browsers cannot use UDP, so
`tools/web_server.js` (plain Node, no npm packages) serves the page and relays
each browser's WebSocket to the unchanged UDP server. Browser and native
players can share a match.

One-time setup: the official Emscripten SDK (Homebrew's `emscripten` builds
LLVM from source):

```sh
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
~/emsdk/emsdk install latest && ~/emsdk/emsdk activate latest
```

Build and run:

```sh
source ~/emsdk/emsdk_env.sh
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web -j8                # -> build-web/web/shadowhunt.html
./build/Debug/unix_main &                  # the game server
node tools/web_server.js                   # http://localhost:8080/
```

`node tests/browser_smoke.js --url http://localhost:8080/ --out shot.png` drives
headless Chrome (throwaway profile): it joins a match, holds a movement key and
saves a screenshot. A public HTTPS page needs a `wss://` endpoint: put the
relay behind a TLS proxy, or pass `?server=wss://host/ws` in the page URL.

## Tuning and hot reload

Gameplay numbers live in `levels/config/tuning.json`: round length, lobby,
release and results timers, red-hot and freeze durations, pellet respawn,
speeds, lantern radius, pickup and tag reach, damage, ammo and fire rate. The
server checks it twice a second while running, so saving the file changes the
live match. Clients receive the values they need (speeds, lantern, power
timings) from the server. Invalid JSON or out-of-range values are rejected and
the previous values kept; the server log says which.

Lights, spawns and pellet sites in `levels/level.json` hot reload the same way
(server and clients); walls need a restart. `SHADOWHUNT_TUNING` and
`SHADOWHUNT_LEVEL` point at other files.

## Level data

`levels/level.json` holds the stealth layout alongside the art and collision:

- `light.object`: lamps as `[x, y, radius]`.
- `stealth.hunter_spawns`, `stealth.hider_spawns`: `[x, y]` points.
- `stealth.pellets`: red-pellet sites as `[x, y]`.

`SHADOWHUNT_OVERVIEW=1` frames the whole map in a client, which is useful when
placing lights.

## Development switches

| Variable | Effect |
|---|---|
| `SHADOWHUNT_BOT=1` | client plays by itself (hiders sneak, grab pellets, burn hunters; hunters patrol and shoot what they can see) |
| `SHADOWHUNT_HEADLESS=1` | client skips drawing (bots, tests) |
| `SHADOWHUNT_TUNING`, `SHADOWHUNT_LEVEL` | alternate tuning or level file |
| `SHADOWHUNT_SERVER_PORT` | server UDP port (tests run servers in parallel) |
| `SHADOWHUNT_ROUND_SECONDS` | override the round clock |
| `SHADOWHUNT_VERBOSE=1` | asset and connection chatter |
| `SHADOWHUNT_NO_HOT_RELOAD=1` | disable file watching |
| `SHADOWHUNT_TEST_LOGS=1` | per-input logs the test harness reads |

## Automated checks

```sh
ctest --test-dir build -j4 --output-on-failure    # about 30 seconds
```

CI (`.github/workflows/ci.yml`) runs the same on macOS for every push.

| Test | What it proves |
|---|---|
| `raycast_geometry` | ray/wall geometry |
| `stealth_rules` | hunter counts, role rotation, light exposure, lantern, touch reach, round outcomes |
| `multiplayer_smoke_4_clients` | join/sync, authoritative movement, malformed packets, disconnect propagation |
| `multiplayer_round_rematch` | one hunter shoots every hider, hunters win, rematch rotates roles |
| `stealth_visibility_4_clients` | the hunter's snapshots withhold shadowed hiders; hiders see everyone |
| `stealth_full_server_8_clients` | a full 8-player server with 2 hunters stays synchronized |
| `stealth_lit_rematch_visibility` | lit hiders stay visible to the hunter across a rematch |
| `bots_play_a_round` | bots play a round to a hunter win and start a rematch |
| `tuning_and_level_hot_reload` | live tuning and layout reloads reach server and clients; broken JSON is rejected |
| `stealth_pellet_tag_duel` | a red-hot hider burns and freezes the hunter; hiders survive the clock |

Test clients run headless with `tests/fixtures/fast_tuning.json`, each test on
its own server port. For a longer soak:

```sh
python3 tests/multiplayer_smoke.py --executable build/Debug/unix_main \
  --clients 8 --timeout 20 --verify-stealth --hold-seconds 60
```
