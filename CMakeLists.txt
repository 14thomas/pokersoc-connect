cmake_minimum_required(VERSION 3.21)
project(pokersoc-connect LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Let Qt's moc/uic/rcc run automatically
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt 6 (Widgets + Sql). Make sure CLion CMake options include:
#   -DCMAKE_PREFIX_PATH="C:/Qt/6.9.2/mingw_64"
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Sql)

# Sources
add_executable(pokersoc_connect
        src/main.cpp
        src/Database.h
        src/Database.cpp
        src/MainWindow.h
        src/MainWindow.cpp
)

# Link Qt
target_link_libraries(pokersoc_connect PRIVATE Qt6::Widgets Qt6::Sql)

# ----- Optional: copy schema.sql next to the built exe (handy for running from build dir)
add_custom_command(TARGET pokersoc_connect POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/schema.sql"
        "$<TARGET_FILE_DIR:pokersoc_connect>/schema.sql"
        COMMENT "Copying schema.sql next to the executable"
)

# ----- Deploy Qt runtime (ONLY for Release builds) -----
if (WIN32)
    # Your Qt 6.9.2 MinGW kit paths
    set(QT_BIN_DIR     "C:/Qt/6.9.2/mingw_64/bin")
    set(QT_PLUGIN_DIR  "C:/Qt/6.9.2/mingw_64/plugins")

    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")

    # Only run windeployqt for Release to avoid slowing down/failing Debug builds
    if (WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET pokersoc_connect POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --compiler-runtime
                --release
                --dir "$<TARGET_FILE_DIR:pokersoc_connect>"
                --plugindir "${QT_PLUGIN_DIR}"
                "$<TARGET_FILE:pokersoc_connect>"
                COMMENT "Running windeployqt to deploy Qt runtime (Release only)"
                VERBATIM
        )
    endif()
endif()
