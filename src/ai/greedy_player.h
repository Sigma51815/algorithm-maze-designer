#pragma once

#include "bosssolver.h"
#include "maze.h"

struct PlayResult {
    bool reachedEnd = false;
    int totalSteps = 0;
    int remainingResource = 0;
    double score = 0.0;
    int collectedCoins = 0;
    int triggeredTraps = 0;
    QVector<int> walk;
    bool bossDefeated = false;
    int bossAttempts = 0;
};

class GreedyPlayer {
public:
    static PlayResult play(const MazeModel &maze,
                           const QVector<int> &bossHealth = {},
                           const QVector<BossSkill> &bossSkills = {},
                           int initialResource = 0);
};
