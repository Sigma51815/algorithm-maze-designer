#include "maze_evaluator.h"

#include <QQueue>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

double meanOf(const QVector<double> &values) {
    if (values.isEmpty()) return 0.0;
    double sum = 0.0;
    for (double value : values) sum += value;
    return sum / values.size();
}

double stddevOf(const QVector<double> &values, double mean) {
    if (values.size() < 2) return 0.0;
    double sum = 0.0;
    for (double value : values) {
        const double delta = value - mean;
        sum += delta * delta;
    }
    return std::sqrt(sum / values.size());
}

} // namespace

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
        int reachedCount = 0;
        QVector<double> aiRatios;
        QVector<double> aiResourceRatios;
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

            if (r.reachedEnd) ++reachedCount;
            const double stepRatio = r.totalSteps > 0
                ? static_cast<double>(r.remainingResource) / r.totalSteps
                : 0.0;
            aiRatios.append(stepRatio);
            const double resourceRatio = result.dpScore > 0
                ? static_cast<double>(std::max(0, r.remainingResource)) / result.dpScore
                : 0.0;
            aiResourceRatios.append(clamp01(resourceRatio));
        }
        result.worstGreedyScore = worst;
        result.bestGreedyScore = best;
        result.worstAIScore = worstAIScore;
        result.coinMissRate = worstMissRate;
        result.trapHitRate = worstHitRate;
        result.pathInefficiency = worstInefficiency;

        const double meanRatio = meanOf(aiRatios);
        const double ratioStddev = stddevOf(aiRatios, meanRatio);
        const double meanResourceRatio = meanOf(aiResourceRatios);
        const double resourceStddev = stddevOf(aiResourceRatios, meanResourceRatio);

        // Cross-validation proxy for design groups:
        // D: distinguish AI players, C: avoid pathological/no-finish maps,
        // B: keep the maze challenging but still playable.
        result.designDiscrimination = clamp01(resourceStddev / 0.35);
        result.designStability = clamp01(static_cast<double>(reachedCount) / strategies.size());
        const double targetMeanRatio = 0.45;
        const double targetSpread = 0.18;
        result.designBalance = clamp01(1.0
            - std::abs(meanResourceRatio - targetMeanRatio) / targetSpread);
        const double geometric = std::cbrt(std::max(0.0,
            result.designDiscrimination * result.designStability * result.designBalance));
        result.designGroupScore = 60.0 + geometric * 40.0;
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

    result.finalFitness = result.designGroupScore;

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

