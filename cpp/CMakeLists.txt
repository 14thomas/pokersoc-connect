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
endif()

add_executable(pokersoc
    src/main.cpp
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
    src/ui/console/ConsoleIO.cpp
    src/ui/console/ConsoleApp.cpp
    src/ui/console/ConsoleAdmin.cpp
    src/ui/console/ConsoleCashbox.cpp
    src/ui/console/ConsoleFood.cpp
    src/ui/console/ConsolePlayers.cpp
)

if (MSVC)
    target_compile_options(pokersoc PRIVATE /W4 /utf-8)
    target_compile_definitions(pokersoc PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

target_include_directories(pokersoc PRIVATE src)
target_link_libraries(pokersoc PRIVATE sqlite3)
