cmake_minimum_required(VERSION 3.16)
project(maze_designer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(maze_designer
    src/main.cpp
    src/core/maze.cpp
    src/core/maze.h
    src/core/bosssolver.cpp
    src/core/bosssolver.h
    src/gui/mainwindow.cpp
    src/gui/mainwindow.h
    src/gui/mazewidget.cpp
    src/gui/mazewidget.h
    src/ai/greedy_player.cpp
    src/ai/greedy_player.h
)

target_include_directories(maze_designer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(maze_designer PRIVATE Qt6::Widgets)
