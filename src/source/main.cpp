#include "aiplayerformat.h"
#include "bosssolver.h"
#include "battlewindow.h"
#include "mainwindow.h"
#include "maze.h"

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
            || wallRun > 17) {
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
    output << "ALL TESTS PASSED\n";
    return 0;
}

} // namespace

int main(int argc, char *argv[]) {
    bool guiSmokeTest = false;
    bool battleSmokeTest = false;
    bool battleAnimationTest = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--self-test") == 0) {
            QCoreApplication app(argc, argv);
            return runSelfTests();
        }
        guiSmokeTest = guiSmokeTest || std::strcmp(argv[i], "--gui-smoke-test") == 0;
        battleSmokeTest = battleSmokeTest
            || std::strcmp(argv[i], "--battle-smoke-test") == 0;
        battleAnimationTest = battleAnimationTest
            || std::strcmp(argv[i], "--battle-animation-test") == 0;
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
