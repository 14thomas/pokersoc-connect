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
    src/application/CashOutService.cpp
    src/application/CashboxService.cpp
    src/persistence/SqliteStore.cpp
)

target_include_directories(pokersoc PRIVATE src)
target_link_libraries(pokersoc PRIVATE sqlite3)
