#pragma once

#include "maze.h"

#include <QObject>

#include <atomic>
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
    int coinValue = 50;
    int trapValue = -30;
    MazeAlgorithm baseAlgorithm = MazeAlgorithm::BreadthFirstSearch;
    quint32 seed = 42;
    // When true (default) the initial population is seeded by round-robining
    // all four base algorithms (DivideAndConquer/KruskalMst/DFS/BFS) so the GA
    // optimizes *on top of* the four base algorithms instead of a single one.
    // When false, only baseAlgorithm is used (legacy behaviour).
    bool useMixedAlgorithms = true;
    bool useSmartPlacement = true;
    bool useAdversarialPlacement = false;
    bool useEnhancedFitness = true;
    bool useBestMazeSearch = false;
    double topoWeight = 0.3;
};

struct OptimizerStats {
    int generation = 0;
    double bestFitness = 0.0;
    double avgFitness = 0.0;
    double worstFitness = 0.0;
    int bestDpScore = 0;
    int bestGreedyScore = 0;
    bool rlUsed = false;
    int totalGenerations = 0;
    int runIndex = 1;
    int runCount = 1;
};

class MazeOptimizer : public QObject {
    Q_OBJECT

public:
    explicit MazeOptimizer(QObject *parent = nullptr);

    void setConfig(const OptimizerConfig &config);
    const OptimizerConfig &config() const { return config_; }

    MazeModel run();
    void stop();

    static void edgeSwap(MazeModel &maze, std::mt19937 &rng);
    static MazeModel crossover(const MazeModel &a, const MazeModel &b, std::mt19937 &rng);
    // Round-robins the four base maze algorithms by index:
    // 0→DivideAndConquer, 1→KruskalMst, 2→DFS, 3→BFS, 4→DivideAndConquer, ...
    static MazeAlgorithm algorithmForIndex(int index);
    static QVector<OptimizerConfig> bestMazeSearchPresets(const OptimizerConfig &base);

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
    std::atomic<bool> stopped_{false};
    std::mt19937 rng_;

    Chromosome runSingle(int generationOffset, int totalGenerations,
                         int runIndex, int runCount);
    MazeModel runBestMazeSearch();
    Chromosome randomChromosome(int index, quint32 seed);
    double evaluateFitness(Chromosome &chrom);
    Chromosome crossover(const Chromosome &a, const Chromosome &b);
    void mutate(Chromosome &chrom);
    Chromosome tournamentSelect(const QVector<Chromosome> &population);

    static QVector<MazeEdge> mazeEdges(const MazeModel &maze);
};
