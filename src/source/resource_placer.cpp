#include "resource_placer.h"

void ResourcePlacer::placeSmart(MazeModel &maze, const ResourcePlacerConfig &config) {
    if (maze.cellCount() <= 2) return;

    std::mt19937 rng(config.seed);
    auto topo = maze.analyzeTopology();

    QVector<int> deadEnds, junctions, branchDeep, mainPathCells, allCells;
    for (int cell = 0; cell < maze.cellCount(); ++cell) {
        if (cell == maze.startCell() || cell == maze.endCell() || cell == maze.bossCell())
            continue;
        allCells.append(cell);
        if (topo[cell].isDeadEnd) deadEnds.append(cell);
        else if (topo[cell].isJunction && !topo[cell].onMainPath) junctions.append(cell);
        if (!topo[cell].onMainPath && topo[cell].branchDepth >= 2) branchDeep.append(cell);
        if (topo[cell].onMainPath) mainPathCells.append(cell);
    }

    QVector<int> resources(maze.cellCount(), 0);

    // === Place coins ===
    int coinsLeft = std::min(config.coinCount, static_cast<int>(allCells.size()));

    int deadEndCoins = std::min(
        static_cast<int>(coinsLeft * config.deadEndCoinRatio),
        static_cast<int>(deadEnds.size()));
    std::shuffle(deadEnds.begin(), deadEnds.end(), rng);
    for (int i = 0; i < deadEndCoins && coinsLeft > 0; ++i) {
        resources[deadEnds[i]] = 50;
        --coinsLeft;
    }

    int branchCoins = std::min(coinsLeft / 2, static_cast<int>(branchDeep.size()));
    std::shuffle(branchDeep.begin(), branchDeep.end(), rng);
    for (int i = 0; i < branchCoins && coinsLeft > 0; ++i) {
        if (resources[branchDeep[i]] == 0) {
            resources[branchDeep[i]] = 50;
            --coinsLeft;
        }
    }

    QVector<int> remaining;
    for (int cell : allCells) {
        if (resources[cell] == 0) remaining.append(cell);
    }
    std::shuffle(remaining.begin(), remaining.end(), rng);
    for (int i = 0; i < coinsLeft && i < remaining.size(); ++i) {
        resources[remaining[i]] = 50;
    }

    // === Place traps ===
    int trapsLeft = std::min(config.trapCount, static_cast<int>(allCells.size()));
    QVector<int> trapCandidates;
    for (int cell : allCells) {
        if (resources[cell] == 0) trapCandidates.append(cell);
    }

    int junctionTraps = std::min(
        static_cast<int>(trapsLeft * config.branchTrapRatio),
        static_cast<int>(junctions.size()));
    std::shuffle(junctions.begin(), junctions.end(), rng);
    for (int i = 0; i < junctionTraps && trapsLeft > 0; ++i) {
        if (resources[junctions[i]] == 0) {
            resources[junctions[i]] = -30;
            --trapsLeft;
        }
    }

    int mainTraps = std::min(
        static_cast<int>(trapsLeft * config.mainPathTrapRatio),
        static_cast<int>(mainPathCells.size()));
    std::shuffle(mainPathCells.begin(), mainPathCells.end(), rng);
    for (int i = 0; i < mainTraps && trapsLeft > 0; ++i) {
        if (resources[mainPathCells[i]] == 0) {
            resources[mainPathCells[i]] = -30;
            --trapsLeft;
        }
    }

    QVector<int> trapRemaining;
    for (int cell : trapCandidates) {
        if (resources[cell] == 0) trapRemaining.append(cell);
    }
    std::shuffle(trapRemaining.begin(), trapRemaining.end(), rng);
    for (int i = 0; i < trapsLeft && i < trapRemaining.size(); ++i) {
        resources[trapRemaining[i]] = -30;
    }

    maze.setResources(resources);
}

void ResourcePlacer::placeRandom(MazeModel &maze, int coinCount, int trapCount, quint32 seed) {
    maze.placeResources(coinCount, trapCount, seed);
}
