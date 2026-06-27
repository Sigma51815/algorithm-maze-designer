#pragma once

#include "ai/greedy_player.h"
#include "maze.h"
#include "resource_placer.h"

struct EvalResult {
    int dpScore = 0;
    int worstGreedyScore = 0;
    int bestGreedyScore = 0;
    int regretGreedy = 0;
    int regretCombined = 0;
    double worstAIScore = 0.0;
    double coinMissRate = 0.0;
    double trapHitRate = 0.0;
    double pathInefficiency = 0.0;
    double topoDifficulty = 0;
    double finalFitness = 0;
    double designDiscrimination = 0.0;
    double designStability = 0.0;
    double designBalance = 0.0;
    double designGroupScore = 0.0;
    // 诊断字段：归一化AI得分的均值与极差（仅新公式下有意义）
    double meanAIScoreRatio = 0.0;
    double aiScoreSpread = 0.0;
    int evaluatedAiCount = 0;
    int reachedAiCount = 0;
};

struct EvaluatorConfig {
    bool useSmartPlacement = true;
    bool useAdversarialPlacement = false;
    bool skipPlacement = false;
    ResourcePlacerConfig placerConfig;
    double topoWeight = 0.3;
};

class MazeEvaluator {
public:
    static EvalResult evaluate(MazeModel &maze, const EvaluatorConfig &config);
    static double computeTopoDifficulty(const MazeModel &maze);
    static int evaluateGreedyWorst(const MazeModel &maze);
};
