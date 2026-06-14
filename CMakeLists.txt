cmake_minimum_required(VERSION 3.21)

project(AlgorithmMazeDesigner VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(maze_designer
    src/source/main.cpp
    src/source/mainwindow.cpp
    src/source/mazewidget.cpp
    src/source/maze.cpp
    src/source/bosssolver.cpp
    src/source/battlewindow.cpp
    src/include/mainwindow.h
    src/include/mazewidget.h
    src/include/maze.h
    src/include/bosssolver.h
    src/include/battlewindow.h
)

target_include_directories(maze_designer PRIVATE src/include)
target_link_libraries(maze_designer PRIVATE Qt6::Widgets)

if(MINGW)
    target_compile_options(maze_designer PRIVATE -Wall -Wextra -Wpedantic)
endif()
