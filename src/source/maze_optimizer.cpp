#include "maze_optimizer.h"

#include "ai/greedy_player.h"
#include "maze_evaluator.h"
#include "resource_placer.h"

#include <QQueue>
#include <QSet>

#include <algorithm>
#include <limits>
#include <numeric>
#include <queue>

MazeOptimizer::MazeOptimizer(QObject *parent) : QObject(parent) {}

void MazeOptimizer::setConfig(const OptimizerConfig &config) {
    config_ = config;
}

void MazeOptimizer::stop() {
    stopped_ = true;
}

MazeModel MazeOptimizer::run() {
    stopped_ = false;
    rng_.seed(config_.seed);

    QVector<Chromosome> population(config_.populationSize);
    for (int i = 0; i < config_.populationSize; ++i) {
        population[i] = randomChromosome(i, static_cast<quint32>(config_.seed + i));
        evaluateFitness(population[i]);
    }

    Chromosome best = *std::max_element(
        population.begin(), population.end(),
        [](const Chromosome &a, const Chromosome &b) {
            return a.fitness < b.fitness;
        });

    for (int gen = 0; gen < config_.generations && !stopped_; ++gen) {
        QVector<Chromosome> nextGen;
        nextGen.reserve(config_.populationSize);

        nextGen.append(best);

        while (nextGen.size() < config_.populationSize) {
            Chromosome parent1 = tournamentSelect(population);
            Chromosome parent2 = tournamentSelect(population);

            std::uniform_real_distribution<double> dist(0.0, 1.0);
            Chromosome child;
            if (dist(rng_) < config_.crossoverRate) {
                child = crossover(parent1, parent2);
            } else {
                child = parent1;
            }

            if (dist(rng_) < config_.mutationRate) {
                mutate(child);
            }

            evaluateFitness(child);
            nextGen.append(child);
        }

        population = nextGen;

        const Chromosome &genBest = *std::max_element(
            population.begin(), population.end(),
            [](const Chromosome &a, const Chromosome &b) {
                return a.fitness < b.fitness;
            });

        if (genBest.fitness > best.fitness) {
            best = genBest;
        }

        OptimizerStats stats;
        stats.generation = gen + 1;
        stats.bestFitness = genBest.fitness;
        stats.bestDpScore = genBest.dpScore;
        stats.bestGreedyScore = genBest.greedyScore;

        double sumFitness = 0.0;
        double worstFitness = genBest.fitness;
        for (const Chromosome &c : population) {
            sumFitness += c.fitness;
            worstFitness = std::min(worstFitness, c.fitness);
        }
        stats.avgFitness = sumFitness / population.size();
        stats.worstFitness = worstFitness;

        emit generationFinished(stats);
    }

    emit finished(best.maze);
    return best.maze;
}

MazeAlgorithm MazeOptimizer::algorithmForIndex(int index) {
    static constexpr MazeAlgorithm kAllAlgorithms[4] = {
        MazeAlgorithm::DivideAndConquer,
        MazeAlgorithm::KruskalMst,
        MazeAlgorithm::DepthFirstSearch,
        MazeAlgorithm::BreadthFirstSearch};
    return kAllAlgorithms[((index % 4) + 4) % 4];
}

MazeOptimizer::Chromosome MazeOptimizer::randomChromosome(int index, quint32 seed) {
    Chromosome chrom;
    const MazeAlgorithm algo = config_.useMixedAlgorithms
        ? algorithmForIndex(index)
        : config_.baseAlgorithm;
    chrom.maze.generate(config_.rows, config_.columns, algo, seed);
    if (config_.useAdversarialPlacement) {
        ResourcePlacerConfig pc;
        pc.coinCount = config_.coinCount;
        pc.trapCount = config_.trapCount;
        pc.seed = seed + 1000;
        ResourcePlacer::placeAdversarial(chrom.maze, pc);
    } else if (config_.useSmartPlacement) {
        ResourcePlacerConfig pc;
        pc.coinCount = config_.coinCount;
        pc.trapCount = config_.trapCount;
        pc.seed = seed + 1000;
        ResourcePlacer::placeSmart(chrom.maze, pc);
    } else {
        chrom.maze.placeResources(config_.coinCount, config_.trapCount, seed + 1000);
    }
    return chrom;
}

