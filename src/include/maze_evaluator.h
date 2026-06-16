#pragma once

#include "ai/greedy_player.h"
#include "ai/rl_player.h"
#include "maze.h"
#include "resource_placer.h"

struct EvalResult {
    int dpScore = 0;
    int worstGreedyScore = 0;
    int bestGreedyScore = 0;
    int rlScore = 0;
    int regretGreedy = 0;
    int regretRL = 0;
    int regretCombined = 0;
    double topoDifficulty = 0;
    double finalFitness = 0;
};

struct EvaluatorConfig {
    bool useSmartPlacement = true;
    bool useAdversarialPlacement = false;
    ResourcePlacerConfig placerConfig;
    bool evaluateAgainstRL = false;
    RLConfig rlConfig;
    double topoWeight = 0.3;
};

class MazeEvaluator {
public:
    static EvalResult evaluate(MazeModel &maze, const EvaluatorConfig &config);
    static double computeTopoDifficulty(const MazeModel &maze);
    static int evaluateGreedyWorst(const MazeModel &maze);
};
