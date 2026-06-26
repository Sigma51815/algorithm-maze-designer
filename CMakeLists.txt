cmake_minimum_required(VERSION 3.16)
project(maze_designer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# --- Qt6 (cross-platform: macOS / Windows / Linux) ---
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# --- Source files ---
add_executable(maze_designer
    src/include/core/maze.h
    src/include/game/bosssolver.h
    src/include/ui/mainwindow.h
    src/include/ui/mazewidget.h
    src/include/ui/battlewindow.h
    src/include/optimization/maze_optimizer.h
    src/include/io/maze_saver.h
    src/include/ai/greedy_player.h
    src/include/ai/aiplayerformat.h
    src/include/optimization/resource_placer.h
    src/include/optimization/maze_evaluator.h
    src/source/main.cpp
    src/source/core/maze.cpp
    src/source/game/bosssolver.cpp
    src/source/ui/mainwindow.cpp
    src/source/ui/mazewidget.cpp
    src/source/ui/battlewindow.cpp
    src/source/optimization/maze_optimizer.cpp
    src/source/io/maze_saver.cpp
    src/source/ai/greedy_player.cpp
    src/source/ai/aiplayerformat.cpp
    src/source/optimization/resource_placer.cpp
    src/source/optimization/maze_evaluator.cpp
)

target_include_directories(maze_designer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include/ai
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include/game
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include/io
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include/optimization
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include/ui
)
target_link_libraries(maze_designer PRIVATE Qt6::Widgets)
