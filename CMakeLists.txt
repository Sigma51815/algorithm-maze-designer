cmake_minimum_required(VERSION 3.21)

project(AlgorithmMazeDesigner VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(maze_designer
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/mazewidget.cpp
    src/mazewidget.h
    src/maze.cpp
    src/maze.h
    src/bosssolver.cpp
    src/bosssolver.h
)

target_link_libraries(maze_designer PRIVATE Qt6::Widgets)

if(MINGW)
    target_compile_options(maze_designer PRIVATE -Wall -Wextra -Wpedantic)
endif()
