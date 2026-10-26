#!/usr/bin/env python3
"""test de bout en bout d'Autosu : lance le build sans fenêtre sur une map de ~21 s jouée en Auto, lui envoie de
vraies commandes UDP et vérifie l'état qu'il publie. échoue si une étape ne se produit pas :

  commande hors liste refusée -> autosu_miss : exactement un miss -> speed_override 1.5 -> cs_override 7
  -> retour à la normale -> autosu_retry : la map repart de zéro -> arrêt propre du jeu

usage : e2e.py <neomod.exe> [--timeout SECS]   (ports 13370 et 13371 libres)
même préparation du dossier de données que tests/sanity/run.py, avec une map plus longue.
"""

import argparse
import json
import math
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import wave
from pathlib import Path

COMMAND_PORT, STATE_PORT = 13370, 13371
# joue la map en Auto le temps du test, puis quitte proprement
SCRIPT = ["mod_autoplay 1", "set_active_ui_screen songbrowser", "@wait_secs 3", "sendkey Return", "@wait_secs 32", "exit"]

HEADER = """osu file format v14

[General]
AudioFilename: audio.wav
AudioLeadIn: 0
Mode: 0

[Metadata]
Title:Autosu E2E
Artist:neomod
Creator:neomod
Version:Normal
BeatmapID:0
BeatmapSetID:-1

[Difficulty]
HPDrainRate:5
CircleSize:4
OverallDifficulty:8
ApproachRate:9
SliderMultiplier:1.4
SliderTickRate:1

[TimingPoints]
0,500,4,2,0,100,1,0

[HitObjects]
"""


def beatmap():
    """un cercle toutes les 400 ms pendant ~20 s, avec un slider et un spinner au milieu"""
    objects, t, i = [], 1000, 0
    while t <= 20000:
        if t == 9000:
            objects.append(f"64,256,{t},2,0,L|320:256,1,256")
            t += 1000
        elif t == 15000:
            objects.append(f"256,192,{t},12,0,{t + 1000},0:0:0:0:")
            t += 1400
        else:
            objects.append(f"{64 + i % 7 * 64},{96 + i % 3 * 96},{t},{5 if i == 0 else 1},0,0:0:0:0:")
            t += 400
            i += 1
    return HEADER + "\n".join(objects) + "\n"


def write_data_dir(data):
    (data / "cfg").mkdir(parents=True)
    (data / "osu").mkdir()
    (data / "cfg" / "osu.cfg").write_text(
        f"auto_update 0\nosu_folder {data.as_posix()}/osu\nwindowed_resolution 1280x720\n"
    )
    songs = data / "maps" / "Autosu E2E"
    songs.mkdir(parents=True)
    (songs / "Normal.osu").write_text(beatmap())
    # 25 s d'un sinus à 441 Hz (le traitement audio saute le silence)
    with wave.open(str(songs / "audio.wav"), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(22050)
        period = struct.pack("<50h", *(round(8000 * math.sin(2 * math.pi * i / 50)) for i in range(50)))
        w.writeframes(period * (22050 * 25 // 50))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("target", type=Path, help="neomod.exe du build Autosu")
    ap.add_argument("--timeout", type=float, default=120, metavar="SECS")
    args = ap.parse_args()
    sys.stdout.reconfigure(errors="replace")

    target = args.target.resolve()
    data = Path(tempfile.mkdtemp(prefix="autosu-e2e-"))
    write_data_dir(data)

    state = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    state.bind(("127.0.0.1", STATE_PORT))
    state.settimeout(0.5)
    commands = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def send(command):
        commands.sendto(command.encode(), ("127.0.0.1", COMMAND_PORT))

    game = {}

    def run_game():
        cmd = [str(target), "-headless", "-multi", "-datadir", data.as_posix()]
        try:
            proc = subprocess.run(cmd, cwd=target.parent, input="".join(ln + "\n" for ln in SCRIPT),
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT, encoding="utf-8",
                                  errors="replace", timeout=args.timeout)
            game["rc"], game["log"] = proc.returncode, proc.stdout
        except subprocess.TimeoutExpired as e:
            game["rc"] = None
            game["log"] = e.output.decode("utf-8", "replace") if isinstance(e.output, bytes) else e.output or ""

    runner = threading.Thread(target=run_game)
    start = time.monotonic()
    runner.start()

    phase, states, seen = "attente", 0, {}
    saw_pp = False
    while phase != "fini" and runner.is_alive():
        try:
            s = json.loads(state.recvfrom(65536)[0])
        except socket.timeout:
            continue
        states += 1
        seen.setdefault("protocole", s.get("protocol"))
        saw_pp = saw_pp or ("ppFc" in s and "stars" in s)
        if not s.get("playing"):
            continue
        if phase == "attente" and s["combo"] >= 3:
            seen["avant"] = s
            send("quit")  # hors liste : doit être refusée, le jeu doit continuer
            send("autosu_miss")
            phase = "miss"
        elif phase == "miss" and s["misses"] > seen["avant"]["misses"]:
            seen["miss"] = s
            send("speed_override 1.5")
            phase = "vitesse"
        elif phase == "vitesse" and s["speed"] == 1.5:
            send("cs_override 7")
            phase = "cs"
        elif phase == "cs" and s["cs"] == 7:
            send("speed_override -1")
            send("cs_override -1")
            phase = "retour"
        elif phase == "retour" and s["speed"] == 1 and s["cs"] == 4:
            seen["retour"] = s
            send("autosu_retry")
            phase = "retry"
        elif phase == "retry" and s["progress"] < seen["retour"]["progress"] - 0.05:
            seen["retry"] = s
            phase = "fini"
    runner.join()
    elapsed = time.monotonic() - start
    log = game.get("log", "")

    problems = []
    if phase != "fini":
        problems.append(f"bloqué à l'étape « {phase} » ({states} états reçus)")
    if seen.get("protocole") != 1:
        problems.append(f"protocole {seen.get('protocole')} au lieu de 1")
    if not saw_pp:
        problems.append("ppFc et stars jamais publiés")
    if "miss" in seen and seen["miss"]["misses"] != seen["avant"]["misses"] + 1:
        problems.append(f"{seen['miss']['misses'] - seen['avant']['misses']} miss au lieu d'un seul")
    if "miss" in seen and seen["miss"]["combo"] != 0:
        problems.append("combo pas remis à 0 par le miss")
    if "command refused: quit" not in log:
        problems.append("la commande hors liste n'a pas été refusée")
    if game.get("rc") is None:
        problems.append(f"jeu bloqué (tué après {args.timeout:g} s)")
    elif game["rc"] != 0 or "Shutdown success." not in log:
        problems.append(f"arrêt du jeu anormal (code {game['rc']})")

    if problems:
        print(log)
        print(f"FAIL après {elapsed:.1f} s (dossier de données : {data})")
        for p in problems:
            print(f"  {p}")
        return 1
    shutil.rmtree(data, ignore_errors=True)
    print(f"PASS en {elapsed:.1f} s : {states} états, 1 miss, vitesse 1,5, CS 7, retour, retry, commande refusée")
    return 0


if __name__ == "__main__":
    sys.exit(main())
