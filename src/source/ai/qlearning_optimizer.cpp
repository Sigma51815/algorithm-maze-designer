#include "ai/qlearning_optimizer.h"

#include "ai/greedy_player.h"

#include <QFile>
#include <QQueue>
#include <QSet>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <numeric>

QLearningOptimizer::QLearningOptimizer(const QLearningConfig &config)
    : config_(config), rng_(config.seed) {
    qTable_.resize(stateCount_, QVector<double>(actionCount_, 0.0));
}

int QLearningOptimizer::encodeState(const MazeModel &maze) const {
    const int totalCells = maze.cellCount();
    if (totalCells == 0) return 0;

    MazeStatistics stats = maze.statistics();

    int f1 = std::min(4, stats.deadEnds * 5 / std::max(1, totalCells));
    int directDist = maze.rows() + maze.columns() - 2;
    int f2 = std::min(4, stats.solutionLength * 5 / std::max(1, directDist));

    int trapOnPath = 0;
    int totalTraps = 0;
    ResourcePlan dp = maze.optimalResourceWalk();
    QSet<int> dpCells(dp.collectedCells.begin(), dp.collectedCells.end());
    for (int cell = 0; cell < totalCells; ++cell) {
        if (maze.resourceAt(cell) < 0) {
            ++totalTraps;
            if (dpCells.contains(cell)) ++trapOnPath;
        }
    }
    int f3 = totalTraps > 0 ? std::min(2, trapOnPath * 3 / totalTraps) : 0;

    return f1 * 15 + f2 * 3 + f3;
}

QVector<int> QLearningOptimizer::availableActions(const MazeModel &maze) const {
    const int rows = maze.rows();
    const int cols = maze.columns();
    const int totalCells = rows * cols;

    auto edgeKey = [](int a, int b) -> quint64 {
        int lo = std::min(a, b), hi = std::max(a, b);
        return (static_cast<quint64>(lo) << 32) | static_cast<quint32>(hi);
    };

    QSet<quint64> edgeSet;
    for (int cell = 0; cell < totalCells; ++cell) {
        for (int next : maze.neighbors(cell)) {
            edgeSet.insert(edgeKey(cell, next));
        }
    }

    QVector<int> wallCandidates;
    for (int cell = 0; cell < totalCells; ++cell) {
        int row = cell / cols;
        int col = cell % cols;
        if (col + 1 < cols) {
            int right = cell + 1;
            if (!edgeSet.contains(edgeKey(cell, right)))
                wallCandidates.append(wallCandidates.size());
        }
        if (row + 1 < rows) {
            int down = cell + cols;
            if (!edgeSet.contains(edgeKey(cell, down)))
                wallCandidates.append(wallCandidates.size());
        }
    }

    QVector<int> actions;
    for (int i = 0; i < std::min(actionCount_, static_cast<int>(wallCandidates.size())); ++i) {
        actions.append(i);
    }
    return actions;
}

bool QLearningOptimizer::applyAction(MazeModel &maze, int actionIndex) const {
    const int rows = maze.rows();
    const int cols = maze.columns();
    const int totalCells = rows * cols;

    auto edgeKey = [](int a, int b) -> quint64 {
        int lo = std::min(a, b), hi = std::max(a, b);
        return (static_cast<quint64>(lo) << 32) | static_cast<quint32>(hi);
    };

    QSet<quint64> edgeSet;
    for (int cell = 0; cell < totalCells; ++cell) {
        for (int next : maze.neighbors(cell)) {
            edgeSet.insert(edgeKey(cell, next));
        }
    }

    struct WallCandidate { int from; int to; };
    QVector<WallCandidate> wallCandidates;
    for (int cell = 0; cell < totalCells; ++cell) {
        int row = cell / cols;
        int col = cell % cols;
        if (col + 1 < cols) {
            int right = cell + 1;
            if (!edgeSet.contains(edgeKey(cell, right)))
                wallCandidates.append({cell, right});
        }
        if (row + 1 < rows) {
            int down = cell + cols;
            if (!edgeSet.contains(edgeKey(cell, down)))
                wallCandidates.append({cell, down});
        }
    }

    if (actionIndex >= wallCandidates.size()) return false;

    const WallCandidate newEdge = wallCandidates[actionIndex];

    QVector<int> parent(totalCells, -1);
    QQueue<int> queue;
    parent[newEdge.from] = newEdge.from;
    queue.enqueue(newEdge.from);
    while (!queue.isEmpty() && parent[newEdge.to] < 0) {
        int cur = queue.dequeue();
        for (int next : maze.neighbors(cur)) {
            if (parent[next] < 0) {
                parent[next] = cur;
                queue.enqueue(next);
            }
        }
    }

    if (parent[newEdge.to] < 0) return false;

    QVector<int> cyclePath;
    for (int cell = newEdge.to;; cell = parent[cell]) {
        cyclePath.append(cell);
        if (cell == newEdge.from) break;
    }
    if (cyclePath.size() < 3) return false;

    std::uniform_int_distribution<int> dist(0, cyclePath.size() - 2);
    int removeIdx = dist(rng_);
    int removeA = cyclePath[removeIdx];
    int removeB = cyclePath[removeIdx + 1];

    QSet<quint64> newEdgeSet = edgeSet;
    newEdgeSet.remove(edgeKey(removeA, removeB));
    newEdgeSet.insert(edgeKey(newEdge.from, newEdge.to));

    QVector<MazeEdge> newEdges;
    newEdges.reserve(newEdgeSet.size());
    for (const auto &key : newEdgeSet) {
        int from = static_cast<int>(key >> 32);
        int to = static_cast<int>(key & 0xFFFFFFFF);
        newEdges.append({from, to});
    }

    maze.setFromEdges(rows, cols, newEdges, rng_());
    return true;
}

