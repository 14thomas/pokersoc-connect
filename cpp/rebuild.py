#!/usr/bin/env python3
"""Rebuild pokersoc (core, console, GUI) and refresh Assets.

Usage:
  python rebuild.py           # configure if needed, build everything
  python rebuild.py --run     # same, then launch the GUI
  python rebuild.py --clean   # wipe build-gui first, then rebuild
  python rebuild.py --gui     # build only pokersoc_gui
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent
BUILD = ROOT / "build-gui"
ASSETS_SRC = ROOT.parent / "Assets"
ASSETS_DST = BUILD / "Assets"

# Match run-gui.bat / the existing MinGW + Qt 6.9.2 kit on this machine.
QT_ROOT = Path(r"C:\Qt")
QT_PREFIX = QT_ROOT / "6.9.2" / "mingw_64"
PATH_PREFIX = [
    QT_ROOT / "Tools" / "CMake_64" / "bin",
    QT_ROOT / "Tools" / "Ninja",
    QT_ROOT / "Tools" / "mingw1310_64" / "bin",
    QT_PREFIX / "bin",
]


def die(msg: str, code: int = 1) -> None:
    print(f"error: {msg}", file=sys.stderr)
    raise SystemExit(code)


def run(cmd: list[str], *, cwd: Path | None = None) -> None:
    print("+", " ".join(cmd), flush=True)
    result = subprocess.run(cmd, cwd=cwd or ROOT)
    if result.returncode != 0:
        die(f"command failed ({result.returncode}): {' '.join(cmd)}", result.returncode)


def setup_env() -> None:
    missing = [p for p in PATH_PREFIX if not p.exists()]
    if missing:
        die(
            "Qt/MinGW tools not found:\n  "
            + "\n  ".join(str(p) for p in missing)
            + "\nUpdate PATH_PREFIX / QT_PREFIX in rebuild.py if your install differs."
        )
    prefixed = os.pathsep.join(str(p) for p in PATH_PREFIX)
    os.environ["PATH"] = prefixed + os.pathsep + os.environ.get("PATH", "")


def stop_gui() -> None:
    if os.name != "nt":
        return
    # Ignore failure if it isn't running.
    subprocess.run(
        ["taskkill", "/F", "/IM", "pokersoc_gui.exe"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    time.sleep(0.4)


def configure() -> None:
    if (BUILD / "CMakeCache.txt").exists():
        return
    print("Configuring build-gui …", flush=True)
    run(
        [
            "cmake",
            "-S",
            str(ROOT),
            "-B",
            str(BUILD),
            "-G",
            "Ninja",
            f"-DCMAKE_PREFIX_PATH={QT_PREFIX.as_posix()}",
            "-DCMAKE_BUILD_TYPE=Release",
        ]
    )


def build(target: str | None) -> None:
    cmd = ["cmake", "--build", str(BUILD)]
    if target:
        cmd += ["--target", target]
    run(cmd)


def sync_assets() -> None:
    if not ASSETS_SRC.is_dir():
        print(f"warning: assets folder missing: {ASSETS_SRC}", flush=True)
        return
    print(f"Syncing Assets -> {ASSETS_DST}", flush=True)
    run(["cmake", "-E", "copy_directory", str(ASSETS_SRC), str(ASSETS_DST)])


def launch() -> None:
    exe = BUILD / "pokersoc_gui.exe"
    if not exe.exists():
        die(f"GUI binary not found: {exe}")
    print(f"Launching {exe}", flush=True)
    subprocess.Popen([str(exe)], cwd=str(BUILD))


def main() -> None:
    parser = argparse.ArgumentParser(description="Rebuild pokersoc GUI / console.")
    parser.add_argument("--run", action="store_true", help="launch pokersoc_gui after build")
    parser.add_argument("--clean", action="store_true", help="delete build-gui before rebuilding")
    parser.add_argument(
        "--gui",
        action="store_true",
        help="build only pokersoc_gui (default: build all targets)",
    )
    args = parser.parse_args()

    setup_env()
    stop_gui()

    if args.clean and BUILD.exists():
        print(f"Removing {BUILD}", flush=True)
        shutil.rmtree(BUILD)

    configure()
    build("pokersoc_gui" if args.gui else None)
    sync_assets()

    print("Build OK.", flush=True)
    if args.run:
        launch()


if __name__ == "__main__":
    main()