double MazeOptimizer::evaluateFitness(Chromosome &chrom) {
    if (config_.useEnhancedFitness) {
        EvaluatorConfig ec;
        // Use the same placement mode for the fitness formula, but
        // skip actual re-placement (chromosome resources are already set
        // by randomChromosome / mutate, matching config_.useAdversarialPlacement).
        ec.useAdversarialPlacement = config_.useAdversarialPlacement;
        ec.useSmartPlacement = false;
        ec.skipPlacement = true;
        ec.topoWeight = config_.topoWeight;
        EvalResult eval = MazeEvaluator::evaluate(chrom.maze, ec);
        chrom.dpScore = eval.dpScore;
        chrom.greedyScore = eval.worstGreedyScore;
        chrom.fitness = eval.finalFitness;
    } else {
        ResourcePlan dpPlan = chrom.maze.optimalResourceWalk();
        chrom.dpScore = dpPlan.maxValue;

        int worstGreedy = std::numeric_limits<int>::max();
        const QVector<GreedyStrategy> strategies = {
            GreedyStrategy::ValuePerStep,
            GreedyStrategy::NearestFirst,
            GreedyStrategy::AvoidTraps,
            GreedyStrategy::EndGoalFirst
        };
        for (GreedyStrategy s : strategies) {
            PlayResult result = GreedyPlayer::play(chrom.maze, {}, {}, 0, s);
            worstGreedy = std::min(worstGreedy, result.remainingResource);
        }
        chrom.greedyScore = worstGreedy;
        chrom.fitness = static_cast<double>(chrom.dpScore - chrom.greedyScore);
    }
    return chrom.fitness;
}

QVector<MazeEdge> MazeOptimizer::mazeEdges(const MazeModel &maze) {
    return maze.allEdges();
}

MazeOptimizer::Chromosome MazeOptimizer::crossover(const Chromosome &a,
                                                     const Chromosome &b) {
    const int totalCells = config_.rows * config_.columns;
    const int targetEdges = totalCells - 1;

    QVector<MazeEdge> edgesA = a.maze.allEdges();
    QVector<MazeEdge> edgesB = b.maze.allEdges();

    QVector<MazeEdge> combined;
    combined.reserve(edgesA.size() + edgesB.size());
    combined += edgesA;
    combined += edgesB;
    std::shuffle(combined.begin(), combined.end(), rng_);

    QVector<int> ufParent(totalCells);
    QVector<int> ufRank(totalCells, 0);
    std::iota(ufParent.begin(), ufParent.end(), 0);

    std::function<int(int)> find = [&](int x) -> int {
        if (ufParent[x] != x) ufParent[x] = find(ufParent[x]);
        return ufParent[x];
    };
    auto unite = [&](int u, int v) -> bool {
        int ru = find(u), rv = find(v);
        if (ru == rv) return false;
        if (ufRank[ru] < ufRank[rv]) std::swap(ru, rv);
        ufParent[rv] = ru;
        if (ufRank[ru] == ufRank[rv]) ++ufRank[ru];
        return true;
    };

    QVector<MazeEdge> childEdges;
    childEdges.reserve(targetEdges);
    for (const MazeEdge &e : combined) {
        if (unite(e.from, e.to)) {
            childEdges.append(e);
            if (childEdges.size() == targetEdges) break;
        }
    }

    for (int cell = 0; childEdges.size() < targetEdges && cell < totalCells; ++cell) {
        int row = cell / config_.columns;
        int col = cell % config_.columns;
        if (col + 1 < config_.columns) {
            int right = cell + 1;
            if (unite(cell, right))
                childEdges.append({std::min(cell, right), std::max(cell, right)});
        }
        if (row + 1 < config_.rows) {
            int down = cell + config_.columns;
            if (unite(cell, down))
                childEdges.append({std::min(cell, down), std::max(cell, down)});
        }
    }

    Chromosome child;
    child.maze.setFromEdges(config_.rows, config_.columns, childEdges, rng_());
    if (config_.useAdversarialPlacement) {
        ResourcePlacerConfig pc;
        pc.coinCount = config_.coinCount;
        pc.trapCount = config_.trapCount;
        pc.seed = rng_();
        ResourcePlacer::placeAdversarial(child.maze, pc);
    } else if (config_.useSmartPlacement) {
        ResourcePlacerConfig pc;
        pc.coinCount = config_.coinCount;
        pc.trapCount = config_.trapCount;
        pc.seed = rng_();
        ResourcePlacer::placeSmart(child.maze, pc);
    } else {
        child.maze.placeResources(config_.coinCount, config_.trapCount, rng_());
    }
    return child;
}

