#include "maze_evaluator.h"

#include <QQueue>

#include <algorithm>
#include <limits>

EvalResult MazeEvaluator::evaluate(MazeModel &maze, const EvaluatorConfig &config) {
    EvalResult result;

    if (config.useSmartPlacement) {
        ResourcePlacer::placeSmart(maze, config.placerConfig);
    } else {
        maze.placeResources(config.placerConfig.coinCount,
                            config.placerConfig.trapCount,
                            config.placerConfig.seed);
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

    result.finalFitness = result.regretCombined * (1.0 + config.topoWeight * result.topoDifficulty);

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

