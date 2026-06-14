#pragma once

#include "maze.h"

#include <QVector>

#include <random>

struct QLearningConfig {
    int episodes = 200;
    double alpha = 0.1;
    double gamma = 0.95;
    double epsilon = 0.2;
    double epsilonDecay = 0.995;
    double minEpsilon = 0.01;
    int refineSteps = 10;
    int topK = 5;
    quint32 seed = 42;
};

class QLearningOptimizer {
public:
    explicit QLearningOptimizer(const QLearningConfig &config = {});

    void train(QVector<MazeModel> &mazes);
    bool refineStep(MazeModel &maze);
    void saveQTable(const QString &path) const;
    void loadQTable(const QString &path);

    int stateCount() const { return stateCount_; }
    int actionCount() const { return actionCount_; }

private:
    QLearningConfig config_;
    QVector<QVector<double>> qTable_;
    int stateCount_ = 75;
    int actionCount_ = 10;
    mutable std::mt19937 rng_;

    int encodeState(const MazeModel &maze) const;
    QVector<int> availableActions(const MazeModel &maze) const;
    bool applyAction(MazeModel &maze, int actionIndex) const;
    double computeReward(const MazeModel &before, const MazeModel &after) const;
    int selectAction(int state, const QVector<int> &actions);
    void decayEpsilon();
};
