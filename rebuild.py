#!/usr/bin/env python3
"""Rebuild pokersoc (core + Qt GUI) and refresh Assets.

Usage:
  python rebuild.py           # configure if needed, build everything
  python rebuild.py --run     # same, then launch the GUI
  python rebuild.py --clean   # wipe build-gui first, then rebuild
  python rebuild.py --gui     # build only pokersoc_gui
  python rebuild.py --export  # build, then write dist/pokersoc_connect_<version>.zip (one exe)
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import time
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
BUILD = ROOT / "build-gui"
ASSETS_SRC = ROOT / "Assets"
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


def app_version() -> str:
    text = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r"set\(POKERSOC_VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)\)", text)
    if not match:
        die("Set POKERSOC_VERSION in CMakeLists.txt, for example: set(POKERSOC_VERSION 2.0.0)")
    return match.group(1)


def export_package() -> None:
    """One exe. Assets and the Qt libraries are packed inside it."""
    version = app_version()
    export_name = f"pokersoc_connect_{version}.exe"
    gui = BUILD / "pokersoc_gui.exe"
    launcher = BUILD / "pokersoc_launcher.exe"
    if not gui.exists():
        die(f"GUI binary not found: {gui}")
    if not launcher.exists():
        die(f"Launcher not found: {launcher}")

    dist = ROOT / "dist"
    stage = dist / "_stage"
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir(parents=True)
    shutil.copy2(gui, stage / "pokersoc_gui.exe")
    for dll in BUILD.glob("*.dll"):
        shutil.copy2(dll, stage / dll.name)
    for name in (
        "generic",
        "iconengines",
        "imageformats",
        "networkinformation",
        "platforms",
        "styles",
        "tls",
    ):
        src = BUILD / name
        if not src.is_dir():
            die(f"deploy folder missing: {src}")
        shutil.copytree(src, stage / name)

    payload = dist / "_payload.zip"
    if payload.exists():
        payload.unlink()
    shutil.make_archive(str(payload.with_suffix("")), "zip", stage)
    payload_bytes = payload.read_bytes()
    bundled = dist / export_name
    bundled.write_bytes(launcher.read_bytes() + payload_bytes + b"POKERPK1" + len(payload_bytes).to_bytes(8, "little"))

    for stale in dist.glob("pokersoc_connect_*.exe"):
        if stale != bundled:
            stale.unlink()
    for stale in (dist / "pokersoc_gui.exe", dist / "PokerSoc.zip"):
        if stale.exists():
            stale.unlink()
    old_folder = dist / "PokerSoc"
    if old_folder.exists():
        shutil.rmtree(old_folder)
    shutil.rmtree(stage)
    payload.unlink()

    zip_path = dist / f"pokersoc_connect_{version}.zip"
    for stale in dist.glob("pokersoc_connect_*.zip"):
        if stale != zip_path:
            stale.unlink()
    if zip_path.exists():
        zip_path.unlink()
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        archive.write(bundled, export_name)
    print(f"Export exe: {bundled}", flush=True)
    print(f"Export zip: {zip_path} ({export_name} only)", flush=True)


def main() -> None:
    parser = argparse.ArgumentParser(description="Rebuild pokersoc GUI / console.")
    parser.add_argument("--run", action="store_true", help="launch pokersoc_gui after build")
    parser.add_argument("--clean", action="store_true", help="delete build-gui before rebuilding")
    parser.add_argument(
        "--gui",
        action="store_true",
        help="build only pokersoc_gui (default: build all targets)",
    )
    parser.add_argument(
        "--export",
        action="store_true",
        help="write dist/pokersoc_connect_<version>.zip containing only that exe",
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
    if args.export:
        build("pokersoc_launcher")
        export_package()
    if args.run:
        launch()


if __name__ == "__main__":
    main()
