cmake_minimum_required(VERSION 3.16)

project(pokersoc_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
)

target_include_directories(pokersoc PRIVATE src)
