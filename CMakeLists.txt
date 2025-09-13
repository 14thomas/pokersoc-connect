cmake_minimum_required(VERSION 3.21)
project(pokersoc_connect LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 COMPONENTS Widgets Sql REQUIRED)

add_executable(pokersoc_connect
        src/main.cpp
)

target_link_libraries(pokersoc_connect PRIVATE Qt6::Widgets Qt6::Sql)
