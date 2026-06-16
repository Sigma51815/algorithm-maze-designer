#include "ai/greedy_player.h"
#include "ai/rl_player.h"
#include "ai/aiplayerformat.h"
#include "bosssolver.h"
#include "battlewindow.h"
#include "coevolution.h"
#include "mainwindow.h"
#include "maze.h"
#include "maze_evaluator.h"
#include "maze_optimizer.h"
#include "qlearning_optimizer.h"
#include "resource_placer.h"

#include <QApplication>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QQueue>
#include <QPushButton>
#include <QSet>
#include <QSaveFile>
#include <QTextStream>
#include <QTimer>

#include <cstdlib>
#include <cstring>
#include <limits>

namespace {

int shortestPathLength(const MazeModel &maze) {
    QVector<int> distance(maze.cellCount(), -1);
    QQueue<int> queue;
    distance[maze.startCell()] = 0;
    queue.enqueue(maze.startCell());
    while (!queue.isEmpty()) {
        const int current = queue.dequeue();
        for (int next : maze.neighbors(current)) {
            if (distance[next] >= 0) {
                continue;
            }
            distance[next] = distance[current] + 1;
            queue.enqueue(next);
        }
    }
    return distance[maze.endCell()];
}

int longestInternalWallRun(const MazeModel &maze) {
    const QStringList grid = maze.expandedGrid();
    int longest = 0;
    auto measure = [&](const QString &line) {
        int current = 0;
        for (int i = 1; i + 1 < line.size(); ++i) {
            if (line[i] == QLatin1Char('#')) {
                longest = std::max(longest, ++current);
            } else {
                current = 0;
            }
        }
    };
    for (int row = 1; row + 1 < grid.size(); ++row) {
        measure(grid[row]);
    }
    for (int column = 1; column + 1 < grid.first().size(); ++column) {
        QString vertical;
        vertical.reserve(grid.size());
        for (const QString &line : grid) {
            vertical.append(line[column]);
        }
        measure(vertical);
    }
    return longest;
}

int bruteForceResourceMaximum(const MazeModel &maze) {
    const int count = maze.cellCount();
    if (count > 20) {
        return 0;
    }
    const quint64 startBit = quint64{1} << maze.startCell();
    const quint64 endBit = quint64{1} << maze.endCell();
    int best = std::numeric_limits<int>::min();
    for (quint64 mask = 0; mask < (quint64{1} << count); ++mask) {
        if ((mask & startBit) == 0 || (mask & endBit) == 0) {
            continue;
        }
        QQueue<int> queue;
        QSet<int> reached;
        queue.enqueue(maze.startCell());
        reached.insert(maze.startCell());
        while (!queue.isEmpty()) {
            const int current = queue.dequeue();
            for (int next : maze.neighbors(current)) {
                if ((mask & (quint64{1} << next)) != 0 && !reached.contains(next)) {
                    reached.insert(next);
                    queue.enqueue(next);
                }
            }
        }
        int selectedCount = 0;
        for (quint64 bits = mask; bits != 0; bits >>= 1) {
            selectedCount += static_cast<int>(bits & 1U);
        }
        if (reached.size() != selectedCount) {
            continue;
        }
        int value = 0;
        for (int cell : reached) {
            if (cell != maze.startCell() && cell != maze.endCell()) {
                value += maze.resourceAt(cell);
            }
        }
        best = std::max(best, value);
    }
    return best;
}

int bruteForceBossTurns(const QVector<int> &initialHealth,
                        const QVector<BossSkill> &skills) {
    struct State {
        QVector<int> health;
        QVector<int> cooldowns;
        int turns = 0;
    };
    auto keyOf = [](const State &state) {
        QString key;
        for (int value : state.health) {
            key += QString::number(std::max(0, value)) + QLatin1Char(',');
        }
        key += QLatin1Char('|');
        for (int value : state.cooldowns) {
            key += QString::number(value) + QLatin1Char(',');
        }
        return key;
    };
    auto firstLiving = [](const QVector<int> &health) {
        for (int i = 0; i < health.size(); ++i) {
            if (health[i] > 0) {
                return i;
            }
        }
        return -1;
    };

    QQueue<State> queue;
    QSet<QString> visited;
    queue.enqueue({initialHealth, QVector<int>(skills.size(), 0), 0});
    while (!queue.isEmpty()) {
        State state = queue.dequeue();
        const int boss = firstLiving(state.health);
        if (boss < 0) {
            return state.turns;
        }
        for (int skillIndex = 0; skillIndex < skills.size(); ++skillIndex) {
            if (state.cooldowns[skillIndex] > 0) {
                continue;
            }
            State next = state;
            next.health[boss] -= skills[skillIndex].damage;
            for (int &cooldown : next.cooldowns) {
                cooldown = std::max(0, cooldown - 1);
            }
            next.cooldowns[skillIndex] = skills[skillIndex].cooldown;
            ++next.turns;
            const QString key = keyOf(next);
            if (!visited.contains(key)) {
                visited.insert(key);
                queue.enqueue(std::move(next));
            }
        }
    }
    return -1;
}

int runSelfTests() {
    QTextStream output(stdout);
    const QVector<MazeAlgorithm> algorithms{
        MazeAlgorithm::DivideAndConquer,
        MazeAlgorithm::KruskalMst,
        MazeAlgorithm::DepthFirstSearch,
        MazeAlgorithm::BreadthFirstSearch};

    for (int i = 0; i < algorithms.size(); ++i) {
        MazeModel maze;
        maze.generate(15, 15, algorithms[i], static_cast<quint32>(1000 + i));
        QString reason;
        if (!maze.validatePerfect(&reason)) {
            output << "FAIL maze algorithm " << i << ": " << reason << '\n';
            return 1;
        }
        maze.placeResources(30, 20, static_cast<quint32>(2000 + i));
        const ResourcePlan plan = maze.optimalResourceWalk();
        if (plan.walk.isEmpty() || plan.walk.first() != maze.startCell()
            || plan.walk.last() != maze.endCell()) {
            output << "FAIL resource path endpoints for algorithm " << i << '\n';
            return 2;
        }
        QSet<int> visited;
        for (int step = 0; step < plan.walk.size(); ++step) {
            visited.insert(plan.walk[step]);
            if (step > 0 && !maze.isOpen(plan.walk[step - 1], plan.walk[step])) {
                output << "FAIL illegal resource path step for algorithm " << i << '\n';
                return 3;
            }
        }
        int value = 0;
        for (int cell : visited) {
            if (cell != maze.startCell() && cell != maze.endCell()) {
                value += maze.resourceAt(cell);
            }
        }
        if (value != plan.maxValue) {
            output << "FAIL resource score mismatch for algorithm " << i << '\n';
            return 4;
        }
        output << "PASS maze algorithm " << i << ": " << reason
               << ", route=" << shortestPathLength(maze)
               << ", wall-run=" << longestInternalWallRun(maze)
               << ", resource=" << plan.maxValue << '\n';
    }

    for (int i = 0; i < algorithms.size(); ++i) {
        MazeModel maze;
        maze.generate(4, 4, algorithms[i], static_cast<quint32>(3000 + i));
        maze.placeResources(5, 5, static_cast<quint32>(4000 + i));
        const ResourcePlan plan = maze.optimalResourceWalk();
        const int bruteForceValue = bruteForceResourceMaximum(maze);
        if (plan.maxValue != bruteForceValue) {
            output << "FAIL DP optimality for algorithm " << i
                   << ": dp=" << plan.maxValue << ", brute=" << bruteForceValue << '\n';
            return 7;
        }
        output << "PASS DP optimality algorithm " << i
               << ": value=" << plan.maxValue << '\n';
    }

    const int directDistance = 15 + 15 - 2;
    for (quint32 seed : {202506U, 7U, 42U, 1003U, 65537U}) {
        MazeModel maze;
        maze.generate(15, 15, MazeAlgorithm::BreadthFirstSearch, seed);
        QString reason;
        const int routeLength = shortestPathLength(maze);
        const int wallRun = longestInternalWallRun(maze);
        if (!maze.validatePerfect(&reason) || routeLength <= directDistance
            || wallRun > 22) {
            output << "FAIL BFS maze quality for seed " << seed
                   << ": route=" << routeLength << ", wall-run=" << wallRun
                   << ", " << reason << '\n';
            return 6;
        }
        output << "PASS BFS quality seed " << seed << ": route=" << routeLength
               << ", wall-run=" << wallRun << '\n';
    }

    const QVector<int> bosses{35, 45, 60};
    const QVector<BossSkill> skills{{QStringLiteral("Normal"), 5, 0},
                                    {QStringLiteral("Heavy"), 10, 2},
                                    {QStringLiteral("Ultimate"), 18, 4}};
    const BossResult bossResult = BossSolver::solve(bosses, skills);
    if (!bossResult.solved
        || !BossSolver::verify(bosses, skills, bossResult.skillSequence)
        || bossResult.minimumTurns != bossResult.skillSequence.size()) {
        output << "FAIL boss solver\n";
        return 5;
    }
    const QVector<int> smallBosses{12, 15};
    const QVector<BossSkill> smallSkills{{QStringLiteral("Normal"), 3, 0},
                                         {QStringLiteral("Burst"), 7, 2}};
    const BossResult smallResult = BossSolver::solve(smallBosses, smallSkills);
    const int bruteForceTurns = bruteForceBossTurns(smallBosses, smallSkills);
    if (!smallResult.solved || smallResult.minimumTurns != bruteForceTurns) {
        output << "FAIL boss optimality: branch-bound=" << smallResult.minimumTurns
               << ", bfs=" << bruteForceTurns << '\n';
        return 8;
    }
    output << "PASS boss optimality: turns=" << bruteForceTurns << '\n';
    output << "PASS boss solver: turns=" << bossResult.minimumTurns
           << ", expanded=" << bossResult.expandedStates
           << ", pruned=" << bossResult.prunedStates << '\n';

    {
        MazeModel bossMaze;
        bossMaze.generate(5, 5, MazeAlgorithm::KruskalMst, 50000U);
        bossMaze.placeResources(5, 3, 50001U);
        BossFullResult fullResult = BossSolver::solveWithMaze(bossMaze, bosses, skills, 2);
        if (!fullResult.solved) {
            output << "FAIL solveWithMaze: not solved\n";
            return 30;
        }
        if (fullResult.roundLimit != fullResult.minimumTurns + 2) {
            output << "FAIL solveWithMaze: roundLimit=" << fullResult.roundLimit
                   << " expected=" << (fullResult.minimumTurns + 2) << '\n';
            return 31;
        }
        if (fullResult.coinConsumption < 1) {
            output << "FAIL solveWithMaze: coinConsumption=" << fullResult.coinConsumption << '\n';
            return 32;
        }
        if (fullResult.maxCoinsFromDP <= 0) {
            output << "FAIL solveWithMaze: maxCoinsFromDP=" << fullResult.maxCoinsFromDP << '\n';
            return 33;
        }
        output << "PASS solveWithMaze: minTurns=" << fullResult.minimumTurns
               << ", roundLimit=" << fullResult.roundLimit
               << ", coinCost=" << fullResult.coinConsumption
               << ", dpCoins=" << fullResult.maxCoinsFromDP << '\n';
    }

    MazeModel contractMaze;
    contractMaze.generate(7, 7, MazeAlgorithm::KruskalMst, 202506U);
    contractMaze.placeResources(8, 6, 202507U);
    const QJsonObject contract = buildAiPlayerInput(
        contractMaze, bosses, skills, 20, 5);
    const QByteArray serializedContract = serializeAiPlayerInput(
        contractMaze, bosses, skills, 20, 5);
    const QSet<QString> expectedKeys{QStringLiteral("maze"), QStringLiteral("B"),
                                     QStringLiteral("PlayerSkills"),
                                     QStringLiteral("minRouds"),
                                     QStringLiteral("CoinConsumption")};
    QSet<QString> actualKeys;
    for (const QString &key : contract.keys()) {
        actualKeys.insert(key);
    }
    if (actualKeys != expectedKeys) {
        output << "FAIL AI JSON keys\n";
        return 9;
    }
    const QJsonArray matrix = contract.value(QStringLiteral("maze")).toArray();
    int startMarkers = 0;
    int endMarkers = 0;
    int bossMarkers = 0;
    bool matrixValid = matrix.size() == 15;
    for (const QJsonValue &rowValue : matrix) {
        const QJsonArray row = rowValue.toArray();
        matrixValid = matrixValid && row.size() == 15;
        for (const QJsonValue &cellValue : row) {
            const QString cell = cellValue.toString();
            matrixValid = matrixValid && cell.size() == 1 && cell != QStringLiteral(".");
            startMarkers += cell == QStringLiteral("S");
            endMarkers += cell == QStringLiteral("E");
            bossMarkers += cell == QStringLiteral("B");
        }
    }
    if (!matrixValid || startMarkers != 1 || endMarkers != 1 || bossMarkers != 1
        || contract.value(QStringLiteral("B")).toArray().size() != bosses.size()
        || contract.value(QStringLiteral("PlayerSkills")).toArray().size() != skills.size()) {
        output << "FAIL AI JSON contract\n";
        return 10;
    }
    QJsonParseError parseError;
    const QJsonDocument parsedContract = QJsonDocument::fromJson(
        serializedContract, &parseError);
    const int mazePosition = serializedContract.indexOf("\"maze\"");
    const int bossesPosition = serializedContract.indexOf("\"B\"");
    const int skillsPosition = serializedContract.indexOf("\"PlayerSkills\"");
    const int roundsPosition = serializedContract.indexOf("\"minRouds\"");
    const int coinsPosition = serializedContract.indexOf("\"CoinConsumption\"");
    if (parseError.error != QJsonParseError::NoError || !parsedContract.isObject()
        || !(mazePosition < bossesPosition && bossesPosition < skillsPosition
             && skillsPosition < roundsPosition && roundsPosition < coinsPosition)
        || !serializedContract.contains("[\"#\",\"#\",\"#\"")) {
        output << "FAIL AI JSON serialization format\n";
        return 12;
    }
    const QByteArray previewPath = qgetenv("AI_JSON_PREVIEW_PATH");
    if (!previewPath.isEmpty()) {
        QSaveFile preview(QString::fromLocal8Bit(previewPath));
        if (!preview.open(QIODevice::WriteOnly)
            || preview.write(serializedContract) < 0
            || !preview.commit()) {
            output << "FAIL writing AI JSON preview\n";
            return 11;
        }
    }
    output << "PASS AI JSON contract: exact field order and compact 15x15 rows\n";

    {
        MazeModel original;
        original.generate(5, 5, MazeAlgorithm::KruskalMst, 80000U);
        original.placeResources(5, 3, 80001U);
        original.setBossCell(original.cellCount() / 2);
        const QJsonObject exported = original.toJson();
        const QJsonArray matrix = exported.value(QStringLiteral("expandedMatrix")).toArray();

        MazeModel loaded;
        QString err;
        bool ok = MazeModel::fromExpandedGrid(matrix, loaded, &err);
        if (!ok) {
            output << "FAIL fromExpandedGrid: " << err << '\n';
            return 40;
        }
        if (loaded.rows() != original.rows() || loaded.columns() != original.columns()) {
            output << "FAIL fromExpandedGrid: dimension mismatch\n";
            return 41;
        }
        if (loaded.startCell() != original.startCell()
            || loaded.endCell() != original.endCell()) {
            output << "FAIL fromExpandedGrid: start/end mismatch\n";
            return 42;
        }
        ResourcePlan loadedDp = loaded.optimalResourceWalk();
        ResourcePlan originalDp = original.optimalResourceWalk();
        if (loadedDp.maxValue != originalDp.maxValue) {
            output << "FAIL fromExpandedGrid: dp mismatch " << loadedDp.maxValue
                   << " vs " << originalDp.maxValue << '\n';
            return 43;
        }
        output << "PASS fromExpandedGrid: " << loaded.rows() << "x" << loaded.columns()
               << ", dp=" << loadedDp.maxValue << '\n';
    }

    {
        MazeModel maze;
        maze.generate(15, 15, MazeAlgorithm::DepthFirstSearch, static_cast<quint32>(42));
        maze.placeResources(28, 18, static_cast<quint32>(42));
        PlayResult result = GreedyPlayer::play(maze);
        if (!result.reachedEnd || result.totalSteps <= 0) {
            output << "FAIL greedy AI\n";
            return 8;
        }
        output << QString::asprintf("PASS greedy AI (score=%.2f, steps=%d, coins=%d, traps=%d)\n",
                                    result.score, result.totalSteps, result.collectedCoins, result.triggeredTraps);
    }

    {
        MazeModel bigMaze;
        bigMaze.generate(15, 15, MazeAlgorithm::DepthFirstSearch, static_cast<quint32>(42));
        bigMaze.placeResources(28, 18, static_cast<quint32>(43));
        int totalValue = 0, totalSteps = 0, caseCount = 0;
        const QVector<int> testCells{38, 56, 112, 168, 196};
        for (int cell : testCells) {
            if (cell >= bigMaze.cellCount()) continue;
            MazeModel sub = bigMaze.extractSubArea(cell);
            if (sub.cellCount() == 0) continue;
            PlayResult result = GreedyPlayer::play(sub);
            totalValue += result.remainingResource;
            totalSteps += result.totalSteps;
            ++caseCount;
        }
        if (caseCount == 0) { output << "FAIL 3x3\n"; return 10; }
        const double avg = totalSteps > 0 ? static_cast<double>(totalValue) / totalSteps : 0.0;
        output << QString::asprintf("PASS 3x3 local test (%d cases, avg=%.2f)\n", caseCount, avg);
    }

    {
        MazeModel maze;
        maze.generate(15, 15, MazeAlgorithm::DepthFirstSearch, static_cast<quint32>(42));
        maze.placeResources(28, 18, static_cast<quint32>(42));
        const QVector<int> bossHealth{35, 45};
        const QVector<BossSkill> bossSkills{{QStringLiteral("Normal"), 5, 0},
                                             {QStringLiteral("Heavy"), 10, 2},
                                             {QStringLiteral("Ultimate"), 18, 4}};
        PlayResult result = GreedyPlayer::play(maze, bossHealth, bossSkills, 100);
        if (!result.reachedEnd) { output << "FAIL BOSS battle\n"; return 11; }
        output << QString::asprintf("PASS BOSS battle (defeated=%s, attempts=%d)\n",
                                    result.bossDefeated ? "true" : "false", result.bossAttempts);
    }

    {
        MazeModel baseMaze;
        baseMaze.generate(5, 5, MazeAlgorithm::KruskalMst, 9000U);
        baseMaze.placeResources(5, 3, 9001U);
        const ResourcePlan dpPlan = baseMaze.optimalResourceWalk();

        int worstGreedy = std::numeric_limits<int>::max();
        const QVector<GreedyStrategy> strategies = {
            GreedyStrategy::ValuePerStep,
            GreedyStrategy::NearestFirst,
            GreedyStrategy::AvoidTraps,
            GreedyStrategy::EndGoalFirst
        };
        for (GreedyStrategy s : strategies) {
            PlayResult result = GreedyPlayer::play(baseMaze, {}, {}, 0, s);
            worstGreedy = std::min(worstGreedy, result.remainingResource);
        }
        const int baseRegret = dpPlan.maxValue - worstGreedy;

        OptimizerConfig cfg;
        cfg.rows = 5;
        cfg.columns = 5;
        cfg.populationSize = 10;
        cfg.generations = 20;
        cfg.mutationRate = 0.4;
        cfg.tournamentSize = 3;
        cfg.baseAlgorithm = MazeAlgorithm::KruskalMst;
        cfg.seed = 9000U;
        cfg.coinCount = 5;
        cfg.trapCount = 3;

        MazeOptimizer optimizer;
        optimizer.setConfig(cfg);
        const MazeModel bestMaze = optimizer.run();

        if (!bestMaze.validatePerfect()) {
            output << "FAIL GA produced invalid maze\n";
            return 16;
        }
        const ResourcePlan optDp = bestMaze.optimalResourceWalk();
        int optWorstGreedy = std::numeric_limits<int>::max();
        for (GreedyStrategy s : strategies) {
            PlayResult result = GreedyPlayer::play(bestMaze, {}, {}, 0, s);
            optWorstGreedy = std::min(optWorstGreedy, result.remainingResource);
        }
        const int optRegret = optDp.maxValue - optWorstGreedy;
        output << "PASS GA optimizer: base=" << baseRegret
               << ", best=" << optRegret
               << ", dp=" << optDp.maxValue << '\n';
    }

    {
        MazeModel trainMaze;
        trainMaze.generate(15, 15, MazeAlgorithm::KruskalMst, 11000U);
        trainMaze.placeResources(30, 20, 11001U);

        RLPlayer rlPlayer;
        RLConfig rlCfg;
        rlCfg.trainEpisodes = 10000;
        rlCfg.playMaxSteps = 800;
        rlCfg.coinCount = 30;
        rlCfg.trapCount = 20;

        const RLPlayResult before = rlPlayer.play(trainMaze, rlCfg);
        rlPlayer.trainOnMaze(trainMaze, rlCfg);
        const RLPlayResult after = rlPlayer.play(trainMaze, rlCfg);

        MazeModel testMaze;
        testMaze.generate(15, 15, MazeAlgorithm::KruskalMst, 11000U);
        testMaze.placeResources(30, 20, 55000U);
        const RLPlayResult testResult = rlPlayer.play(testMaze, rlCfg);

        MazeModel genMaze;
        genMaze.generate(15, 15, MazeAlgorithm::DepthFirstSearch, 22000U);
        genMaze.placeResources(30, 20, 22001U);
        const RLPlayResult genResult = rlPlayer.play(genMaze, rlCfg);

        if (after.totalResource < before.totalResource - 150) {
            output << "FAIL RL score decreased: before=" << before.totalResource
                   << ", after=" << after.totalResource << '\n';
            return 20;
        }
        output << "PASS RL player: before=" << before.totalResource
               << ", after=" << after.totalResource
               << ", steps=" << after.steps
               << ", sameMaze=" << testResult.totalResource
               << ", diffMaze=" << genResult.totalResource << '\n';
    }

    {
        CoEvolConfig cfg;
        cfg.cycles = 2;
        cfg.gaGenerations = 3;
        cfg.gaPopulation = 8;
        cfg.gaMutationRate = 0.3;
        cfg.rlTrainEpisodes = 200;
        cfg.rlTopK = 3;
        cfg.mazeRows = 15;
        cfg.mazeCols = 15;
        cfg.baseAlgorithm = MazeAlgorithm::KruskalMst;
        cfg.baseSeed = 12000U;
        cfg.coinCount = 5;
        cfg.trapCount = 3;

        CoEvolution coEvol;
        const CoEvolResult coResult = coEvol.run(cfg);

        if (!coResult.bestMaze.validatePerfect()) {
            output << "FAIL co-evolution produced invalid maze\n";
            return 21;
        }
        if (coResult.cycleBestFitness.isEmpty()) {
            output << "FAIL co-evolution no cycles completed\n";
            return 22;
        }
        output << "PASS co-evolution: cycles=" << coResult.cycleBestFitness.size()
               << ", bestFitness=" << coResult.bestFitness
               << ", lastRLScore=" << (coResult.rlScores.isEmpty() ? 0 : coResult.rlScores.last())
               << '\n';
    }

    // Verify the four base algorithms round-robin through algorithmForIndex,
    // and that useMixedAlgorithms seeds the GA population from all four
    // base algorithms instead of a single one.
    {
        QSet<int> seenEnumValues;
        for (int i = 0; i < 8; ++i) {
            seenEnumValues.insert(static_cast<int>(MazeOptimizer::algorithmForIndex(i)));
        }
        if (seenEnumValues.size() != 4) {
            output << "FAIL algorithmForIndex did not cover all 4 algorithms\n";
            return 23;
        }

        // Run a small GA with mixed algorithms; the best maze must still be
        // a valid perfect maze built on top of the four base algorithms.
        OptimizerConfig mixedCfg;
        mixedCfg.rows = 5;
        mixedCfg.columns = 5;
        mixedCfg.populationSize = 8;
        mixedCfg.generations = 5;
        mixedCfg.mutationRate = 0.3;
        mixedCfg.tournamentSize = 2;
        mixedCfg.baseAlgorithm = MazeAlgorithm::BreadthFirstSearch;
        mixedCfg.seed = 77000U;
        mixedCfg.coinCount = 5;
        mixedCfg.trapCount = 3;
        mixedCfg.useMixedAlgorithms = true;

        MazeOptimizer mixedOpt;
        mixedOpt.setConfig(mixedCfg);
        const MazeModel mixedBest = mixedOpt.run();
        QString mixedReason;
        if (!mixedBest.validatePerfect(&mixedReason)) {
            output << "FAIL mixed-algo GA produced invalid maze: " << mixedReason << '\n';
            return 24;
        }

        // Sanity: same config with useMixedAlgorithms=false should still
        // produce a valid maze (legacy single-algorithm path preserved).
        OptimizerConfig singleCfg = mixedCfg;
        singleCfg.useMixedAlgorithms = false;
        MazeOptimizer singleOpt;
        singleOpt.setConfig(singleCfg);
        const MazeModel singleBest = singleOpt.run();
        if (!singleBest.validatePerfect()) {
            output << "FAIL single-algo GA legacy path produced invalid maze\n";
            return 25;
        }

        const ResourcePlan mixedDp = mixedBest.optimalResourceWalk();
        output << "PASS four-algo mixed GA: regret=" << mixedDp.maxValue
               << ", validate=" << mixedReason
               << ", single-legacy-ok=" << singleBest.validatePerfect() << '\n';
    }

    {
        MazeModel maze;
        maze.generate(15, 15, MazeAlgorithm::DepthFirstSearch, 50000U);

        ResourcePlacerConfig pc;
        pc.coinCount = 10;
        pc.trapCount = 6;
        pc.seed = 50001U;
        ResourcePlacer::placeSmart(maze, pc);

        int coinCount = 0, trapCount = 0;
        for (int cell = 0; cell < maze.cellCount(); ++cell) {
            if (maze.resourceAt(cell) == 50) ++coinCount;
            if (maze.resourceAt(cell) == -30) ++trapCount;
        }
        if (coinCount != 10 || trapCount != 6) {
            output << "FAIL smart placement: coins=" << coinCount
                   << ", traps=" << trapCount << '\n';
            return 30;
        }

        auto topo = maze.analyzeTopology();
        int deadEndCoins = 0;
        for (int cell = 0; cell < maze.cellCount(); ++cell) {
            if (topo[cell].isDeadEnd && maze.resourceAt(cell) == 50) ++deadEndCoins;
        }
        if (deadEndCoins == 0) {
            output << "FAIL smart placement: no coins in dead ends\n";
            return 31;
        }

        output << "PASS smart resource placement: coins=" << coinCount
               << ", traps=" << trapCount << ", dead-end-coins=" << deadEndCoins << '\n';
    }

    {
        MazeModel maze;
        maze.generate(5, 5, MazeAlgorithm::KruskalMst, 60000U);
        maze.placeResources(5, 3, 60001U);

        EvaluatorConfig ec;
        ec.useSmartPlacement = false;
        ec.topoWeight = 0.3;
        EvalResult eval = MazeEvaluator::evaluate(maze, ec);

        if (eval.dpScore == 0) {
            output << "FAIL enhanced fitness: dpScore=0\n";
            return 32;
        }

        double topo = MazeEvaluator::computeTopoDifficulty(maze);
        if (topo < 0.0 || topo > 5.0) {
            output << "FAIL enhanced fitness: topoDifficulty=" << topo << '\n';
            return 33;
        }

        output << "PASS enhanced fitness: dp=" << eval.dpScore
               << ", regret=" << eval.regretGreedy
               << ", topo=" << topo
               << ", fitness=" << eval.finalFitness << '\n';
    }

    {
        OptimizerConfig baseCfg;
        baseCfg.rows = 7;
        baseCfg.columns = 7;
        baseCfg.populationSize = 12;
        baseCfg.generations = 20;
        baseCfg.mutationRate = 0.3;
        baseCfg.tournamentSize = 3;
        baseCfg.seed = 70000U;
        baseCfg.coinCount = 10;
        baseCfg.trapCount = 6;
        baseCfg.useMixedAlgorithms = true;

        OptimizerConfig randomCfg = baseCfg;
        randomCfg.useSmartPlacement = false;
        randomCfg.useEnhancedFitness = false;
        MazeOptimizer randomOpt;
        randomOpt.setConfig(randomCfg);
        MazeModel randomBest = randomOpt.run();
        ResourcePlan randomDp = randomBest.optimalResourceWalk();
        int randomGreedy = MazeEvaluator::evaluateGreedyWorst(randomBest);
        int randomRegret = randomDp.maxValue - randomGreedy;

        OptimizerConfig smartCfg = baseCfg;
        smartCfg.useSmartPlacement = true;
        smartCfg.useEnhancedFitness = true;
        smartCfg.topoWeight = 0.3;
        MazeOptimizer smartOpt;
        smartOpt.setConfig(smartCfg);
        MazeModel smartBest = smartOpt.run();
        ResourcePlan smartDp = smartBest.optimalResourceWalk();
        int smartGreedy = MazeEvaluator::evaluateGreedyWorst(smartBest);
        int smartRegret = smartDp.maxValue - smartGreedy;

        OptimizerConfig advCfg = baseCfg;
        advCfg.useSmartPlacement = false;
        advCfg.useAdversarialPlacement = true;
        advCfg.useEnhancedFitness = true;
        advCfg.topoWeight = 0.3;
        MazeOptimizer advOpt;
        advOpt.setConfig(advCfg);
        MazeModel advBest = advOpt.run();
        ResourcePlan advDp = advBest.optimalResourceWalk();
        int advGreedy = MazeEvaluator::evaluateGreedyWorst(advBest);
        int advRegret = advDp.maxValue - advGreedy;

        PlayResult randomPlay = GreedyPlayer::play(randomBest);
        PlayResult smartPlay = GreedyPlayer::play(smartBest);
        PlayResult advPlay = GreedyPlayer::play(advBest);

        output << "PASS GA comparison:\n"
               << "  random: dp=" << randomDp.maxValue << " greedy=" << randomGreedy
               << " regret=" << randomRegret
               << " ai_score=" << randomPlay.score
               << " ai_steps=" << randomPlay.totalSteps << '\n'
               << "  smart:  dp=" << smartDp.maxValue << " greedy=" << smartGreedy
               << " regret=" << smartRegret
               << " ai_score=" << smartPlay.score
               << " ai_steps=" << smartPlay.totalSteps << '\n'
               << "  adversarial: dp=" << advDp.maxValue << " greedy=" << advGreedy
               << " regret=" << advRegret
               << " ai_score=" << advPlay.score
               << " ai_steps=" << advPlay.totalSteps << '\n';
    }

    {
        OptimizerConfig cfg;
        cfg.rows = 7;
        cfg.columns = 7;
        cfg.populationSize = 12;
        cfg.generations = 10;
        cfg.mutationRate = 0.3;
        cfg.tournamentSize = 3;
        cfg.seed = 80000U;
        cfg.coinCount = 8;
        cfg.trapCount = 5;
        cfg.useMixedAlgorithms = true;
        cfg.useSmartPlacement = true;
        cfg.useEnhancedFitness = true;
        cfg.topoWeight = 0.3;

        MazeOptimizer optimizer;
        optimizer.setConfig(cfg);
        MazeModel best = optimizer.run();

        if (!best.validatePerfect()) {
            output << "FAIL end-to-end: invalid maze\n";
            return 40;
        }

        EvaluatorConfig ec;
        ec.useSmartPlacement = false;
        ec.topoWeight = 0.3;
        EvalResult eval = MazeEvaluator::evaluate(best, ec);

        double topo = MazeEvaluator::computeTopoDifficulty(best);

        output << QString::asprintf(
            "PASS end-to-end: fitness=%.1f, dp=%d, greedy=%d, regret=%d, topo=%.2f\n",
            eval.finalFitness, eval.dpScore, eval.worstGreedyScore,
            eval.regretCombined, topo);
    }

    output << "ALL TESTS PASSED\n";
    return 0;
}

} // namespace

