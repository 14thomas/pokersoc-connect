cmake_minimum_required(VERSION 3.16)

# Shipped exe is pokersoc_connect_<this>. Change it here.
set(POKERSOC_VERSION 2.0.0)

project(pokersoc_cpp VERSION ${POKERSOC_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Vendored SQLite (amalgamation) ----
add_library(sqlite3 STATIC third_party/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC third_party/sqlite)
if (MSVC)
    target_compile_options(sqlite3 PRIVATE /w)
    target_compile_definitions(sqlite3 PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(sqlite3 PRIVATE -w)
endif()

# ---- Core: domain + application services + persistence (no UI) ----
add_library(pokersoc_core STATIC
    src/domain/money/Money.cpp
    src/domain/money/Denomination.cpp
    src/domain/money/CashDenomination.cpp
    src/domain/money/ChipDenomination.cpp
    src/domain/money/DenominationTable.cpp
    src/domain/money/DenominationCount.cpp
    src/domain/cashbox/Cashbox.cpp
    src/domain/transaction/CashOut.cpp
    src/domain/transaction/BuyIn.cpp
    src/domain/transaction/CashboxTransactions.cpp
    src/domain/transaction/FoodSale.cpp
    src/domain/transaction/TournamentPayout.cpp
    src/domain/food/ProductCatalog.cpp
    src/domain/player/Attendance.cpp
    src/domain/player/Player.cpp
    src/domain/player/PlayerDirectory.cpp
    src/domain/session/CashGameMode.cpp
    src/domain/session/TournamentMode.cpp
    src/domain/session/TournamentConfig.cpp
    src/application/Ledger.cpp
    src/application/AdminService.cpp
    src/application/BuyInService.cpp
    src/application/CashOutService.cpp
    src/application/CashboxService.cpp
    src/application/FoodService.cpp
    src/application/ImportExportService.cpp
    src/application/PlayerService.cpp
    src/application/PrizeService.cpp
    src/application/ReportService.cpp
    src/persistence/AppStore.cpp
    src/persistence/SqliteStore.cpp
    src/util/Csv.cpp
    src/util/Json.cpp
    src/util/Time.cpp
)
target_include_directories(pokersoc_core PUBLIC src)
target_link_libraries(pokersoc_core PUBLIC sqlite3)

if (MSVC)
    target_compile_options(pokersoc_core PUBLIC /utf-8)
    target_compile_options(pokersoc_core PRIVATE /W4)
    target_compile_definitions(pokersoc_core PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
    target_compile_options(pokersoc_core PRIVATE -Wall -Wextra)
endif()

# ---- Qt Widgets front end ----
# MinGW kit example:
#   cmake -S . -B build-gui -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.9.2/mingw_64 -DCMAKE_BUILD_TYPE=Release
find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(pokersoc_gui WIN32
    src/ui/main.cpp
    src/ui/SessionContext.cpp
    src/ui/Style.cpp
    src/ui/Widgets.cpp
    src/ui/StartWindow.cpp
    src/ui/MainWindow.cpp
    src/ui/SettingsChrome.cpp
    src/ui/screens/BuyInScreen.cpp
    src/ui/screens/CashOutScreen.cpp
    src/ui/screens/CashboxScreen.cpp
    src/ui/screens/LostChipsScreen.cpp
    src/ui/screens/FoodScreen.cpp
    src/ui/screens/PlayersScreen.cpp
    src/ui/screens/SettingsScreen.cpp
    src/ui/screens/TournamentScreens.cpp
    src/ui/screens/TournamentSettingsScreen.cpp
    src/ui/screens/BreakdownScreen.cpp
    src/ui/board/TournamentClock.cpp
    src/ui/board/TournamentBoardWindow.cpp
    src/ui/MainWindow.h
    src/ui/SessionContext.h
    src/ui/StartWindow.h
    src/ui/Style.h
    src/ui/Widgets.h
    src/ui/SettingsChrome.h
    src/ui/screens/Screens.h
    src/ui/board/TournamentClock.h
    src/ui/board/TournamentBoardWindow.h
)
set_target_properties(pokersoc_gui PROPERTIES AUTOMOC ON)
target_link_libraries(pokersoc_gui PRIVATE pokersoc_core Qt6::Widgets)

file(GLOB_RECURSE POKERSOC_ASSET_FILES CONFIGURE_DEPENDS
    LIST_DIRECTORIES false
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/Assets/*")
qt_add_resources(pokersoc_gui pokersoc_assets
    PREFIX "/"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES ${POKERSOC_ASSET_FILES})

get_target_property(_qmake Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin "${_qmake}" DIRECTORY)
find_program(WINDEPLOYQT windeployqt HINTS "${_qt_bin}")
if (WINDEPLOYQT)
    add_custom_command(TARGET pokersoc_gui POST_BUILD
        COMMAND "${WINDEPLOYQT}" --no-translations --no-system-d3d-compiler
                --no-opengl-sw $<TARGET_FILE:pokersoc_gui>
        COMMENT "Deploying Qt runtime")
endif()

# Export wraps this stub around pokersoc_gui.exe plus the Qt libraries.
add_executable(pokersoc_launcher WIN32 src/pack/Launcher.cpp)
target_compile_features(pokersoc_launcher PRIVATE cxx_std_17)
if (MINGW)
    target_link_options(pokersoc_launcher PRIVATE -static)
endif()
