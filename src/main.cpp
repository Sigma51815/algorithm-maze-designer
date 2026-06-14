#include "core/bosssolver.h"
#include "core/maze.h"
#include "gui/mainwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QSet>
#include <QTextStream>
#include <QTimer>

#include <cstring>

namespace {

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
               << ", resource=" << plan.maxValue << '\n';
    }

    {
        MazeModel maze;
        maze.generate(15, 15, MazeAlgorithm::DepthFirstSearch, 42);
        maze.placeResources(10, 5, 43);

        if (!maze.hasBoss()) {
            output << "FAIL hasBoss should be true after generate\n";
            return 6;
        }
        if (maze.bossCell() == maze.endCell()) {
            output << "FAIL bossCell should differ from endCell\n";
            return 6;
        }
        if (maze.bossCell() == maze.startCell()) {
            output << "FAIL bossCell should differ from startCell\n";
            return 6;
        }

        const QStringList grid = maze.compactGrid();
        if (grid.size() != 31) {
            output << "FAIL compactGrid row count: " << grid.size() << '\n';
            return 6;
        }
        bool foundB = false, foundE = false;
        for (const QString &row : grid) {
            if (row.size() != 31) {
                output << "FAIL compactGrid col count: " << row.size() << '\n';
                return 6;
            }
            if (row.contains(QLatin1Char('B'))) foundB = true;
            if (row.contains(QLatin1Char('E'))) foundE = true;
        }
        if (!foundB || !foundE) {
            output << "FAIL compactGrid missing B or E marker (B=" << foundB << " E=" << foundE << ")\n";
            return 6;
        }

        const int coinCell = [&]() {
            for (int c = 0; c < maze.cellCount(); ++c)
                if (maze.resourceAt(c) > 0) return c;
            return -1;
        }();
        if (coinCell >= 0) {
            maze.consumeResource(coinCell);
            if (maze.resourceAt(coinCell) != 0) {
                output << "FAIL consumeResource did not clear cell " << coinCell << '\n';
                return 6;
            }
        }

        const QVector<BossSkill> testSkills{{QStringLiteral("Normal"), 5, 0},
                                             {QStringLiteral("Heavy"), 10, 2}};
        const QJsonObject crossJson = maze.toCrossTestJson({35, 45}, testSkills, 20, 100);
        if (!crossJson.contains(QStringLiteral("maze"))
            || !crossJson.contains(QStringLiteral("B"))
            || !crossJson.contains(QStringLiteral("PlayerSkills"))
            || !crossJson.contains(QStringLiteral("minRouds"))
            || !crossJson.contains(QStringLiteral("CoinConsumption"))) {
            output << "FAIL cross-test JSON missing fields\n";
            return 7;
        }
        output << "PASS cross-test JSON format (B=" << maze.bossCell() << " E=" << maze.endCell() << ")\n";
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
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--self-test") == 0) {
            QCoreApplication app(argc, argv);
            return runSelfTests();
        }
        guiSmokeTest = guiSmokeTest || std::strcmp(argv[i], "--gui-smoke-test") == 0;
    }

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Algorithm Maze Designer"));
    MainWindow window;
    window.show();
    if (guiSmokeTest) {
        QTimer::singleShot(200, &app, &QCoreApplication::quit);
    }
    return app.exec();
}
