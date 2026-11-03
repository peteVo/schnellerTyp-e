#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Bounded smoke test of the installed addon in a private Qt scratch window."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import tempfile
import urllib.request
from gi.repository import GLib

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("build_dir", nargs="?", type=Path, default=Path("build-native"))
parser.add_argument("--manual", action="store_true", help="Wait for real keyboard input; send no synthetic keys")
parser.add_argument("--browser", action="store_true", help="Use a fresh Chromium XWayland scratch window (manual only)")
args = parser.parse_args()
if args.browser and not args.manual:
    parser.error("--browser requires --manual")
build = args.build_dir.resolve()
state_path = build / "omarchy-scratch.json"
results = []
processes = []
logs = []
connection = None
read_state = lambda: json.loads(state_path.read_text())
group = f"schnellerTyp-e verification {os.getpid()}"


def run(*command):
    return subprocess.check_output(command, text=True).strip()


def call(method, *arguments):
    return run("gdbus", "call", "--session", "--dest", "org.fcitx.Fcitx5", "--object-path",
               "/controller", "--method", "org.fcitx.Fcitx.Controller1." + method, *arguments)


def wait(read, expected, timeout=8):
    deadline = time.monotonic() + timeout
    value = None
    while time.monotonic() < deadline:
        try:
            value = read()
            if expected(value):
                return value
        except (OSError, ValueError):
            pass
        time.sleep(0.05)
    raise RuntimeError(f"Unexpected scratch state: {value!r}")


