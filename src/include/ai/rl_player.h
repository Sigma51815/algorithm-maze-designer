#pragma once

#include "maze.h"

#include <QVector>

#include <random>

struct RLConfig {
    int trainEpisodes = 2000;
    int playMaxSteps = 800;
    double alpha = 0.2;
    double gamma = 0.92;
    double epsilonStart = 0.5;
    double epsilonEnd = 0.03;
    double epsilonDecay = 0.9995;
    int coinCount = 30;
    int trapCount = 20;
    quint32 resourceSeed = 30000;
};

struct RLPlayResult {
    int totalResource = 0;
    int steps = 0;
    QVector<int> path;
};

class RLPlayer {
public:
    RLPlayer();

    void trainOnMaze(const MazeModel &maze, const RLConfig &config);
    void trainOnMazes(const QVector<MazeModel> &mazes, const RLConfig &config);
    RLPlayResult play(const MazeModel &maze, const RLConfig &config) const;
    void resetQTable();

    bool saveQTable(const QString &path, const RLConfig &config) const;
    bool loadQTable(const QString &path, RLConfig &config);

    static QVector<MazeModel> generateDiverseMazes(int count, quint32 baseSeed);
    static QVector<MazeModel> generateEasyMazes(int count, quint32 baseSeed);
    static QVector<MazeModel> generateHardMazes(int count, quint32 baseSeed);

private:
    static constexpr int ViewStates = 65536;
    static constexpr int PassableBits = 16;
    static constexpr int StateSize = ViewStates * PassableBits;
    static constexpr int ActionCount = 4;

    QVector<double> qTable_;

    int encodeState(const MazeModel &maze, int cell, const QSet<int> &visited) const;
    int chooseAction(int state, double epsilon, std::mt19937 &rng) const;
};