double QLearningOptimizer::computeReward(const MazeModel &before,
                                          const MazeModel &after) const {
    auto evalRegret = [](const MazeModel &m) -> double {
        ResourcePlan dp = m.optimalResourceWalk();
        PlayResult greedy = GreedyPlayer::play(m);
        return static_cast<double>(dp.maxValue - greedy.remainingResource);
    };
    return evalRegret(after) - evalRegret(before);
}

int QLearningOptimizer::selectAction(int state, const QVector<int> &actions) {
    if (actions.isEmpty()) return -1;

    std::uniform_real_distribution<double> dist(0.0, 1.0);
    if (dist(rng_) < config_.epsilon) {
        std::uniform_int_distribution<int> aDist(0, actions.size() - 1);
        return actions[aDist(rng_)];
    }

    int bestAction = actions.first();
    double bestValue = qTable_[state][actions.first()];
    for (int a : actions) {
        if (qTable_[state][a] > bestValue) {
            bestValue = qTable_[state][a];
            bestAction = a;
        }
    }
    return bestAction;
}

void QLearningOptimizer::decayEpsilon() {
    config_.epsilon = std::max(config_.minEpsilon,
                                config_.epsilon * config_.epsilonDecay);
}

void QLearningOptimizer::train(QVector<MazeModel> &mazes) {
    for (int ep = 0; ep < config_.episodes; ++ep) {
        for (MazeModel &maze : mazes) {
            int state = encodeState(maze);

            for (int step = 0; step < config_.refineSteps; ++step) {
                QVector<int> actions = availableActions(maze);
                if (actions.isEmpty()) break;

                int action = selectAction(state, actions);
                if (action < 0) break;

                MazeModel before = maze;
                if (!applyAction(maze, action)) continue;

                double reward = computeReward(before, maze);
                int nextState = encodeState(maze);

                double maxNextQ = *std::max_element(
                    qTable_[nextState].begin(), qTable_[nextState].end());

                qTable_[state][action] += config_.alpha *
                    (reward + config_.gamma * maxNextQ - qTable_[state][action]);

                state = nextState;
            }
        }
        decayEpsilon();
    }
}

bool QLearningOptimizer::refineStep(MazeModel &maze) {
    int state = encodeState(maze);
    QVector<int> actions = availableActions(maze);
    if (actions.isEmpty()) return false;

    int action = selectAction(state, actions);
    if (action < 0) return false;

    MazeModel before = maze;
    if (!applyAction(maze, action)) return false;

    double reward = computeReward(before, maze);
    int nextState = encodeState(maze);

    double maxNextQ = *std::max_element(
        qTable_[nextState].begin(), qTable_[nextState].end());

    qTable_[state][action] += config_.alpha *
        (reward + config_.gamma * maxNextQ - qTable_[state][action]);

    return true;
}

void QLearningOptimizer::saveQTable(const QString &path) const {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);
    for (int s = 0; s < stateCount_; ++s) {
        for (int a = 0; a < actionCount_; ++a) {
            if (a > 0) out << ',';
            out << qTable_[s][a];
        }
        out << '\n';
    }
}

void QLearningOptimizer::loadQTable(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream in(&file);
    for (int s = 0; s < stateCount_ && !in.atEnd(); ++s) {
        QString line = in.readLine();
        QStringList values = line.split(',');
        for (int a = 0; a < std::min(actionCount_, static_cast<int>(values.size())); ++a) {
            qTable_[s][a] = values[a].toDouble();
        }
    }
}
