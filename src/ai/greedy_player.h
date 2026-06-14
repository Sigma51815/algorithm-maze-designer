#pragma once

#include "core/maze.h"

#include <QVector>

struct PlayResult {
    double score = 0.0;
    int remainingResource = 0;
    int collectedCoins = 0;
    int triggeredTraps = 0;
    int totalSteps = 0;
    QVector<int> path;
    bool reachedEnd = false;
};

class GreedyPlayer {
public:
    static PlayResult play(MazeModel &maze);

private:
    static QVector<int> observeVisible(const MazeModel &maze, int pos);
    static int bfsDistance(const MazeModel &maze, int from, int to);
    static QVector<int> bfsPath(const MazeModel &maze, int from, int to);
};
