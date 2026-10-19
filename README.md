# pokersoc-connect

Tournament and cash-game host app for PokerSoc (C++ / Qt).

## Layout

| Path | Purpose |
|------|---------|
| `src/` | Domain, services, Qt UI (`ui/screens`, `ui/board`) |
| `Assets/` | Brand, chips, fonts, blind-structure JSON |
| `third_party/` | Vendored SQLite |
| `rebuild.py` | Configure, build, sync assets, optional launch |

## Build (Windows, Qt 6 MinGW)

```bash
python rebuild.py          # build everything
python rebuild.py --run    # build and launch the GUI
python rebuild.py --gui    # GUI target only
python rebuild.py --clean  # wipe build-gui, then rebuild
```

Or manually:

```bash
cmake -S . -B build-gui -G Ninja ^
  -DCMAKE_PREFIX_PATH=C:/Qt/6.9.2/mingw_64 ^
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-gui --target pokersoc_gui
```

Requires Qt 6 + MinGW (paths match `rebuild.py` / `run-gui.bat`).

## License

See [LICENSE](LICENSE).
