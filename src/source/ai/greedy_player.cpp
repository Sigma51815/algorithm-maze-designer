#include "ai/greedy_player.h"

#include <QQueue>
#include <QSet>

#include <algorithm>
#include <limits>

namespace {

struct VisibleCell {
    int gridRow = 0;
    int gridCol = 0;
    int mazeCell = -1;
    int value = 0;
    int dist = 0;
};

QVector<int> bfsPath(const MazeModel &maze, int from, int to) {
    if (from == to) return {from};
    QVector<int> parent(maze.cellCount(), -1);
    QQueue<int> queue;
    parent[from] = from;
    queue.enqueue(from);
    while (!queue.isEmpty()) {
        int cur = queue.dequeue();
        for (int nb : maze.neighbors(cur)) {
            if (parent[nb] >= 0) continue;
            parent[nb] = cur;
            if (nb == to) {
                QVector<int> path;
                for (int c = to; c != from; c = parent[c]) path.prepend(c);
                path.prepend(from);
                return path;
            }
            queue.enqueue(nb);
        }
    }
    return {};
}

} // namespace

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



    while (result.totalSteps < maxSteps) {
        if (pos == maze.endCell()) {
            result.reachedEnd = true;
            break;
        }

        int cellRow = pos / maze.columns();
        int cellCol = pos % maze.columns();
        int gr = cellRow * 2 + 1;
        int gc = cellCol * 2 + 1;

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

        int target = -1;

        if (strategy == GreedyStrategy::EndGoalFirst) {
            target = maze.endCell();
        } else {
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
                    sc = static_cast<double>(vc.value) / vc.dist;
                    break;
                case GreedyStrategy::NearestFirst:
                    sc = 1.0 / vc.dist;
                    break;
                case GreedyStrategy::AvoidTraps:
                    sc = static_cast<double>(vc.value) / vc.dist;
                    break;
                case GreedyStrategy::EndGoalFirst:
                    sc = 0.0;
                    break;
                }
                if (sc > bestScore) {
                    bestScore = sc;
                    target = vc.mazeCell;
                }
            }

            if (!foundResource) {
                int leastVisited = std::numeric_limits<int>::max();
                int bestTarget = -1;
                for (const auto &vc : visible) {
                    int visits = visitCount.value(vc.mazeCell, 0);
                    if (visits < leastVisited) {
                        leastVisited = visits;
                        bestTarget = vc.mazeCell;
                    }
                }
                if (bestTarget >= 0) target = bestTarget;
            }
        }

        if (target < 0 || target == pos) {
            int leastVisited = std::numeric_limits<int>::max();
            int bestTarget = -1;
            for (const auto &vc : visible) {
                int visits = visitCount.value(vc.mazeCell, 0);
                if (visits < leastVisited) {
                    leastVisited = visits;
                    bestTarget = vc.mazeCell;
                }
            }
            if (bestTarget >= 0) target = bestTarget;
            else break;
        }

        if (target == pos) break;

        QVector<int> path = bfsPath(maze, pos, target);
        if (path.size() < 2) break;

        int nextCell = path[1];
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

        if (pos == maze.endCell()) {
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
