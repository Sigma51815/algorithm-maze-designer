#include "resource_placer.h"

namespace {

int normalizedTrapValue(int value) {
    return value < 0 ? value : -std::max(1, value);
}

} // namespace

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
        resources[deadEnds[i]] = config.coinValue;
        --coinsLeft;
    }

    int branchCoins = std::min(coinsLeft / 2, static_cast<int>(branchDeep.size()));
    std::shuffle(branchDeep.begin(), branchDeep.end(), rng);
    for (int i = 0; i < branchCoins && coinsLeft > 0; ++i) {
        if (resources[branchDeep[i]] == 0) {
            resources[branchDeep[i]] = config.coinValue;
            --coinsLeft;
        }
    }

    QVector<int> remaining;
    for (int cell : allCells) {
        if (resources[cell] == 0) remaining.append(cell);
    }
    std::shuffle(remaining.begin(), remaining.end(), rng);
    for (int i = 0; i < coinsLeft && i < remaining.size(); ++i) {
        resources[remaining[i]] = config.coinValue;
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
            resources[junctions[i]] = normalizedTrapValue(config.trapValue);
            --trapsLeft;
        }
    }

    int mainTraps = std::min(
        static_cast<int>(trapsLeft * config.mainPathTrapRatio),
        static_cast<int>(mainPathCells.size()));
    std::shuffle(mainPathCells.begin(), mainPathCells.end(), rng);
    for (int i = 0; i < mainTraps && trapsLeft > 0; ++i) {
        if (resources[mainPathCells[i]] == 0) {
            resources[mainPathCells[i]] = normalizedTrapValue(config.trapValue);
            --trapsLeft;
        }
    }

    QVector<int> trapRemaining;
    for (int cell : trapCandidates) {
        if (resources[cell] == 0) trapRemaining.append(cell);
    }
    std::shuffle(trapRemaining.begin(), trapRemaining.end(), rng);
    for (int i = 0; i < trapsLeft && i < trapRemaining.size(); ++i) {
        resources[trapRemaining[i]] = normalizedTrapValue(config.trapValue);
    }

    maze.setResources(resources);
}

void ResourcePlacer::placeAdversarial(MazeModel &maze, const ResourcePlacerConfig &config) {
    if (maze.cellCount() <= 2) return;

    std::mt19937 rng(config.seed);
    auto topo = maze.analyzeTopology();

    QVector<int> deadEnds, junctions, branchDeep, mainPathCells, allCells;
    for (int cell = 0; cell < maze.cellCount(); ++cell) {
        if (cell == maze.startCell() || cell == maze.endCell() || cell == maze.bossCell())
            continue;
        allCells.append(cell);
        if (topo[cell].isDeadEnd) deadEnds.append(cell);
        else if (topo[cell].isJunction) junctions.append(cell);
        if (!topo[cell].onMainPath && topo[cell].branchDepth >= 2) branchDeep.append(cell);
        if (topo[cell].onMainPath) mainPathCells.append(cell);
    }

    QVector<int> resources(maze.cellCount(), 0);

    // === 对抗式陷阱分布 ===
    // 陷阱放在分叉口（迫使AI在探索时踩到）和主路径上
    int trapsLeft = std::min(config.trapCount, static_cast<int>(allCells.size()));

    // 70% 陷阱放分叉口（包括主路径分叉口）
    int junctionTraps = std::min(
        static_cast<int>(trapsLeft * 0.7),
        static_cast<int>(junctions.size()));
    std::shuffle(junctions.begin(), junctions.end(), rng);
    for (int i = 0; i < junctionTraps && trapsLeft > 0; ++i) {
        resources[junctions[i]] = normalizedTrapValue(config.trapValue);
        --trapsLeft;
    }

    // 剩余陷阱随机放
    QVector<int> trapRemaining;
    for (int cell : allCells) {
        if (resources[cell] == 0) trapRemaining.append(cell);
    }
    std::shuffle(trapRemaining.begin(), trapRemaining.end(), rng);
    for (int i = 0; i < trapsLeft && i < trapRemaining.size(); ++i) {
        resources[trapRemaining[i]] = normalizedTrapValue(config.trapValue);
    }

    // === 对抗式金币分布 ===
    // 金币放在远离主路径的深处（迫使AI走冤枉路）
    // 不放死胡同（太容易拿），放分支深处
    int coinsLeft = std::min(config.coinCount, static_cast<int>(allCells.size()));

    // 优先放分支深处（depth >= 3）
    QVector<int> veryDeep;
    for (int cell : branchDeep) {
        if (topo[cell].branchDepth >= 3 && resources[cell] == 0)
            veryDeep.append(cell);
    }
    std::shuffle(veryDeep.begin(), veryDeep.end(), rng);
    for (int i = 0; i < coinsLeft && i < veryDeep.size(); ++i) {
        resources[veryDeep[i]] = config.coinValue;
        --coinsLeft;
    }

    // 再放普通分支深处
    std::shuffle(branchDeep.begin(), branchDeep.end(), rng);
    for (int i = 0; i < branchDeep.size() && coinsLeft > 0; ++i) {
        if (resources[branchDeep[i]] == 0) {
            resources[branchDeep[i]] = config.coinValue;
            --coinsLeft;
        }
    }

    // 最后随机放
    QVector<int> coinRemaining;
    for (int cell : allCells) {
        if (resources[cell] == 0) coinRemaining.append(cell);
    }
    std::shuffle(coinRemaining.begin(), coinRemaining.end(), rng);
    for (int i = 0; i < coinsLeft && i < coinRemaining.size(); ++i) {
        resources[coinRemaining[i]] = config.coinValue;
    }

    maze.setResources(resources);
}

void ResourcePlacer::placeRandom(MazeModel &maze, int coinCount, int trapCount, quint32 seed,
                                 int coinValue, int trapValue) {
    maze.placeResources(coinCount, trapCount, seed, coinValue, trapValue);
}