void MazeOptimizer::mutate(Chromosome &chrom) {
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    if (dist(rng_) < 0.3) {
        quint32 newSeed = rng_();
        const MazeAlgorithm algo = config_.useMixedAlgorithms
            ? algorithmForIndex(static_cast<int>(rng_() % 4))
            : config_.baseAlgorithm;
        chrom.maze.generate(config_.rows, config_.columns, algo, newSeed);
        if (config_.useAdversarialPlacement) {
            ResourcePlacerConfig pc;
            pc.coinCount = config_.coinCount;
            pc.trapCount = config_.trapCount;
            pc.seed = newSeed + 1000;
            ResourcePlacer::placeAdversarial(chrom.maze, pc);
        } else if (config_.useSmartPlacement) {
            ResourcePlacerConfig pc;
            pc.coinCount = config_.coinCount;
            pc.trapCount = config_.trapCount;
            pc.seed = newSeed + 1000;
            ResourcePlacer::placeSmart(chrom.maze, pc);
        } else {
            chrom.maze.placeResources(config_.coinCount, config_.trapCount, newSeed + 1000);
        }
        return;
    }

    const int rows = config_.rows;
    const int cols = config_.columns;
    const int totalCells = rows * cols;
    QVector<MazeEdge> edges = chrom.maze.allEdges();

    auto edgeKey = [](int a, int b) -> quint64 {
        int lo = std::min(a, b), hi = std::max(a, b);
        return (static_cast<quint64>(lo) << 32) | static_cast<quint32>(hi);
    };

    QSet<quint64> edgeSet;
    for (const MazeEdge &e : edges) {
        edgeSet.insert(edgeKey(e.from, e.to));
    }

    QVector<MazeEdge> wallCandidates;
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

    if (wallCandidates.empty()) return;

    std::shuffle(wallCandidates.begin(), wallCandidates.end(), rng_);
    const MazeEdge newEdge = wallCandidates.first();

    QVector<int> parent(totalCells, -1);
    QQueue<int> queue;
    parent[newEdge.from] = newEdge.from;
    queue.enqueue(newEdge.from);
    while (!queue.isEmpty() && parent[newEdge.to] < 0) {
        int cur = queue.dequeue();
        for (int next : chrom.maze.neighbors(cur)) {
            if (parent[next] < 0) {
                parent[next] = cur;
                queue.enqueue(next);
            }
        }
    }

    if (parent[newEdge.to] < 0) return;

    QVector<int> cyclePath;
    for (int cell = newEdge.to;; cell = parent[cell]) {
        cyclePath.append(cell);
        if (cell == newEdge.from) break;
    }

    if (cyclePath.size() < 3) return;

    std::uniform_int_distribution<int> edgeDist(0, cyclePath.size() - 2);
    int removeIdx = edgeDist(rng_);
    int removeA = cyclePath[removeIdx];
    int removeB = cyclePath[removeIdx + 1];

    QSet<quint64> newEdgeSet = edgeSet;
    newEdgeSet.remove(edgeKey(removeA, removeB));
    newEdgeSet.insert(edgeKey(newEdge.from, newEdge.to));

    QVector<MazeEdge> newEdges;
    newEdges.reserve(edges.size());
    for (const auto &key : newEdgeSet) {
        int from = static_cast<int>(key >> 32);
        int to = static_cast<int>(key & 0xFFFFFFFF);
        newEdges.append({from, to});
    }

    chrom.maze.setFromEdges(rows, cols, newEdges, rng_());
    if (config_.useAdversarialPlacement) {
        ResourcePlacerConfig pc;
        pc.coinCount = config_.coinCount;
        pc.trapCount = config_.trapCount;
        pc.seed = rng_();
        ResourcePlacer::placeAdversarial(chrom.maze, pc);
    } else if (config_.useSmartPlacement) {
        ResourcePlacerConfig pc;
        pc.coinCount = config_.coinCount;
        pc.trapCount = config_.trapCount;
        pc.seed = rng_();
        ResourcePlacer::placeSmart(chrom.maze, pc);
    } else {
        chrom.maze.placeResources(config_.coinCount, config_.trapCount, rng_());
    }
}

