#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Opt-in scratch tests on an isolated headless Wayland/XWayland desktop."""
import argparse
import json
import os
from pathlib import Path
import shlex
import select
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.request


def wait_for(read, predicate=bool, timeout=15):
    deadline = time.monotonic() + timeout
    last = None
    while time.monotonic() < deadline:
        try:
            last = read()
            if predicate(last):
                return last
        except (OSError, ValueError, KeyError):
            pass
        time.sleep(0.05)
    raise RuntimeError(f"Timed out waiting for expected state; last observation: {last!r}")


def session(build, output, native_protocol=False, clients=None, installed=False):
    import websocket  # Development-only dependency, never part of the addon.

    processes = []
    logs = []
    results = []
    runtime_directory = tempfile.TemporaryDirectory(prefix="st-desktop-", ignore_cleanup_errors=True)
    runtime = Path(runtime_directory.name)
    env = os.environ.copy()
    for name in ("DISPLAY", "WAYLAND_DISPLAY", "SWAYSOCK", "HYPRLAND_INSTANCE_SIGNATURE",
                 "FCITX_CONFIG_HOME", "FCITX_CONFIG_DIRS", "FCITX_DATA_HOME"):
        env.pop(name, None)
    env.update(XDG_RUNTIME_DIR=str(runtime), XDG_CONFIG_HOME=str(output / "config"),
               XDG_CACHE_HOME=str(output / "cache"), XDG_DATA_HOME=str(output / "data"),
               XDG_DATA_DIRS="/usr/local/share:/usr/share" if installed else f"{build / 'stage/usr/share'}:/usr/local/share:/usr/share",
               FCITX_ADDON_DIRS="/usr/lib/fcitx5" if installed else f"{build / 'stage/usr/lib/fcitx5'}:/usr/lib/fcitx5",
               WLR_BACKENDS="headless", WLR_RENDERER="pixman", QT_IM_MODULE="fcitx",
               GTK_IM_MODULE="fcitx", XMODIFIERS="@im=fcitx", NO_AT_BRIDGE="1",
               XDG_CURRENT_DESKTOP="sway", QT_QPA_PLATFORMTHEME="", GTK_USE_PORTAL="0")
    config = output / "sway.conf"
    config.write_text("output HEADLESS-1 resolution 1280x800\nxwayland force\nseat seat0 fallback true\n")
    fcitx_config = output / "config/fcitx5"
    fcitx_config.mkdir(parents=True)
    (fcitx_config / "config").write_text("[Behavior]\nActiveByDefault=True\nShareInputState=All\n")
    (fcitx_config / "profile").write_text("""[Groups/0]
Name=Default
Default Layout=us
DefaultIM=schnellerTyp-e

[Groups/0/Items/0]
Name=keyboard-us
Layout=

[Groups/0/Items/1]
Name=schnellerTyp-e
Layout=

[GroupOrder]
0=Default
""")

    def start(command, name):
        log = (output / f"{name}.log").open("w")
        logs.append(log)
        process = subprocess.Popen(command, env=env, stdout=log, stderr=log)
        processes.append(process)
        return process

    def run(*command):
        return subprocess.run(command, env=env, check=True, text=True,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout

    keyboard = None

    def send(command):
        keyboard.stdin.write(json.dumps(command) + "\n")
        keyboard.stdin.flush()
        if not select.select([keyboard.stdout], [], [], 20)[0] or keyboard.stdout.readline().strip() != "ok":
            raise RuntimeError("Private keyboard failed to acknowledge input")

    def key(name):
        send({"key": name})
        time.sleep(0.1)

    def type_text(text):
        send({"text": text})
        time.sleep(0.2)

    def clear():
        send({"key": "a", "ctrl": True})
        key("BackSpace")

    try:
        sway = start(["sway", "-c", str(config)], "sway")
        socket = wait_for(lambda: list(runtime.glob("sway-ipc.*.sock")))[0]
        env["SWAYSOCK"] = str(socket)
        env["WAYLAND_DISPLAY"] = wait_for(lambda: [p.name for p in runtime.glob("wayland-*")
                                                   if not p.name.endswith(".lock")])[0]
        keyboard_log = (output / "keyboard.log").open("w")
        logs.append(keyboard_log)
        keyboard = subprocess.Popen([str(build / "schnellertype_wayland_keyboard")], env=env,
                                    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=keyboard_log, text=True)
        processes.append(keyboard)
        if not select.select([keyboard.stdout], [], [], 10)[0] or keyboard.stdout.readline().strip() != "ready":
            raise RuntimeError("Private keyboard failed to initialize")
        display_file = output / "display.json"
        script = "import os,json,pathlib; pathlib.Path(" + repr(str(display_file)) + ").write_text(json.dumps({k:os.environ.get(k,'') for k in ['DISPLAY','WAYLAND_DISPLAY']}))"
        run("swaymsg", "exec", shlex.join([sys.executable, "-c", script]))
        env.update(wait_for(lambda: json.loads(display_file.read_text())))
        run("dbus-update-activation-environment", "--all")
        fcitx = start(["fcitx5", "--disable=notificationitem,notifications,classicui"], "fcitx5")
        wait_for(lambda: run("gdbus", "call", "--session", "--dest", "org.freedesktop.DBus",
                            "--object-path", "/org/freedesktop/DBus", "--method",
                            "org.freedesktop.DBus.NameHasOwner", "org.fcitx.Fcitx5"),
                 lambda value: "true" in value)
        if fcitx.poll() is not None:
            raise RuntimeError("Private Fcitx process failed to start")
        page = output / "scratch.html"
        page.write_text('''<!doctype html><meta charset="utf-8"><title>schnellerTyp-e scratch</title>
<input id="normal" autofocus><input id="second"><input id="password" type="password">
<style>input{display:block;margin:30px;font-size:24px;width:90%}</style>''')
        for name, command in [
            ("gtk-wayland", ["env", "GDK_BACKEND=wayland", f"GTK_IM_MODULE={'wayland' if native_protocol else 'fcitx'}", str(build / "schnellertype_gtk_probe")]),
            ("qt-wayland", ["env", "QT_QPA_PLATFORM=wayland", str(build / "schnellertype_qt_probe")]),
            ("qt-xwayland", ["env", "QT_QPA_PLATFORM=xcb", str(build / "schnellertype_qt_probe")]),
            ("chromium-wayland", None),
            ("chromium-xwayland", None),
        ]:
            if clients and name not in clients:
                continue
            state_file = output / f"{name}.json"
            browser = name.startswith("chromium")
            connection = None
            if browser:
                profile = output / name
                command = ["chromium", f"--user-data-dir={profile}", "--no-first-run",
                           "--no-default-browser-check", "--disable-background-networking",
                           "--disable-sync", "--disable-extensions", "--password-store=basic",
                           "--host-resolver-rules=MAP * ~NOTFOUND",
                           "--remote-debugging-port=0", "--disable-dev-shm-usage",
                           f"--ozone-platform={'wayland' if name.endswith('-wayland') else 'x11'}",
                           *(["--enable-wayland-ime", "--wayland-text-input-version=3"] if native_protocol
                             else ["--gtk-version=4"]), page.as_uri()]
            else:
                command.append(str(state_file))
            application = start(command, name)
            if browser:
                port = wait_for(lambda: (profile / "DevToolsActivePort").read_text().splitlines())[0]
                opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
                targets = wait_for(lambda: json.load(opener.open(f"http://127.0.0.1:{port}/json", timeout=2)))
                target = next(t for t in targets if t.get("type") == "page")
                connection = websocket.create_connection(target["webSocketDebuggerUrl"],
                                                          suppress_origin=True, timeout=5)
                counter = 0

                def evaluate(expression):
                    nonlocal counter
                    counter += 1
                    connection.send(json.dumps({"id": counter, "method": "Runtime.evaluate",
                                                "params": {"expression": expression, "returnByValue": True}}))
                    while True:
                        reply = json.loads(connection.recv())
                        if reply.get("id") == counter:
                            return reply["result"]["result"].get("value")

                def state():
                    return evaluate("Object.fromEntries(['normal','second','password'].map(id=>[id,document.getElementById(id).value]).concat([['focus',document.activeElement.id]]))")
                evaluate("document.getElementById('normal').focus()")
            else:
                def state():
                    return json.loads(state_file.read_text())
            wait_for(state, lambda s: s and s.get("focus") == "normal")
            run("fcitx5-remote", "-o")
            run("fcitx5-remote", "-s", "schnellerTyp-e")
            wait_for(lambda: run("fcitx5-remote", "-n").strip(), lambda value: value == "schnellerTyp-e")
            wait_for(lambda: run("fcitx5-remote").strip(), lambda value: value == "2")

            def check(label, field, expected):
                actual = wait_for(state, lambda s: s.get(field) == expected, timeout=3)
                results.append({"client": name, "transport": "native-protocol" if native_protocol else "toolkit-module", "check": label, "field": field, "expected": expected,
                                "actual": actual[field], "passed": True})
                print(f"PASS {name}: {label}", flush=True)

            for label, raw, expected in [
                ("mixed languages", "tooi ddeens Berlin und moechte coffee ", "tôi đến Berlin und möchte coffee "),
                ("delayed Telex", "nuawx ", "nữa "),
                ("literal undo", "ww ", "w "),
                ("English and identifiers", "This is coffee test@example.com ", "This is coffee test@example.com "),
            ]:
                clear()
                type_text(raw)
                check(label, "normal", expected)
            clear()
            type_text("hoom")
            key("Escape")
            type_text(" ")
            check("Escape restores literal input", "normal", "hoom ")
            clear()
            type_text("hoom")
            key("BackSpace")
            type_text("m ")
            check("Backspace edits preedit", "normal", "hôm ")
            clear()
            type_text("hoom")
            key("Return")
            check("Enter commits once", "normal", "hôm")
            clear()
            type_text("hoom")
            key("Tab")
            check("focus loss commits once", "normal", "hôm")
            type_text("tooi ")
            check("second field is independent", "second", "tôi ")
            key("Tab")
            type_text("hoom nuawx")
            check("password bypass", "password", "hoom nuawx")
            send({"key": "Tab", "shift": True})
            send({"key": "Tab", "shift": True})
            time.sleep(0.2)
            clear()
            type_text("hoom ")
            check("return from password field", "normal", "hôm ")
            run("fcitx5-remote", "-c")
            clear()
            type_text("hoom ")
            check("Fcitx pause", "normal", "hoom ")
            run("fcitx5-remote", "-o")
            clear()
            type_text("hoom ")
            check("Fcitx resume", "normal", "hôm ")
            clear()
            type_text("tooi ddeens Berlin und moechte coffee " * 20)
            check("20-sentence typing", "normal", "tôi đến Berlin und möchte coffee " * 20)
            if connection:
                connection.close()
            run("swaymsg", "kill")
            try:
                application.wait(timeout=5)
            except subprocess.TimeoutExpired:
                application.terminate()
                application.wait(timeout=5)
            time.sleep(0.3)
        if sway.poll() is not None:
            raise RuntimeError("Test compositor exited unexpectedly")
    except Exception as error:
        results.append({"passed": False, "error": str(error)})
        raise
    finally:
        (output / "results.json").write_text(json.dumps(results, indent=2, ensure_ascii=False))
        if env.get("SWAYSOCK"):
            subprocess.run(["swaymsg", "exit"], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
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
        try:
            runtime_directory.cleanup()
        except PermissionError:
            pass  # The private portal's FUSE mount disappears when its bus exits.


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path("build-native"))
    parser.add_argument("--installed", action="store_true", help="Test the pacman-installed addon instead of staging the build")
    parser.add_argument("--clients", nargs="+", choices=["gtk-wayland", "qt-wayland", "qt-xwayland", "chromium-wayland", "chromium-xwayland"])
    parser.add_argument("--native-wayland-protocol", action="store_true",
                        help="Exercise the experimental native text-input protocol instead of toolkit modules")
    parser.add_argument("--session", type=Path, help=argparse.SUPPRESS)
    args = parser.parse_args()
    build = args.build_dir.resolve()
    if args.session:
        if (os.environ.get("SCHNELLERTYPE_TEST_OUTPUT") != str(args.session.resolve())
            or os.environ.get("DBUS_SESSION_BUS_ADDRESS") == os.environ.get("SCHNELLERTYPE_ORIGINAL_BUS")):
            parser.error("Internal session mode requires the harness's private D-Bus session")
        session(build, args.session.resolve(), args.native_wayland_protocol, args.clients, args.installed)
        return
    for command in ("sway", "swaymsg", "fcitx5", "fcitx5-remote", "chromium", "dbus-run-session"):
        if not shutil.which(command):
            parser.error(f"Missing development tool: {command}")
    for probe in ("schnellertype_qt_probe", "schnellertype_gtk_probe", "schnellertype_wayland_keyboard"):
        if not (build / probe).is_file():
            parser.error("Build with SCHNELLERTYPE_BUILD_LINUX_DESKTOP_PROBES=ON first")
    if not args.installed:
        subprocess.run(["cmake", "--install", str(build)], check=True,
                       env={**os.environ, "DESTDIR": str(build / "stage")})
    output = Path(tempfile.mkdtemp(prefix="desktop-verification-", dir=build))
    print(f"Scratch desktop logs and results: {output}", flush=True)
    result = subprocess.run(["dbus-run-session", "--", sys.executable, str(Path(__file__).resolve()),
                             "--build-dir", str(build), "--session", str(output),
                             *(["--native-wayland-protocol"] if args.native_wayland_protocol else []),
                             *(["--clients", *args.clients] if args.clients else []),
                             *(["--installed"] if args.installed else [])],
                            env={**os.environ, "SCHNELLERTYPE_TEST_OUTPUT": str(output),
                                 "SCHNELLERTYPE_ORIGINAL_BUS": os.environ.get("DBUS_SESSION_BUS_ADDRESS", "")})
    raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
