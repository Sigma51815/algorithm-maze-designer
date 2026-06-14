#include "ai/greedy_player.h"

#include <QQueue>
#include <QSet>

#include <algorithm>
#include <limits>

namespace {

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

QVector<int> bfsDistances(const MazeModel &maze, int from) {
    QVector<int> dist(maze.cellCount(), -1);
    QQueue<int> queue;
    dist[from] = 0;
    queue.enqueue(from);
    while (!queue.isEmpty()) {
        int cur = queue.dequeue();
        for (int nb : maze.neighbors(cur)) {
            if (dist[nb] >= 0) continue;
            dist[nb] = dist[cur] + 1;
            queue.enqueue(nb);
        }
    }
    return dist;
}

} // namespace

PlayResult GreedyPlayer::play(const MazeModel &maze,
                              const QVector<int> &bossHealth,
                              const QVector<BossSkill> &bossSkills,
                              int initialResource) {
    PlayResult result;
    if (maze.cellCount() == 0) return result;

    const int maxSteps = maze.cellCount() * 10;
    int pos = maze.startCell();
    int resource = initialResource;
    int coins = 0;
    int traps = 0;
    QSet<int> visitedResources;
    result.walk.append(pos);

    while (result.totalSteps < maxSteps) {
        if (pos == maze.endCell()) {
            result.reachedEnd = true;
            break;
        }

        const QVector<int> dist = bfsDistances(maze, pos);
        int target = -1;
        double bestScore = -1.0;
        for (int cell = 0; cell < maze.cellCount(); ++cell) {
            if (cell == pos) continue;
            if (visitedResources.contains(cell)) continue;
            int val = maze.resourceAt(cell);
            if (val <= 0) continue;
            if (dist[cell] <= 0) continue;
            double sc = static_cast<double>(val) / dist[cell];
            if (sc > bestScore) {
                bestScore = sc;
                target = cell;
            }
        }

        if (target < 0) {
            target = maze.endCell();
        }

        QVector<int> path = bfsPath(maze, pos, target);
        if (path.size() < 2) break;

        for (int i = 1; i < path.size(); ++i) {
            pos = path[i];
            ++result.totalSteps;
            result.walk.append(pos);
            if (!visitedResources.contains(pos)) {
                int val = maze.resourceAt(pos);
                if (val != 0) {
                    visitedResources.insert(pos);
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
        if (result.reachedEnd) break;
    }

    result.remainingResource = resource;
    result.collectedCoins = coins;
    result.triggeredTraps = traps;
    result.score = result.totalSteps > 0
        ? static_cast<double>(resource) / result.totalSteps
        : 0.0;
    return result;
}