original_group = run("fcitx5-remote", "-q")
original_im = run("fcitx5-remote", "-n")
original_state = run("fcitx5-remote")
created = False
original_config = None
try:
    original_config = GLib.Variant.parse(None, call("GetConfig", "fcitx://config/global"), None, None).get_child_value(0).get_variant()
    values = original_config.unpack()
    values["Behavior"]["ShareInputState"] = "All"
    values["Behavior"]["ActiveByDefault"] = "True"

    def variant(value):
        if isinstance(value, dict):
            return GLib.Variant("a{sv}", {key: variant(child) for key, child in value.items()})
        return GLib.Variant("s", value)

    call("SetConfig", "fcitx://config/global", "<" + variant(values).print_(True) + ">")
    call("AddInputMethodGroup", group)
    created = True
    call("SetInputMethodGroupInfo", group, "us", "[('keyboard-us', ''), ('schnellerTyp-e', '')]")
    env = {**os.environ, "QT_QPA_PLATFORM": "wayland", "QT_IM_MODULE": "fcitx", "GTK_IM_MODULE": "fcitx"}
    log = (build / "omarchy-scratch.log").open("w")
    logs.append(log)
    command = [str(build / "schnellertype_qt_probe"), str(state_path), *(["--manual"] if args.manual else [])]
    if args.browser:
        import websocket
        profile = Path(tempfile.mkdtemp(prefix="physical-chromium-", dir=build))
        page = profile / "scratch.html"
        page.write_text('<!doctype html><meta charset="utf-8"><title>Physical typing verification</title><h1>Type each example, then Tab</h1><label>1. tooi ddeens Berlin und moechte coffee, then Space and Tab<input id="normal" autofocus></label><label>2. hoom, then Space and Tab<input id="second"></label><label>3. Synthetic password only: hoom nuawx<input id="password" type="password"></label><style>body{font:18px sans-serif;padding:24px}label,input{display:block;margin:20px;width:90%;font-size:20px}</style>')
        command = ["chromium", "--ozone-platform=x11", "--gtk-version=4", f"--user-data-dir={profile}",
                   "--remote-debugging-port=0", "--no-first-run", "--no-default-browser-check",
                   "--disable-background-networking", "--disable-sync", "--disable-extensions",
                   "--password-store=basic", "--host-resolver-rules=MAP * ~NOTFOUND", page.as_uri()]
    app = subprocess.Popen(command, env=env, stdout=log, stderr=log)
    processes.append(app)
    if args.browser:
        port = wait(lambda: (profile / "DevToolsActivePort").read_text().splitlines()[0], bool)
        opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
        targets = wait(lambda: json.load(opener.open(f"http://127.0.0.1:{port}/json", timeout=2)), bool)
        target = next(item for item in targets if item.get("type") == "page")
        connection = websocket.create_connection(target["webSocketDebuggerUrl"], suppress_origin=True, timeout=5)
        counter = 0
        def read_state():
            global counter
            counter += 1
            connection.send(json.dumps({"id": counter, "method": "Runtime.evaluate", "params": {
                "expression": "Object.fromEntries(['normal','second','password'].map(id=>[id,document.getElementById(id).value]))", "returnByValue": True}}))
            while True:
                message = json.loads(connection.recv())
                if message.get("id") == counter:
                    return message["result"]["result"]["value"]
    wait(lambda: json.loads(run("hyprctl", "-j", "clients")),
         lambda windows: any(window.get("pid") == app.pid for window in windows))
    wait(read_state, lambda s: args.browser or s.get("focus") == "normal")
    run("hyprctl", "dispatch", f'hl.dsp.focus({{window = "pid:{app.pid}"}})')
    wait(lambda: json.loads(run("hyprctl", "-j", "activewindow")), lambda w: w.get("pid") == app.pid)
    call("SwitchInputMethodGroup", group)
    run("fcitx5-remote", "-s", "schnellerTyp-e")
    run("fcitx5-remote", "-o")
    wait(lambda: run("fcitx5-remote", "-n"), lambda im: im == "schnellerTyp-e")
    def send(command):
        values = command["text"] if "text" in command else [command["key"]]
        for value in values:
            if json.loads(run("hyprctl", "-j", "activewindow")).get("pid") != app.pid:
                raise RuntimeError("Scratch window lost focus; no further input will be sent")
            key = "space" if value == " " else value.lower() if len(value) == 1 else value
            mods = "CTRL" if command.get("ctrl") else "SHIFT" if len(value) == 1 and value.isupper() else ""
            for state in ("down", "up"):
                request = ("hl.dsp.send_key_state({mods = " + json.dumps(mods)
                           + ", key = " + json.dumps(key) + ", state = " + json.dumps(state)
                           + ", window = " + json.dumps(f"pid:{app.pid}") + "})")
                run("hyprctl", "dispatch", request)
                time.sleep(0.02)
        time.sleep(0.2)

    def check(label, field, text):
        observed = wait(read_state, lambda s: s.get(field) == text)
        results.append({"check": label, "field": field, "expected": text,
                        "actual": observed[field], "passed": True})
        print("PASS Omarchy installed addon:", label, flush=True)

    if args.manual:
        print("MANUAL READY: type the displayed examples in the three fields; only use the synthetic password shown.", flush=True)
        observed = wait(read_state,
                        lambda value: value["normal"].strip() == "tôi đến Berlin und möchte coffee"
                        and value["second"].strip() == "hôm" and value["password"] == "hoom nuawx", timeout=300)
        results.append({"check": "physical keyboard across normal, second and password fields",
                        "input": "manual", "client": "chromium-xwayland" if args.browser else "qt-wayland", "passed": True, "actual": observed})
        print("PASS physical keyboard verification", flush=True)
    else:
        send({"text": "tooi ddeens Berlin und moechte coffee "})
        check("mixed-language typing", "normal", "tôi đến Berlin und möchte coffee ")
        send({"text": "hoom"})
        send({"key": "Tab"})
        check("focus change commits once", "normal", "tôi đến Berlin und möchte coffee hôm")
        send({"text": "tooi "})
        check("independent second field", "second", "tôi ")
        send({"key": "Tab"})
        send({"text": "hoom nuawx"})
        check("password bypass", "password", "hoom nuawx")

except Exception as error:
    debug = GLib.Variant.parse(None, call("DebugInfo"), None, None).unpack()[0]
    print("\n".join(line for line in debug.splitlines() if "schnellertype" in line), flush=True)
    print("Controller:", run("fcitx5-remote", "-q"), run("fcitx5-remote", "-n"), run("fcitx5-remote"), flush=True)
    results.append({"passed": False, "error": str(error)})
    raise
finally:
    # Restore the existing group and activation state even if a check fails.
    try:
        call("SwitchInputMethodGroup", original_group)
        if original_im:
            run("fcitx5-remote", "-s", original_im)
        run("fcitx5-remote", "-o" if original_state == "2" else "-c")
        if created:
            call("RemoveInputMethodGroup", group)
        if original_config is not None:
            call("SetConfig", "fcitx://config/global", "<" + original_config.print_(True) + ">")
        call("Save")
    finally:
        if connection:
            connection.close()
        for process in reversed(processes):
            if process.poll() is None:
                process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        for log in logs:
            log.close()
        (build / ("omarchy-browser-physical.json" if args.browser else "omarchy-physical-verification.json" if args.manual else "omarchy-installed-verification.json")).write_text(json.dumps(results, indent=2, ensure_ascii=False))
