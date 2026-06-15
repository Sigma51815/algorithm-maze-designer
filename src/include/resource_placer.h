#pragma once

#include "maze.h"

#include <random>

struct ResourcePlacerConfig {
    int coinCount = 8;
    int trapCount = 5;
    double deadEndCoinRatio = 0.5;
    double mainPathTrapRatio = 0.3;
    double branchTrapRatio = 0.4;
    quint32 seed = 42;
};

class ResourcePlacer {
public:
    static void placeSmart(MazeModel &maze, const ResourcePlacerConfig &config);
    static void placeRandom(MazeModel &maze, int coinCount, int trapCount, quint32 seed);
};
