cmake_minimum_required(VERSION 3.16)
project(maze_designer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(maze_designer
    src/include/maze.h
    src/include/bosssolver.h
    src/include/mainwindow.h
    src/include/mazewidget.h
    src/include/battlewindow.h
    src/include/maze_optimizer.h
    src/include/qlearning_optimizer.h
    src/include/ai/greedy_player.h
    src/include/ai/aiplayerformat.h
    src/include/ai/rl_player.h
    src/include/coevolution.h
    src/source/main.cpp
    src/source/maze.cpp
    src/source/bosssolver.cpp
    src/source/mainwindow.cpp
    src/source/mazewidget.cpp
    src/source/battlewindow.cpp
    src/source/maze_optimizer.cpp
    src/source/qlearning_optimizer.cpp
    src/source/ai/greedy_player.cpp
    src/source/ai/aiplayerformat.cpp
    src/source/ai/rl_player.cpp
    src/source/coevolution.cpp
)

target_include_directories(maze_designer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include
)
target_link_libraries(maze_designer PRIVATE Qt6::Widgets)