MazeOptimizer::Chromosome MazeOptimizer::tournamentSelect(
    const QVector<Chromosome> &population) {
    std::uniform_int_distribution<int> dist(0, population.size() - 1);
    const Chromosome *best = nullptr;
    for (int i = 0; i < config_.tournamentSize; ++i) {
        const Chromosome &candidate = population[dist(rng_)];
        if (!best || candidate.fitness > best->fitness) {
            best = &candidate;
        }
    }
    return *best;
}

void MazeOptimizer::edgeSwap(MazeModel &maze, std::mt19937 &rng) {
    const int rows = maze.rows();
    const int cols = maze.columns();
    const int totalCells = rows * cols;
    QVector<MazeEdge> edges = maze.allEdges();

    auto edgeKey = [](int a, int b) -> quint64 {
        int lo = std::min(a, b), hi = std::max(a, b);
        return (static_cast<quint64>(lo) << 32) | static_cast<quint32>(hi);
    };

    QSet<quint64> edgeSet;
    for (const MazeEdge &e : edges) {
        edgeSet.insert(edgeKey(e.from, e.to));
    }

    QVector<MazeEdge> wallCandidates;
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

    if (wallCandidates.empty()) return;

    std::shuffle(wallCandidates.begin(), wallCandidates.end(), rng);
    const MazeEdge newEdge = wallCandidates.first();

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

    if (parent[newEdge.to] < 0) return;

    QVector<int> cyclePath;
    for (int cell = newEdge.to;; cell = parent[cell]) {
        cyclePath.append(cell);
        if (cell == newEdge.from) break;
    }

    if (cyclePath.size() < 3) return;

    std::uniform_int_distribution<int> edgeDist(0, cyclePath.size() - 2);
    int removeIdx = edgeDist(rng);
    int removeA = cyclePath[removeIdx];
    int removeB = cyclePath[removeIdx + 1];

    QSet<quint64> newEdgeSet = edgeSet;
    newEdgeSet.remove(edgeKey(removeA, removeB));
    newEdgeSet.insert(edgeKey(newEdge.from, newEdge.to));

    QVector<MazeEdge> newEdges;
    newEdges.reserve(edges.size());
    for (const auto &key : newEdgeSet) {
        int from = static_cast<int>(key >> 32);
        int to = static_cast<int>(key & 0xFFFFFFFF);
        newEdges.append({from, to});
    }

    maze.setFromEdges(rows, cols, newEdges, rng());
}

MazeModel MazeOptimizer::crossover(const MazeModel &a, const MazeModel &b, std::mt19937 &rng) {
    const int rows = a.rows();
    const int cols = a.columns();
    const int totalCells = rows * cols;
    const int targetEdges = totalCells - 1;

    QVector<MazeEdge> edgesA = a.allEdges();
    QVector<MazeEdge> edgesB = b.allEdges();

    QVector<MazeEdge> combined;
    combined.reserve(edgesA.size() + edgesB.size());
    combined += edgesA;
    combined += edgesB;
    std::shuffle(combined.begin(), combined.end(), rng);

    QVector<int> ufParent(totalCells);
    QVector<int> ufRank(totalCells, 0);
    std::iota(ufParent.begin(), ufParent.end(), 0);

    std::function<int(int)> find = [&](int x) -> int {
        if (ufParent[x] != x) ufParent[x] = find(ufParent[x]);
        return ufParent[x];
    };
    auto unite = [&](int u, int v) -> bool {
        int ru = find(u), rv = find(v);
        if (ru == rv) return false;
        if (ufRank[ru] < ufRank[rv]) std::swap(ru, rv);
        ufParent[rv] = ru;
        if (ufRank[ru] == ufRank[rv]) ++ufRank[ru];
        return true;
    };

    QVector<MazeEdge> childEdges;
    childEdges.reserve(targetEdges);
    for (const MazeEdge &e : combined) {
        if (unite(e.from, e.to)) {
            childEdges.append(e);
            if (childEdges.size() == targetEdges) break;
        }
    }

    for (int cell = 0; childEdges.size() < targetEdges && cell < totalCells; ++cell) {
        int row = cell / cols;
        int col = cell % cols;
        if (col + 1 < cols) {
            int right = cell + 1;
            if (unite(cell, right))
                childEdges.append({std::min(cell, right), std::max(cell, right)});
        }
        if (row + 1 < rows) {
            int down = cell + cols;
            if (unite(cell, down))
                childEdges.append({std::min(cell, down), std::max(cell, down)});
        }
    }

    MazeModel child;
    child.setFromEdges(rows, cols, childEdges, rng());
    return child;
}
