#pragma once

#include "ai/rl_player.h"
#include "maze_optimizer.h"

#include <QObject>

struct CoEvolConfig {
    int cycles = 10;
    int gaGenerations = 20;
    int gaPopulation = 20;
    double gaMutationRate = 0.3;
    int rlTrainEpisodes = 100;
    int rlTopK = 5;
    int mazeRows = 15;
    int mazeCols = 15;
    MazeAlgorithm baseAlgorithm = MazeAlgorithm::KruskalMst;
    quint32 baseSeed = 202506;
    int coinCount = 30;
    int trapCount = 20;
    // When true (default) the initial GA population is seeded by round-robining
    // all four base algorithms so co-evolution builds *on top of* the four base
    // algorithms instead of a single one. When false, only baseAlgorithm is used.
    bool useMixedAlgorithms = true;
    bool useSmartPlacement = true;
    double topoWeight = 0.3;
};

struct CoEvolResult {
    MazeModel bestMaze;
    int bestFitness = 0;
    QVector<int> cycleBestFitness;
    QVector<int> rlScores;
    QVector<double> topoScores;
};

class CoEvolution : public QObject {
    Q_OBJECT
public:
    explicit CoEvolution(QObject *parent = nullptr);

    CoEvolResult run(const CoEvolConfig &config);

signals:
    void cycleFinished(int cycle, int gaBestFitness, int rlScore);
    void finished(const CoEvolResult &result);
};
