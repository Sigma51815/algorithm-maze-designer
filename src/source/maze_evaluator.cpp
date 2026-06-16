#include "maze_evaluator.h"

#include <QQueue>

#include <algorithm>
#include <limits>

EvalResult MazeEvaluator::evaluate(MazeModel &maze, const EvaluatorConfig &config) {
    EvalResult result;

    if (config.useAdversarialPlacement) {
        ResourcePlacer::placeAdversarial(maze, config.placerConfig);
    } else if (config.useSmartPlacement) {
        ResourcePlacer::placeSmart(maze, config.placerConfig);
    }

    ResourcePlan dp = maze.optimalResourceWalk();
    result.dpScore = dp.maxValue;

    {
        const QVector<GreedyStrategy> strategies = {
            GreedyStrategy::ValuePerStep,
            GreedyStrategy::NearestFirst,
            GreedyStrategy::AvoidTraps,
            GreedyStrategy::EndGoalFirst
        };
        int worst = std::numeric_limits<int>::max();
        int best = std::numeric_limits<int>::min();
        for (GreedyStrategy s : strategies) {
            PlayResult r = GreedyPlayer::play(maze, {}, {}, 0, s);
            worst = std::min(worst, r.remainingResource);
            best = std::max(best, r.remainingResource);
        }
        result.worstGreedyScore = worst;
        result.bestGreedyScore = best;
    }
    result.regretGreedy = result.dpScore - result.worstGreedyScore;

    if (config.evaluateAgainstRL) {
        RLPlayer rlPlayer;
        rlPlayer.trainOnMaze(maze, config.rlConfig);
        RLPlayResult rlResult = rlPlayer.play(maze, config.rlConfig);
        result.rlScore = rlResult.totalResource;
        result.regretRL = result.dpScore - result.rlScore;
    }

    int worstAI = result.worstGreedyScore;
    if (config.evaluateAgainstRL) {
        worstAI = std::min(worstAI, result.rlScore);
    }
    result.regretCombined = result.dpScore - worstAI;

    result.topoDifficulty = computeTopoDifficulty(maze);

    if (config.useAdversarialPlacement) {
        // 对抗模式：直接最小化 AI 分数
        // fitness = -(AI worst score) + 拓扑难度加成
        // AI 分数越低（越负），fitness 越高
        result.finalFitness = -worstAI + config.topoWeight * result.topoDifficulty * 50;
    } else {
        result.finalFitness = result.regretCombined * (1.0 + config.topoWeight * result.topoDifficulty);
    }

    return result;
}

double MazeEvaluator::computeTopoDifficulty(const MazeModel &maze) {
    if (maze.cellCount() == 0) return 0.0;

    MazeStatistics stats = maze.statistics();

    double score = 0.0;

    double deadEndRatio = static_cast<double>(stats.deadEnds) / maze.cellCount();
    score += deadEndRatio * 2.0;

    double corridorNorm = std::min(1.0, stats.longestCorridor / 10.0);
    score += corridorNorm * 0.5;

    int shortestPath = 0;
    {
        QVector<int> dist(maze.cellCount(), -1);
        QQueue<int> queue;
        dist[maze.startCell()] = 0;
        queue.enqueue(maze.startCell());
        while (!queue.isEmpty()) {
            int cur = queue.dequeue();
            for (int next : maze.neighbors(cur)) {
                if (dist[next] < 0) {
                    dist[next] = dist[cur] + 1;
                    queue.enqueue(next);
                }
            }
        }
        shortestPath = dist[maze.endCell()];
    }
    double pathRatio = shortestPath > 0
        ? static_cast<double>(stats.solutionLength) / shortestPath
        : 1.0;
    score += (pathRatio - 1.0) * 0.5;

    double junctionRatio = static_cast<double>(stats.junctions) / maze.cellCount();
    score += junctionRatio * 1.0;

    return std::clamp(score, 0.0, 5.0);
}

int MazeEvaluator::evaluateGreedyWorst(const MazeModel &maze) {
    int worst = std::numeric_limits<int>::max();
    const QVector<GreedyStrategy> strategies = {
        GreedyStrategy::ValuePerStep,
        GreedyStrategy::NearestFirst,
        GreedyStrategy::AvoidTraps,
        GreedyStrategy::EndGoalFirst
    };
    for (GreedyStrategy s : strategies) {
        PlayResult result = GreedyPlayer::play(maze, {}, {}, 0, s);
        worst = std::min(worst, result.remainingResource);
    }
    return worst;
}

