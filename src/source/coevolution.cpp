#include "coevolution.h"

#include "ai/greedy_player.h"
#include "maze_evaluator.h"
#include "resource_placer.h"

#include <algorithm>
#include <numeric>
#include <random>

CoEvolution::CoEvolution(QObject *parent) : QObject(parent) {}

CoEvolResult CoEvolution::run(const CoEvolConfig &config) {
    CoEvolResult result;
    std::mt19937 rng(config.baseSeed);

    RLPlayer rlPlayer;
    RLConfig rlCfg;
    rlCfg.trainEpisodes = config.rlTrainEpisodes;
    rlCfg.coinCount = config.coinCount;
    rlCfg.trapCount = config.trapCount;

    const int popSize = config.gaPopulation;
    const int eliteCount = std::max(2, popSize / 5);
    const quint32 resourceSeed = config.baseSeed + 50000;

    QVector<MazeModel> population(popSize);
    for (int i = 0; i < popSize; ++i) {
        const MazeAlgorithm algo = config.useMixedAlgorithms
            ? MazeOptimizer::algorithmForIndex(i)
            : config.baseAlgorithm;
        population[i].generate(config.mazeRows, config.mazeCols, algo,
                               static_cast<quint32>(config.baseSeed + i * 1000));
        for (int m = 0; m < 5; ++m) {
            MazeOptimizer::edgeSwap(population[i], rng);
        }
    }

    auto evalRLFitness = [&](MazeModel &maze) -> int {
        if (config.useSmartPlacement) {
            ResourcePlacerConfig pc;
            pc.coinCount = config.coinCount;
            pc.trapCount = config.trapCount;
            pc.seed = resourceSeed;
            ResourcePlacer::placeSmart(maze, pc);
        } else {
            maze.placeResources(config.coinCount, config.trapCount, resourceSeed);
        }
        const int dpVal = maze.optimalResourceWalk().maxValue;
        const RLPlayResult rl = rlPlayer.play(maze, rlCfg);
        return dpVal - rl.totalResource;
    };

    auto evalDP = [&](MazeModel &maze) -> int {
        if (config.useSmartPlacement) {
            ResourcePlacerConfig pc;
            pc.coinCount = config.coinCount;
            pc.trapCount = config.trapCount;
            pc.seed = resourceSeed;
            ResourcePlacer::placeSmart(maze, pc);
        } else {
            maze.placeResources(config.coinCount, config.trapCount, resourceSeed);
        }
        return maze.optimalResourceWalk().maxValue;
    };

    QVector<int> fitness(popSize);
    for (int i = 0; i < popSize; ++i) {
        fitness[i] = evalRLFitness(population[i]);
    }

    for (int cycle = 0; cycle < config.cycles; ++cycle) {
        for (int gen = 0; gen < config.gaGenerations; ++gen) {
            QVector<int> indices(popSize);
            for (int i = 0; i < popSize; ++i) {
                indices[i] = i;
            }
            std::sort(indices.begin(), indices.end(),
                      [&](int a, int b) { return fitness[a] > fitness[b]; });

            QVector<MazeModel> newPop(popSize);
            QVector<int> newFit(popSize);

            for (int i = 0; i < eliteCount; ++i) {
                newPop[i] = population[indices[i]];
                newFit[i] = fitness[indices[i]];
            }

            for (int i = eliteCount; i < popSize; ++i) {
                std::uniform_int_distribution<int> selDist(0, popSize / 2);
                const int p1 = indices[selDist(rng)];
                int p2 = indices[selDist(rng)];
                if (p1 == p2) {
                    p2 = indices[(p1 == indices[0]) ? 1 : 0];
                }

                newPop[i] = MazeOptimizer::crossover(population[p1], population[p2], rng);
                if (std::uniform_real_distribution<double>(0.0, 1.0)(rng) < config.gaMutationRate) {
                    MazeOptimizer::edgeSwap(newPop[i], rng);
                }
                newFit[i] = evalRLFitness(newPop[i]);
            }

            population = std::move(newPop);
            fitness = std::move(newFit);
        }

        const int bestIdx = static_cast<int>(
            std::max_element(fitness.begin(), fitness.end()) - fitness.begin());
        const int gaBest = fitness[bestIdx];
        if (gaBest > result.bestFitness) {
            result.bestFitness = gaBest;
            result.bestMaze = population[bestIdx];
        }
        result.cycleBestFitness.append(gaBest);

        QVector<int> sortedIdx(popSize);
        for (int i = 0; i < popSize; ++i) {
            sortedIdx[i] = i;
        }
        std::sort(sortedIdx.begin(), sortedIdx.end(),
                  [&](int a, int b) { return fitness[a] > fitness[b]; });

        QVector<MazeModel> hardestMazes;
        const int topK = std::min(config.rlTopK, popSize);
        for (int i = 0; i < topK; ++i) {
            MazeModel m = population[sortedIdx[i]];
            if (config.useSmartPlacement) {
                ResourcePlacerConfig pc;
                pc.coinCount = config.coinCount;
                pc.trapCount = config.trapCount;
                pc.seed = resourceSeed;
                ResourcePlacer::placeSmart(m, pc);
            } else {
                m.placeResources(config.coinCount, config.trapCount, resourceSeed);
            }
            hardestMazes.append(m);
        }

        rlPlayer.trainOnMazes(hardestMazes, rlCfg);

        MazeModel evalMaze = population[bestIdx];
        if (config.useSmartPlacement) {
            ResourcePlacerConfig pc;
            pc.coinCount = config.coinCount;
            pc.trapCount = config.trapCount;
            pc.seed = resourceSeed;
            ResourcePlacer::placeSmart(evalMaze, pc);
        } else {
            evalMaze.placeResources(config.coinCount, config.trapCount, resourceSeed);
        }
        const RLPlayResult rlResult = rlPlayer.play(evalMaze, rlCfg);
        result.rlScores.append(rlResult.totalResource);

        double topoScore = MazeEvaluator::computeTopoDifficulty(result.bestMaze);
        result.topoScores.append(topoScore);

        emit cycleFinished(cycle + 1, gaBest, rlResult.totalResource);
    }

    if (result.bestMaze.cellCount() == 0) {
        const int bestIdx = static_cast<int>(
            std::max_element(fitness.begin(), fitness.end()) - fitness.begin());
        result.bestMaze = population[bestIdx];
    }

    emit finished(result);
    return result;
}
