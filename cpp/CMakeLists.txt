cmake_minimum_required(VERSION 3.16)

project(pokersoc_cpp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Vendored SQLite (amalgamation) ----
add_library(sqlite3 STATIC third_party/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC third_party/sqlite)
if (MSVC)
    target_compile_options(sqlite3 PRIVATE /w)             # silence C-library warnings
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

# ---- Console front end ----
add_executable(pokersoc
    src/main.cpp
    src/ui/console/ConsoleIO.cpp
    src/ui/console/ConsoleApp.cpp
    src/ui/console/ConsoleAdmin.cpp
    src/ui/console/ConsoleCashbox.cpp
    src/ui/console/ConsoleFood.cpp
    src/ui/console/ConsolePlayers.cpp
)
target_link_libraries(pokersoc PRIVATE pokersoc_core)
if (MSVC)
    target_compile_options(pokersoc PRIVATE /W4)
endif()

# ---- Qt Widgets front end (built only when Qt 6 is available) ----
# Qt here is the MinGW kit, so configure this in a separate MinGW build dir:
#   cmake -S . -B build-gui -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.9.2/mingw_64
find_package(Qt6 QUIET COMPONENTS Widgets)
if (Qt6_FOUND)
    add_executable(pokersoc_gui WIN32
        src/ui/qt/main.cpp
        src/ui/qt/SessionContext.cpp
        src/ui/qt/Style.cpp
        src/ui/qt/Widgets.cpp
        src/ui/qt/StartWindow.cpp
        src/ui/qt/MainWindow.cpp
        src/ui/qt/BuyInScreen.cpp
        src/ui/qt/CashOutScreen.cpp
        src/ui/qt/CashboxScreen.cpp
        src/ui/qt/LostChipsScreen.cpp
        src/ui/qt/FoodScreen.cpp
        src/ui/qt/PlayersScreen.cpp
        src/ui/qt/SettingsScreen.cpp
        src/ui/qt/TournamentScreens.cpp
        src/ui/qt/BreakdownScreen.cpp
        src/ui/qt/MainWindow.h
        src/ui/qt/Screens.h
        src/ui/qt/SessionContext.h
        src/ui/qt/StartWindow.h
        src/ui/qt/Style.h
        src/ui/qt/Widgets.h
    )
    set_target_properties(pokersoc_gui PROPERTIES AUTOMOC ON)
    target_link_libraries(pokersoc_gui PRIVATE pokersoc_core Qt6::Widgets)

    # Images used by the denomination buttons.
    add_custom_command(TARGET pokersoc_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/../Assets $<TARGET_FILE_DIR:pokersoc_gui>/Assets)

    # Copy the Qt (and MinGW runtime) DLLs next to the exe so it runs by double-click.
    get_target_property(_qmake Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin "${_qmake}" DIRECTORY)
    find_program(WINDEPLOYQT windeployqt HINTS "${_qt_bin}")
    if (WINDEPLOYQT)
        add_custom_command(TARGET pokersoc_gui POST_BUILD
            COMMAND "${WINDEPLOYQT}" --no-translations --no-system-d3d-compiler
                    --no-opengl-sw $<TARGET_FILE:pokersoc_gui>
            COMMENT "Deploying Qt runtime")
    endif()
endif()
