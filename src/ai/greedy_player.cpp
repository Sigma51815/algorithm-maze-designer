#include "ai/greedy_player.h"

#include <QHash>
#include <QQueue>

#include <algorithm>
#include <limits>

namespace {

int rowOfCell(int cell, int cols) { return cell / cols; }
int colOfCell(int cell, int cols) { return cell % cols; }
int cellIndex(int row, int col, int cols) { return row * cols + col; }

} // namespace

QVector<int> GreedyPlayer::observeVisible(const MazeModel &maze, int pos) {
    QVector<int> visible;
    const int r = rowOfCell(pos, maze.columns());
    const int c = colOfCell(pos, maze.columns());

    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            const int nr = r + dr;
            const int nc = c + dc;
            if (nr < 0 || nr >= maze.rows() || nc < 0 || nc >= maze.columns()) {
                continue;
            }
            const int candidate = cellIndex(nr, nc, maze.columns());
            if (candidate == pos) {
                visible.append(candidate);
            } else if ((dr == 0 || dc == 0) && maze.isOpen(pos, candidate)) {
                visible.append(candidate);
            }
        }
    }
    return visible;
}

int GreedyPlayer::bfsDistance(const MazeModel &maze, int from, int to) {
    if (from == to) return 0;
    QQueue<int> queue;
    QHash<int, int> dist;
    queue.enqueue(from);
    dist.insert(from, 0);

    while (!queue.isEmpty()) {
        const int cur = queue.dequeue();
        const int d = dist.value(cur);
        for (const int nb : maze.neighbors(cur)) {
            if (dist.contains(nb)) continue;
            if (nb == to) return d + 1;
            dist.insert(nb, d + 1);
            queue.enqueue(nb);
        }
    }
    return std::numeric_limits<int>::max();
}

QVector<int> GreedyPlayer::bfsPath(const MazeModel &maze, int from, int to) {
    if (from == to) return {from};
    QQueue<int> queue;
    QHash<int, int> parent;
    queue.enqueue(from);
    parent.insert(from, -1);

    while (!queue.isEmpty()) {
        const int cur = queue.dequeue();
        for (const int nb : maze.neighbors(cur)) {
            if (parent.contains(nb)) continue;
            parent.insert(nb, cur);
            if (nb == to) {
                QVector<int> path;
                int node = to;
                while (node != -1) {
                    path.prepend(node);
                    node = parent.value(node, -1);
                }
                return path;
            }
            queue.enqueue(nb);
        }
    }
    return {};
}

PlayResult GreedyPlayer::play(MazeModel &maze) {
    PlayResult result;
    const int totalCells = maze.cellCount();
    const int endCell = maze.endCell();

    int pos = maze.startCell();
    int resource = 0;
    int steps = 0;
    result.path.append(pos);

    while (pos != endCell) {
        if (steps > totalCells * 4) break;

        const QVector<int> visible = observeVisible(maze, pos);

        int bestTarget = -1;
        double bestRatio = -1.0;
        for (const int cell : visible) {
            const int val = maze.resourceAt(cell);
            if (val <= 0) continue;
            const int dist = bfsDistance(maze, pos, cell);
            if (dist <= 0) continue;
            const double ratio = static_cast<double>(val) / dist;
            if (ratio > bestRatio) {
                bestRatio = ratio;
                bestTarget = cell;
            }
        }

        int target = endCell;
        if (bestTarget >= 0) {
            target = bestTarget;
        }

        const QVector<int> path = bfsPath(maze, pos, target);
        if (path.size() < 2) break;

        pos = path[1];
        ++steps;

        const int val = maze.resourceAt(pos);
        if (val != 0) {
            resource += val;
            maze.consumeResource(pos);
        }

        result.path.append(pos);
    }

    result.totalSteps = steps;
    result.remainingResource = resource;
    result.reachedEnd = (pos == endCell);
    result.score = (steps > 0) ? static_cast<double>(resource) / steps : 0.0;
    return result;
}
