#!/usr/bin/env python3
"""Run a local UDP server and 1-8 Shadowhunt clients as a smoke test."""

import argparse
import json
import os
import re
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time
from pathlib import Path


SERVER_PORT = 8000  # The current executable fixes both server and client target to this port.


def reserve_client_ports(count):
    sockets = []
    ports = []
    try:
        for _ in range(count):
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.bind(("127.0.0.1", 0))
            sockets.append(sock)
            ports.append(sock.getsockname()[1])
    finally:
        for sock in sockets:
            sock.close()
    if SERVER_PORT in ports:
        return reserve_client_ports(count)
    return ports


def read_log(path):
    try:
        return path.read_text(errors="replace")
    except OSError:
        return ""


def terminate(processes, timeout=3):
    """Ask SDL apps to stop with SIGINT, then reap or force-stop stragglers."""
    for process in processes:
        if process.poll() is None:
            try:
                process.send_signal(signal.SIGINT)
            except ProcessLookupError:
                pass
    deadline = time.monotonic() + timeout
    for process in processes:
        remaining = max(0, deadline - time.monotonic())
        try:
            process.wait(timeout=remaining)
        except subprocess.TimeoutExpired:
            process.terminate()
    for process in processes:
        try:
            process.wait(timeout=1)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, default=Path("build/Debug/unix_main"))
    parser.add_argument("--clients", type=int, default=1, help="number of clients (1-8)")
    parser.add_argument("--timeout", type=float, default=15, help="seconds to wait for connection and acknowledgements")
    parser.add_argument("--hold-seconds", type=float, default=0,
                        help="keep all processes exchanging packets for this long after verification")
    parser.add_argument("--verify-authority-disconnect", action="store_true",
                        help="require 4 clients, then verify movement, timeout disconnect, and replicated removal")
    parser.add_argument("--verify-round", action="store_true",
                        help="require 4 clients, then fire until results and verify automatic rematch")
    parser.add_argument("--verify-malformed", action="store_true",
                        help="send malformed UDP datagrams and require the server to keep accepting clients")
    parser.add_argument("--verify-stealth", action="store_true",
                        help="require the hunter's snapshots to withhold hiders standing in shadow")
    parser.add_argument("--verify-tag", action="store_true",
                        help="duel fixture: a red-hot hider burns and freezes the hunter, then survives the clock")
    parser.add_argument("--verify-lit-rounds", action="store_true",
                        help="fully lit fixture: no hider may be withheld from a hunter in rounds 1 or 2")
    parser.add_argument("--level", type=Path, help="level fixture passed to every process via SHADOWHUNT_LEVEL")
    parser.add_argument("--round-seconds", type=int, help="shorten the hunt clock via SHADOWHUNT_ROUND_SECONDS")
    parser.add_argument("--logs", type=Path, help="directory for logs (defaults to a temporary directory)")
    args = parser.parse_args()
    if not 1 <= args.clients <= 8:
        parser.error("--clients must be between 1 and 8")
    executable = args.executable.resolve()
    if not executable.is_file():
        parser.error(f"executable not found: {executable}")
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    if args.hold_seconds < 0:
        parser.error("--hold-seconds cannot be negative")
    if args.verify_authority_disconnect and args.clients != 4:
        parser.error("--verify-authority-disconnect requires --clients 4")
    if args.verify_round and args.clients != 4:
        parser.error("--verify-round requires --clients 4")
    if args.verify_round and args.verify_authority_disconnect:
        parser.error("round and disconnect verification are separate scenarios")
    if args.verify_tag and args.clients != 2:
        parser.error("--verify-tag requires --clients 2")

    if args.logs:
        log_dir = args.logs.resolve()
        log_dir.mkdir(parents=True, exist_ok=True)
        keep_logs = True
    else:
        log_dir = Path(tempfile.mkdtemp(prefix="shadowhunt-smoke-"))
        keep_logs = False
    print(f"Logs: {log_dir}", flush=True)

    # Line buffering keeps diagnostics useful while processes are still running.
    stdbuf = shutil.which("stdbuf")
    command = ([stdbuf, "-oL", "-eL"] if stdbuf else []) + [str(executable)]
    env = os.environ.copy()
    env.setdefault("SDL_VIDEODRIVER", "dummy")
    env.setdefault("SDL_RENDER_DRIVER", "software")
    env["SHADOWHUNT_TEST_LOGS"] = "1"
    repo_root = Path(__file__).resolve().parent.parent
    level_path = (args.level if args.level else repo_root / "levels" / "level.json").resolve()
    if args.level:
        env["SHADOWHUNT_LEVEL"] = str(level_path)
    if args.round_seconds:
        env["SHADOWHUNT_ROUND_SECONDS"] = str(args.round_seconds)
    level = json.loads(level_path.read_text())
    processes = []
    log_files = []
    failure = None
    try:
        server_log = (log_dir / "server.log").open("w")
        log_files.append(server_log)
        server = subprocess.Popen(command, cwd=repo_root, env=env,
                                  stdout=server_log, stderr=subprocess.STDOUT, start_new_session=True)
        processes.append(server)
        time.sleep(0.35)
        if server.poll() is not None:
            raise RuntimeError(f"server exited early with status {server.returncode}")

        if args.verify_malformed:
            probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            malformed = [bytes(length) for length in range(1, 13)]
            malformed.extend([
                struct.pack("<IBII", 1, 2, 1, 0) + b"\x04",
                struct.pack("<IBII", 1, 0, 16385, 0) + b"\x04",
                struct.pack("<IBII", 1, 0, 1, 999999) + b"\x04",
            ])
            for payload in malformed:
                probe.sendto(payload, ("127.0.0.1", SERVER_PORT))
            probe.close()
            time.sleep(0.2)
            if server.poll() is not None:
                raise RuntimeError("server exited after malformed UDP datagrams")
            print("PASS: server survived malformed UDP datagrams.", flush=True)

        ports = reserve_client_ports(args.clients)
        client_logs = []
        for index, port in enumerate(ports, 1):
            path = log_dir / f"client-{index}.log"
            client_logs.append(path)
            log = path.open("w")
            log_files.append(log)
            client_env = env.copy()
            if args.verify_authority_disconnect and index == 1:
                client_env["SHADOWHUNT_TEST_KEYS"] = "d"
            if args.verify_round:
                client_env["SHADOWHUNT_TEST_KEYS"] = "t"
                client_env["SHADOWHUNT_TEST_AIM"] = "0"
            process = subprocess.Popen(command + [str(port)], cwd=repo_root,
                                       env=client_env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
            processes.append(process)

        server_path = log_dir / "server.log"
        deadline = time.monotonic() + args.timeout
        while time.monotonic() < deadline:
            exited = [(i, p.returncode) for i, p in enumerate(processes) if p.poll() is not None]
            if exited:
                raise RuntimeError(f"process exited before smoke test completed: {exited}")
            server_text = read_log(server_path)
            connected = len(re.findall(r"adding client\. conid=\d+", server_text))
            clients_ready = all("client setting state to run" in read_log(path) for path in client_logs)
            clients_acked = all(re.search(r"acked record ID\s+\d+", read_log(path)) for path in client_logs)
            clients_synced = all(len(re.findall(r"creating entity\s+\d+", read_log(path))) >= args.clients
                                 for path in client_logs)
            if connected >= args.clients and clients_ready and clients_acked and clients_synced:
                print(f"PASS: {args.clients} client(s) connected, synchronized all players, and received input acknowledgements.", flush=True)
                break
            time.sleep(0.1)
        else:
            raise RuntimeError("timed out waiting for every client to connect, synchronize all players, and receive an acknowledgement")

        if args.verify_authority_disconnect:
            moving_log = read_log(client_logs[0])
            if "test input active: d" not in moving_log:
                raise RuntimeError("moving client did not activate the test movement input")

            def server_positions():
                values = re.findall(r"vector position\s+(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)", read_log(server_path))
                return [(float(x), float(y)) for x, y in values]

            stealth = level.get("stealth", {})
            spawn_positions = {(float(x), float(y)) for x, y in
                               stealth.get("hunter_spawns", []) + stealth.get("hider_spawns", [])}
            # A hunter waits out the lobby and release countdowns before moving.
            movement_deadline = time.monotonic() + max(args.timeout, 12)
            while time.monotonic() < movement_deadline:
                positions = server_positions()
                if any((x, y) not in spawn_positions for x, y in positions):
                    break
                time.sleep(0.1)
            else:
                raise RuntimeError("server did not report a changed entity position after client movement input")
            print("PASS: server reported an authoritative position change.", flush=True)

            owner_match = re.search(
                r"client ent id -1 \d+ 1\s*\ncreating entity (\d+)\s+\d+", moving_log
            )
            if not owner_match:
                raise RuntimeError("could not identify the moving client's owned server entity from its sync log")
            owner_entity_id = int(owner_match.group(1))

            # Abruptly stopping this process models a lost client; the server
            # drops it after its existing three-second receive timeout.
            processes[1].kill()
            processes[1].wait(timeout=3)
            disconnect_deadline = time.monotonic() + max(6, min(args.timeout, 10))
            while time.monotonic() < disconnect_deadline:
                server_text = read_log(server_path)
                survivor_text = read_log(client_logs[1])
                disconnected = re.search(r"disconnecting client\. conid=\d+ entity=" + str(owner_entity_id), server_text)
                removed = f"removing remote entity {owner_entity_id}" in survivor_text
                if disconnected and removed:
                    break
                if processes[0].poll() is not None or processes[2].poll() is not None:
                    raise RuntimeError("server or a surviving client exited during disconnect propagation")
                time.sleep(0.1)
            else:
                server_text = read_log(server_path)
                survivor_text = read_log(client_logs[1])
                raise RuntimeError(
                    f"server timeout or surviving client entity removal not observed for entity {owner_entity_id}; "
                    f"server_disconnect={bool(re.search(r'disconnecting client\\. conid=\\d+ entity=' + str(owner_entity_id), server_text))}, "
                    f"survivor_removed={'removing remote entity ' + str(owner_entity_id) in survivor_text}"
                )
            print(f"PASS: server timed out disconnected client and survivor removed entity {owner_entity_id}.", flush=True)

        def wait_for(description, predicate, seconds):
            deadline = time.monotonic() + seconds
            while time.monotonic() < deadline:
                if predicate():
                    return
                if any(process.poll() is not None for process in processes):
                    raise RuntimeError(f"a process exited while waiting for {description}")
                time.sleep(0.1)
            raise RuntimeError(f"timed out waiting for {description}")

        def server_has(*needles):
            text = read_log(server_path)
            return all(needle in text for needle in needles)

        if args.verify_round:
            wait_for("every hider shot, a hunter win, and a rematch",
                     lambda: server_has("hider eliminated", "round winner: hunters") and
                     read_log(server_path).count("match state: running") >= 2,
                     max(20, args.timeout))
            roles = re.findall(r"round 1: conid=\d+ entity=\d+ role=(\w+)", read_log(server_path))
            if roles.count("hunter") != 1 or roles.count("hider") != args.clients - 1:
                raise RuntimeError(f"unexpected round 1 roles: {roles}")
            round2 = re.findall(r"round 2: conid=(\d+) entity=\d+ role=hunter", read_log(server_path))
            round1 = re.findall(r"round 1: conid=(\d+) entity=\d+ role=hunter", read_log(server_path))
            if not round2 or round2 == round1:
                raise RuntimeError(f"hunter role did not rotate: round1={round1} round2={round2}")
            print("PASS: one hunter shot every hider, hunters won, and the rematch rotated roles.", flush=True)

        if args.verify_stealth:
            def role_logs(role):
                return [p for p in client_logs if f"local role: {role}" in read_log(p)]
            wait_for("roles on every client",
                     lambda: len(role_logs("hunter")) + len(role_logs("hider")) == args.clients, 15)
            hunters, hiders = role_logs("hunter"), role_logs("hider")
            expected_hunters = 2 if args.clients >= 5 else 1
            if len(hunters) != expected_hunters:
                raise RuntimeError(f"expected {expected_hunters} hunter client(s), found {len(hunters)}")
            for path in hunters:
                wait_for("every hunter's snapshots to withhold shadowed hiders",
                         lambda: re.search(r"stealth hidden hiders: [1-9]", read_log(path)), 10)
            for path in hiders:
                if re.search(r"stealth hidden hiders: [1-9]", read_log(path)):
                    raise RuntimeError(f"a hider's snapshot withheld players: {path.name}")
            print("PASS: hiders in shadow were withheld from the hunter but visible to hiders.", flush=True)

        if args.verify_lit_rounds:
            wait_for("a second round", lambda: server_has("round 2: "), max(30, args.timeout))
            time.sleep(3)
            for path in client_logs:
                if re.search(r"stealth hidden hiders: [1-9]", read_log(path)):
                    raise RuntimeError(f"a lit hider was withheld from a hunter: {path.name}")
            print("PASS: lit hiders stayed visible to the hunter across the rematch.", flush=True)

        if args.verify_tag:
            wait_for("a hider to take a pellet", lambda: server_has("pellet taken"), 12)
            wait_for("the red-hot hider to burn the hunter",
                     lambda: server_has("hunter tagged"), 12)
            wait_for("the hiders to survive the clock",
                     lambda: server_has("round winner: hiders"), max(20, args.timeout))
            print("PASS: a red-hot hider froze the hunter and the hiders survived the clock.", flush=True)

        if args.hold_seconds:
            hold_deadline = time.monotonic() + args.hold_seconds
            while time.monotonic() < hold_deadline:
                exited = [(i, process.returncode) for i, process in enumerate(processes)
                          if process.poll() is not None]
                if exited:
                    raise RuntimeError(f"process exited during hold period: {exited}")
                time.sleep(0.1)
            print(f"PASS: all processes remained active for {args.hold_seconds:g} additional seconds.", flush=True)
    except Exception as exc:
        failure = str(exc)
    finally:
        terminate(processes)
        for log in log_files:
            log.close()

    if failure:
        print(f"FAIL: {failure}", file=sys.stderr)
        for path in sorted(log_dir.glob("*.log")):
            print(f"\n--- {path.name} ---", file=sys.stderr)
            contents = read_log(path)
            print(contents[-12000:] if contents else "(empty)", file=sys.stderr)
        if not keep_logs:
            print(f"Temporary logs retained at {log_dir}", file=sys.stderr)
        return 1
    if keep_logs:
        print("Process logs retained.")
    else:
        shutil.rmtree(log_dir, ignore_errors=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
