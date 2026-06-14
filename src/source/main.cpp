#include "bosssolver.h"
#include "battlewindow.h"
#include "mainwindow.h"
#include "maze.h"

#include <QApplication>
#include <QCoreApplication>
#include <QQueue>
#include <QPushButton>
#include <QSet>
#include <QTextStream>
#include <QTimer>

#include <cstring>

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
    output << "PASS boss solver: turns=" << bossResult.minimumTurns
           << ", expanded=" << bossResult.expandedStates
           << ", pruned=" << bossResult.prunedStates << '\n';
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
