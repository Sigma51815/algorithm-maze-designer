#include "maze_evaluator.h"

#include <QQueue>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr double kStddevNorm   = 0.22;
constexpr double kRangeNorm    = 0.55;
constexpr double kBalanceIdeal = 0.45;
constexpr double kBalanceTol   = 0.30;

constexpr double kWeightD = 0.55;
constexpr double kWeightB = 0.30;
constexpr double kWeightC = 0.15;

static_assert(kStddevNorm > 0.0 && kStddevNorm <= 1.0);
static_assert(kRangeNorm  > 0.0 && kRangeNorm  <= 1.0);
static_assert(kBalanceIdeal >= 0.0 && kBalanceIdeal <= 1.0);
static_assert(kBalanceTol > 0.0 && kBalanceTol <= 1.0);
static_assert(kWeightD + kWeightB + kWeightC > 0.99 &&
              kWeightD + kWeightB + kWeightC < 1.01);

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
            GreedyStrategy::CautiousCollector,
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
        // 存储每个AI的原始数据，供新公式使用
        struct AiRaw {
            int remainingResource = 0;
            int totalSteps = 0;
            bool reachedEnd = false;
        };
        QVector<AiRaw> aiRawResults;
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
            aiRawResults.append({r.remainingResource, r.totalSteps, r.reachedEnd});
        }
        result.worstGreedyScore = worst;
        result.bestGreedyScore = best;
        result.worstAIScore = worstAIScore;
        result.coinMissRate = worstMissRate;
        result.trapHitRate = worstHitRate;
        result.pathInefficiency = worstInefficiency;

        // ── 新公式（Codex 对齐）：归一化AI得分 → D/B/C → 加权适应度 ──
        // 理论最大效率 = dpScore / shortestPathLen（每步最优得分）
        const double idealScore = (result.dpScore > 0 && shortestPathLen > 0)
            ? static_cast<double>(result.dpScore) / shortestPathLen
            : 1.0;

        QVector<double> normalizedScores;
        for (const auto &ai : aiRawResults) {
            double ns = 0.0;
            if (idealScore > 0 && ai.totalSteps > 0 && ai.reachedEnd) {
                double aiScore = static_cast<double>(std::max(0, ai.remainingResource))
                    / ai.totalSteps;
                ns = clamp01(aiScore / idealScore);
            }
            // 未到终点或步数为0 → 0分（防止"全员失败=好迷宫"）
            normalizedScores.append(ns);
        }

        const double nsMean = meanOf(normalizedScores);
        const double nsStddev = stddevOf(normalizedScores, nsMean);
        const double nsMin = *std::min_element(normalizedScores.begin(),
                                                normalizedScores.end());
        const double nsMax = *std::max_element(normalizedScores.begin(),
                                                normalizedScores.end());
        const double nsRange = nsMax - nsMin;

        const double dStddev = clamp01(nsStddev / kStddevNorm);
        const double dRange  = clamp01(nsRange  / kRangeNorm);
        if (dStddev + dRange > 0.0) {
            result.designDiscrimination =
                2.0 * dStddev * dRange / (dStddev + dRange);
        } else {
            result.designDiscrimination = 0.0;
        }

        // C: 可完成性 — 到达终点的AI比例
        result.designStability = clamp01(
            static_cast<double>(reachedCount) / strategies.size());

        result.designBalance = clamp01(
            1.0 - std::abs(nsMean - kBalanceIdeal) / kBalanceTol);

        if (result.designStability <= 0.0) {
            result.finalFitness = 0.0;
        } else {
            const double dTerm = std::pow(std::max(result.designDiscrimination, 0.001), kWeightD);
            const double bTerm = std::pow(std::max(result.designBalance, 0.001), kWeightB);
            const double cTerm = std::pow(std::max(result.designStability, 0.001), kWeightC);
            result.finalFitness = 100.0 * dTerm * bTerm * cTerm;
        }
        result.designGroupScore = result.finalFitness;

        // 诊断字段
        result.meanAIScoreRatio = nsMean;
        result.aiScoreSpread = nsRange;
    }
    result.regretGreedy = result.dpScore - result.worstGreedyScore;
    result.regretCombined = result.regretGreedy;

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
        GreedyStrategy::CautiousCollector,
        GreedyStrategy::AvoidTraps,
        GreedyStrategy::EndGoalFirst
    };
    for (GreedyStrategy s : strategies) {
        PlayResult result = GreedyPlayer::play(maze, {}, {}, 0, s);
        worst = std::min(worst, result.remainingResource);
    }
    return worst;
}

