#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise package transactions in a fresh pacman root, without host changes."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--old", type=Path, required=True)
parser.add_argument("--new", type=Path, required=True)
parser.add_argument("--output", type=Path, default=Path("build-native"))
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
root = Path(tempfile.mkdtemp(prefix="package-lifecycle-", dir=args.output.resolve()))
(root / "var/lib/pacman").mkdir(parents=True)
(root / "var/cache/pacman/pkg").mkdir(parents=True)
config = root / "pacman.conf"
config.write_text("[options]\nArchitecture=auto\nSigLevel=Never\n")
preferences = root / "home/test/.config/schnellerTyp-e/schnellerTyp-e.conf"
preferences.parent.mkdir(parents=True)
preferences.write_text("[app]\nlanguage=vi\n")
records = []

def metadata(path):
    text = subprocess.check_output(["bsdtar", "-xOf", str(path), ".PKGINFO"], text=True)
    return dict(line.split(" = ", 1) for line in text.splitlines() if " = " in line)

old, new = args.old.resolve(), args.new.resolve()
old_info, new_info = metadata(old), metadata(new)
assert old_info["pkgname"] == new_info["pkgname"] == "schnellertype-e-fcitx5"
assert old_info["pkgver"] != new_info["pkgver"]
base = ["pacman", "--config", str(config), "--root", str(root), "--dbpath", str(root / "var/lib/pacman"),
        "--cachedir", str(root / "var/cache/pacman/pkg"), "--logfile", str(root / "pacman.log")]
try:
    for label, package, info in [("install", old, old_info), ("upgrade", new, new_info),
                                 ("reinstall", new, new_info), ("rollback", old, old_info),
                                 ("upgrade after rollback", new, new_info)]:
        subprocess.run(["pkexec", *base, "-U", "--noconfirm", "-dd", "--noscriptlet", str(package)], check=True)
        version = subprocess.check_output([*base, "-Q", info["pkgname"]], text=True).strip().split()[1]
        assert version == info["pkgver"]
        subprocess.run([*base, "-Qkk", info["pkgname"]], check=True)
        assert preferences.read_text() == "[app]\nlanguage=vi\n"
        records.append({"check": label, "version": version, "passed": True})
    subprocess.run(["pkexec", *base, "-R", "--noconfirm", "--noscriptlet", old_info["pkgname"]], check=True)
    assert not (root / "usr/lib/fcitx5/schnellerTyp-e-fcitx5.so").exists()
    assert preferences.read_text() == "[app]\nlanguage=vi\n"
    records.append({"check": "remove preserves user preferences", "passed": True})
except Exception as error:
    records.append({"passed": False, "error": str(error)})
    raise
finally:
    (root / "results.json").write_text(json.dumps(records, indent=2) + "\n")
    print("Lifecycle results:", root / "results.json", flush=True)
