// 文件职责：内置 3x3 视野贪心 AI。
// 用几种局部策略模拟参赛 AI，给迷宫区分度和 GA 适应度评估提供对照。
#include "ai/greedy_player.h"

#include <QQueue>
#include <QSet>

#include <algorithm>
#include <cstdlib>
#include <limits>

namespace {

struct VisibleCell {
    int gridRow = 0;
    int gridCol = 0;
    int mazeCell = -1;
    int value = 0;
    int dist = 0;  // expanded-grid distance, always 2 for immediate neighbours
};

} // namespace

// 3x3 局部 AI 主循环：每一步只看相邻格，按策略选择下一步，用于模拟真实受限视野玩家。
PlayResult GreedyPlayer::play(const MazeModel &maze,
                               const QVector<int> &bossHealth,
                               const QVector<BossSkill> &bossSkills,
                               int initialResource,
                               GreedyStrategy strategy) {
    PlayResult result;
    if (maze.cellCount() == 0) return result;

    const QStringList grid = maze.expandedGrid();
    const int gridRows = grid.size();
    const int gridCols = grid.isEmpty() ? 0 : grid.first().size();

    auto gridChar = [&](int r, int c) -> QChar {
        if (r < 0 || r >= gridRows || c < 0 || c >= gridCols)
            return QLatin1Char('#');
        return grid[r][c];
    };

    auto cellAtGrid = [&](int r, int c) -> int {
        if ((r & 1) == 0 || (c & 1) == 0) return -1;
        int cr = (r - 1) / 2;
        int cc = (c - 1) / 2;
        if (cr < 0 || cr >= maze.rows() || cc < 0 || cc >= maze.columns())
            return -1;
        return cr * maze.columns() + cc;
    };

    const int maxSteps = maze.cellCount() * 10;
    int pos = maze.startCell();
    int resource = initialResource;
    int coins = 0;
    int traps = 0;
    QSet<int> collectedCells;
    QMap<int, int> visitCount;
    visitCount[pos] = 1;
    result.walk.append(pos);

    const int endCell = maze.endCell();
    const int cols = maze.columns();
    const int endRow = endCell / cols;
    const int endCol = endCell % cols;

    while (result.totalSteps < maxSteps) {
        if (pos == endCell) {
            result.reachedEnd = true;
            break;
        }

        int cellRow = pos / cols;
        int cellCol = pos % cols;
        int gr = cellRow * 2 + 1;
        int gc = cellCol * 2 + 1;

        // ---- 3×3 视野：扫描四个相邻方向 ----
        QVector<VisibleCell> visible;
        const int dirs[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        for (auto &d : dirs) {
            int pr = gr + d[0];
            int pc = gc + d[1];
            if (gridChar(pr, pc) == QLatin1Char('#')) continue;
            int nr = gr + d[0] * 2;
            int nc = gc + d[1] * 2;
            QChar ch = gridChar(nr, nc);
            if (ch == QLatin1Char('#')) continue;
            int mc = cellAtGrid(nr, nc);
            if (mc < 0) continue;
            int val = 0;
            if (!collectedCells.contains(mc)) {
                val = maze.resourceAt(mc);
            }
            visible.append({nr, nc, mc, val, 2});
        }

        // ---- 贪心目标选择 + 直接踏入（纯 3×3，无 BFS）----
        int nextCell = -1;

        if (strategy == GreedyStrategy::EndGoalFirst) {
            // 朝终点方向：选 4 邻居中曼哈顿距离最小的
            int bestDist = std::numeric_limits<int>::max();
            int bestVisits = std::numeric_limits<int>::max();
            for (const auto &vc : visible) {
                int nr = vc.mazeCell / cols;
                int nc = vc.mazeCell % cols;
                int md = std::abs(nr - endRow) + std::abs(nc - endCol);
                int vis = visitCount.value(vc.mazeCell, 0);
                if (md < bestDist || (md == bestDist && vis < bestVisits)) {
                    bestDist = md;
                    bestVisits = vis;
                    nextCell = vc.mazeCell;
                }
            }
        } else {
            // 资源收集策略：从可见相邻格中选目标，直接踏入
            double bestScore = -1e9;
            bool foundResource = false;
            for (const auto &vc : visible) {
                if (vc.value == 0) continue;
                if (vc.value < 0 && strategy == GreedyStrategy::AvoidTraps) continue;
                if (vc.value < 0 && strategy != GreedyStrategy::ValuePerStep) continue;
                foundResource = true;
                double sc = 0.0;
                switch (strategy) {
                case GreedyStrategy::ValuePerStep:
                    sc = static_cast<double>(vc.value) / vc.dist;  // dist=2, so ∝ value
                    break;
                case GreedyStrategy::CautiousCollector:
                    sc = static_cast<double>(vc.value) / vc.dist;  // 只拿正价值，同AvoidTraps
                    break;
                case GreedyStrategy::AvoidTraps:
                    sc = static_cast<double>(vc.value) / vc.dist;
                    break;
                default:
                    break;
                }
                if (sc > bestScore) {
                    bestScore = sc;
                    nextCell = vc.mazeCell;
                }
            }

            // 无可见资源 → 探索：去访问次数最少的邻居
            if (!foundResource || nextCell < 0) {
                int leastVisited = std::numeric_limits<int>::max();
                int bestManhattan = std::numeric_limits<int>::max();
                for (const auto &vc : visible) {
                    int visits = visitCount.value(vc.mazeCell, 0);
                    // CautiousCollector 探索时偏好朝终点方向（曼哈顿最近）
                    // 其他策略纯最少访问（迭代顺序决定 tie-break）
                    if (strategy == GreedyStrategy::CautiousCollector) {
                        int md = std::abs(vc.mazeCell / cols - endRow)
                               + std::abs(vc.mazeCell % cols - endCol);
                        if (visits < leastVisited
                            || (visits == leastVisited && md < bestManhattan)) {
                            leastVisited = visits;
                            bestManhattan = md;
                            nextCell = vc.mazeCell;
                        }
                    } else {
                        if (visits < leastVisited) {
                            leastVisited = visits;
                            nextCell = vc.mazeCell;
                        }
                    }
                }
            }
        }

        // 无路可走
        if (nextCell < 0 || nextCell == pos) break;

        pos = nextCell;
        ++result.totalSteps;
        result.walk.append(pos);
        ++visitCount[pos];

        if (!collectedCells.contains(pos)) {
            int val = maze.resourceAt(pos);
            if (val != 0) {
                collectedCells.insert(pos);
                resource += val;
                if (val > 0) ++coins;
                else ++traps;
            }
        }

        if (pos == maze.bossCell() && !bossHealth.isEmpty()
            && !bossSkills.isEmpty() && !result.bossDefeated) {
            BossResult bossResult = BossSolver::solve(bossHealth, bossSkills);
            if (bossResult.solved) {
                result.bossDefeated = true;
                result.bossAttempts = 1;
            }
        }

        if (pos == endCell) {
            result.reachedEnd = true;
            break;
        }
    }

    result.remainingResource = resource;
    result.collectedCoins = coins;
    result.triggeredTraps = traps;
    result.score = result.totalSteps > 0
        ? static_cast<double>(resource) / result.totalSteps
        : 0.0;
    return result;
}
