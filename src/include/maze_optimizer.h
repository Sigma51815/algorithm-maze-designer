#pragma once

#include "maze.h"
#include "qlearning_optimizer.h"

#include <QObject>

#include <random>

struct OptimizerConfig {
    int rows = 15;
    int columns = 15;
    int populationSize = 20;
    int generations = 50;
    double mutationRate = 0.15;
    double crossoverRate = 0.7;
    int tournamentSize = 3;
    int coinCount = 8;
    int trapCount = 5;
    MazeAlgorithm baseAlgorithm = MazeAlgorithm::BreadthFirstSearch;
    quint32 seed = 42;
    bool enableRL = false;
    int rlEpisodes = 50;
    int rlTopK = 3;
    int rlRefineSteps = 5;
};

struct OptimizerStats {
    int generation = 0;
    double bestFitness = 0.0;
    double avgFitness = 0.0;
    double worstFitness = 0.0;
    int bestDpScore = 0;
    int bestGreedyScore = 0;
    bool rlUsed = false;
};

class MazeOptimizer : public QObject {
    Q_OBJECT

public:
    explicit MazeOptimizer(QObject *parent = nullptr);

    void setConfig(const OptimizerConfig &config);
    const OptimizerConfig &config() const { return config_; }

    MazeModel run();
    void stop();

signals:
    void generationFinished(const OptimizerStats &stats);
    void finished(const MazeModel &bestMaze);

private:
    struct Chromosome {
        MazeModel maze;
        double fitness = 0.0;
        int dpScore = 0;
        int greedyScore = 0;
    };

    OptimizerConfig config_;
    bool stopped_ = false;
    std::mt19937 rng_;

    Chromosome randomChromosome(quint32 seed);
    double evaluateFitness(Chromosome &chrom);
    Chromosome crossover(const Chromosome &a, const Chromosome &b);
    void mutate(Chromosome &chrom);
    Chromosome tournamentSelect(const QVector<Chromosome> &population);

    static QVector<MazeEdge> mazeEdges(const MazeModel &maze);
};