int main(int argc, char *argv[]) {
    bool guiSmokeTest = false;
    bool battleSmokeTest = false;
    bool battleAnimationTest = false;
    bool headlessOptimizer = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--self-test") == 0) {
            setenv("QT_QPA_PLATFORM", "offscreen", 1);
            QCoreApplication app(argc, argv);
            return runSelfTests();
        }
        if (std::strcmp(argv[i], "--run-optimizer") == 0) {
            headlessOptimizer = true;
        }
        guiSmokeTest = guiSmokeTest || std::strcmp(argv[i], "--gui-smoke-test") == 0;
        battleSmokeTest = battleSmokeTest
            || std::strcmp(argv[i], "--battle-smoke-test") == 0;
        battleAnimationTest = battleAnimationTest
            || std::strcmp(argv[i], "--battle-animation-test") == 0;
    }

    if (headlessOptimizer) {
        setenv("QT_QPA_PLATFORM", "offscreen", 1);
        QCoreApplication app(argc, argv);
        QTextStream out(stdout);
        QTextStream err(stderr);

        OptimizerConfig cfg;
        cfg.rows = 7;
        cfg.columns = 7;
        cfg.populationSize = 16;
        cfg.generations = 30;
        cfg.mutationRate = 0.15;
        cfg.tournamentSize = 3;
        cfg.coinCount = 8;
        cfg.trapCount = 5;
        cfg.useMixedAlgorithms = true;
        cfg.useSmartPlacement = true;
        cfg.useEnhancedFitness = true;
        cfg.topoWeight = 0.3;
        cfg.seed = 42;

        for (int i = 1; i < argc; ++i) {
            auto arg = [&](const char *name) -> bool {
                return std::strcmp(argv[i], name) == 0 && i + 1 < argc;
            };
            if (arg("--rows")) cfg.rows = std::atoi(argv[++i]);
            else if (arg("--cols")) cfg.columns = std::atoi(argv[++i]);
            else if (arg("--population")) cfg.populationSize = std::atoi(argv[++i]);
            else if (arg("--generations")) cfg.generations = std::atoi(argv[++i]);
            else if (arg("--mutation-rate")) cfg.mutationRate = std::atof(argv[++i]);
            else if (arg("--coins")) cfg.coinCount = std::atoi(argv[++i]);
            else if (arg("--traps")) cfg.trapCount = std::atoi(argv[++i]);
            else if (arg("--seed")) cfg.seed = static_cast<quint32>(std::atoll(argv[++i]));
            else if (arg("--enable-rl")) cfg.enableRL = (std::atoi(argv[++i]) != 0);
            else if (arg("--rl-episodes")) cfg.rlEpisodes = std::atoi(argv[++i]);
            else if (arg("--topo-weight")) cfg.topoWeight = std::atof(argv[++i]);
        }

        out << "=== Headless Optimizer ===\n";
        out << "Maze: " << cfg.rows << "x" << cfg.columns
            << " | Pop: " << cfg.populationSize
            << " | Gen: " << cfg.generations << "\n";
        out << "Coins: " << cfg.coinCount << " | Traps: " << cfg.trapCount
            << " | Mutation: " << (cfg.mutationRate * 100) << "%\n";
        out << "Smart placement: " << (cfg.useSmartPlacement ? "ON" : "OFF")
            << " | Enhanced fitness: " << (cfg.useEnhancedFitness ? "ON" : "OFF")
            << " | RL: " << (cfg.enableRL ? "ON" : "OFF") << "\n\n";

        MazeOptimizer optimizer;
        optimizer.setConfig(cfg);
        QObject::connect(&optimizer, &MazeOptimizer::generationFinished,
                         [&out, &cfg](const OptimizerStats &stats) {
                             out << QString::asprintf(
                                 "Gen %3d/%d | best=%.1f avg=%.1f | dp=%d greedy=%d regret=%d\n",
                                 stats.generation, cfg.generations,
                                 stats.bestFitness, stats.avgFitness,
                                 stats.bestDpScore, stats.bestGreedyScore,
                                 stats.bestDpScore - stats.bestGreedyScore);
                             out.flush();
                         });

        MazeModel best = optimizer.run();

        ResourcePlan dp = best.optimalResourceWalk();
        EvalResult eval = MazeEvaluator::evaluate(best, [&]() {
            EvaluatorConfig ec;
            ec.useSmartPlacement = false;
            ec.topoWeight = cfg.topoWeight;
            return ec;
        }());
        double topo = MazeEvaluator::computeTopoDifficulty(best);

        out << "\n=== Result ===\n";
        out << "DP optimal: " << dp.maxValue << "\n";
        out << "Greedy worst: " << eval.worstGreedyScore << "\n";
        out << "Regret: " << eval.regretCombined << "\n";
        out << "Topo difficulty: " << topo << "\n";
        out << "Final fitness: " << eval.finalFitness << "\n";
        out << "Valid: " << (best.validatePerfect() ? "YES" : "NO") << "\n";

        const QString outputPath = QStringLiteral("optimizer_result.json");
        QByteArray json = serializeAiPlayerInput(best, {35, 45, 60},
            {{QStringLiteral("Normal"), 5, 0},
             {QStringLiteral("Heavy"), 10, 2},
             {QStringLiteral("Ultimate"), 18, 4}},
            20, 5);
        QSaveFile file(outputPath);
        if (file.open(QIODevice::WriteOnly) && file.write(json) >= 0 && file.commit()) {
            out << "Saved: " << outputPath << "\n";
        }

        return 0;
    }

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Algorithm Maze Designer"));
    if (battleSmokeTest || battleAnimationTest) {
        const QVector<int> health{35, 45, 60};
        const QVector<BossSkill> skills{{QStringLiteral("普通攻击"), 5, 0},
                                        {QStringLiteral("重击"), 10, 2},
                                        {QStringLiteral("大招"), 18, 4}};
        const BossResult result = BossSolver::solve(health, skills);
        auto *battle = new BattleWindow(
            health, skills, result, result.minimumTurns + 2, 100);
        battle->show();
        if (battleAnimationTest) {
            QTimer::singleShot(100, battle, [battle] {
                const auto buttons = battle->findChildren<QPushButton *>();
                for (QPushButton *button : buttons) {
                    if (button->text().contains(QStringLiteral("自动播放"))) {
                        button->click();
                        break;
                    }
                }
            });
        }
        const QByteArray previewPath = qgetenv("BATTLE_PREVIEW_PATH");
        if (!previewPath.isEmpty()) {
            QTimer::singleShot(150, battle, [battle, previewPath] {
                battle->grab().save(QString::fromLocal8Bit(previewPath), "PNG");
            });
        }
        QTimer::singleShot(battleAnimationTest ? 1800 : 250,
                           &app, &QCoreApplication::quit);
        return app.exec();
    }
    MainWindow window;
    window.show();
    if (guiSmokeTest) {
        QTimer::singleShot(200, &app, &QCoreApplication::quit);
    }
    return app.exec();
}
