@echo off
setlocal
cd /d "%~dp0"
if not exist "build-gui\pokersoc_gui.exe" (
  echo Building the GUI first...
  set "PATH=C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.9.2\mingw_64\bin;%PATH%"
  if not exist build-gui (
    cmake -S . -B build-gui -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.9.2/mingw_64 -DCMAKE_BUILD_TYPE=Release
  )
  cmake --build build-gui --target pokersoc_gui
)
start "" "%~dp0build-gui\pokersoc_gui.exe"
