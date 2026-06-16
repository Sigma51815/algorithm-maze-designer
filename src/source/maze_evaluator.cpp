#include "maze_evaluator.h"

#include <QQueue>

#include <algorithm>
#include <limits>

EvalResult MazeEvaluator::evaluate(MazeModel &maze, const EvaluatorConfig &config) {
    EvalResult result;

    if (!config.skipPlacement) {
        if (config.useAdversarialPlacement) {
            ResourcePlacer::placeAdversarial(maze, config.placerConfig);
        } else if (config.useSmartPlacement) {
            ResourcePlacer::placeSmart(maze, config.placerConfig);
        }
    }

    ResourcePlan dp = maze.optimalResourceWalk();
    result.dpScore = dp.maxValue;

    // Count total coins / traps in the maze.
    int totalCoins = 0, totalTraps = 0;
    for (int cell = 0; cell < maze.cellCount(); ++cell) {
        int val = maze.resourceAt(cell);
        if (val > 0) ++totalCoins;
        else if (val < 0) ++totalTraps;
    }

    // Shortest path length (BFS, used for pathInefficiency).
    int shortestPathLen = 0;
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
        shortestPathLen = dist[maze.endCell()] > 0 ? dist[maze.endCell()] : 1;
    }

    {
        const QVector<GreedyStrategy> strategies = {
            GreedyStrategy::ValuePerStep,
            GreedyStrategy::NearestFirst,
            GreedyStrategy::AvoidTraps,
            GreedyStrategy::EndGoalFirst
        };
        int worst = std::numeric_limits<int>::max();
        int best = std::numeric_limits<int>::min();
        double worstAIScore = 0.0;
        double worstMissRate = 0.0;
        double worstHitRate = 0.0;
        double worstInefficiency = 0.0;
        for (GreedyStrategy s : strategies) {
            PlayResult r = GreedyPlayer::play(maze, {}, {}, 0, s);
            if (r.remainingResource < worst) {
                worst = r.remainingResource;
                worstAIScore = r.totalSteps > 0
                    ? static_cast<double>(r.remainingResource) / r.totalSteps
                    : 0.0;
            }
            best = std::max(best, r.remainingResource);

            double miss = totalCoins > 0
                ? static_cast<double>(totalCoins - r.collectedCoins) / totalCoins : 0.0;
            double hit  = totalTraps > 0
                ? static_cast<double>(r.triggeredTraps) / totalTraps : 0.0;
            double ineff = r.totalSteps > shortestPathLen
                ? static_cast<double>(r.totalSteps - shortestPathLen) / shortestPathLen : 0.0;
            if (miss > worstMissRate) worstMissRate = miss;
            if (hit  > worstHitRate)  worstHitRate  = hit;
            if (ineff > worstInefficiency) worstInefficiency = ineff;
        }
        result.worstGreedyScore = worst;
        result.bestGreedyScore = best;
        result.worstAIScore = worstAIScore;
        result.coinMissRate = worstMissRate;
        result.trapHitRate = worstHitRate;
        result.pathInefficiency = worstInefficiency;
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

    // Composite difficulty score (0–100 scale) — same formula for all modes.
    result.finalFitness = result.coinMissRate * 50.0
                          + result.trapHitRate * 30.0
                          + result.pathInefficiency * 20.0
                          + config.topoWeight * result.topoDifficulty * 50.0;

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

