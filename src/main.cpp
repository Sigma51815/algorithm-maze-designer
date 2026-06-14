#include "bosssolver.h"
#include "mainwindow.h"
#include "maze.h"

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
