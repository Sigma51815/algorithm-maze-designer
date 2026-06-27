// 文件职责：主窗口交互逻辑。
// 负责算法下拉框、生成按钮、资源求解、BOSS 演示、AI 运行、导出和 GA 优化入口。
#include "mainwindow.h"

#include "ai/aiplayerformat.h"
#include "battlewindow.h"
#include "maze_optimizer.h"
#include "maze_saver.h"
#include "maze_evaluator.h"
#include "mazewidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollArea>
#include <QSet>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>
#include <memory>

namespace {

QString formatMatrixPath(const MazeModel &maze, const QVector<int> &walk) {
    QStringList points;
    points.reserve(walk.size());
    for (int cell : walk) {
        const int matrixRow = (cell / maze.columns()) * 2 + 1;
        const int matrixColumn = (cell % maze.columns()) * 2 + 1;
        points.append(QStringLiteral("(%1,%2)").arg(matrixRow).arg(matrixColumn));
    }
    return points.isEmpty() ? QStringLiteral("无") : points.join(QStringLiteral(" → "));
}

QString formatCellCoord(const MazeModel &maze, int cell) {
    if (cell < 0 || maze.columns() <= 0) return QStringLiteral("(-,-)");
    const int matrixRow = (cell / maze.columns()) * 2 + 1;
    const int matrixColumn = (cell % maze.columns()) * 2 + 1;
    return QStringLiteral("(%1,%2)").arg(matrixRow).arg(matrixColumn);
}

QString formatCellList(const MazeModel &maze, const QVector<int> &cells, int limit = 18) {
    QStringList parts;
    const int n = std::min(limit, static_cast<int>(cells.size()));
    for (int i = 0; i < n; ++i) {
        parts.append(formatCellCoord(maze, cells[i]));
    }
    if (cells.size() > limit) {
        parts.append(QStringLiteral("...共%1格").arg(cells.size()));
    }
    return parts.isEmpty() ? QStringLiteral("无") : parts.join(QStringLiteral(" -> "));
}

bool writeJsonObject(const QString &path, const QJsonObject &object, QString *error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    if (file.write(QJsonDocument(object).toJson(QJsonDocument::Indented)) < 0) {
        if (error) *error = file.errorString();
        return false;
    }
    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

QString buildDpProcessReport(const MazeModel &maze, const ResourcePlan &plan) {
    QStringList selected;
    QStringList rejected;
    int selectedGain = 0;
    int rejectedCount = 0;
    for (const ResourceBranchDecision &decision : plan.branchDecisions) {
        const QString line = QStringLiteral("%1 接入 %2，收益 %3，覆盖 %4 格")
            .arg(formatCellCoord(maze, decision.attachCell))
            .arg(formatCellCoord(maze, decision.rootCell))
            .arg(decision.gain)
            .arg(decision.cells.size());
        if (decision.selected) {
            selected.append(line);
            selectedGain += decision.gain;
        } else {
            ++rejectedCount;
            if (rejected.size() < 8) {
                rejected.append(line);
            }
        }
    }

    QStringList cumulative;
    const int stride = std::max(1, static_cast<int>(plan.walk.size()) / 12);
    for (int i = 0; i < plan.walk.size() && i < plan.cumulativeValues.size(); i += stride) {
        cumulative.append(QStringLiteral("步%1 %2 累计=%3")
            .arg(i)
            .arg(formatCellCoord(maze, plan.walk[i]))
            .arg(plan.cumulativeValues[i]));
    }
    if (!plan.walk.isEmpty() && !plan.cumulativeValues.isEmpty()
        && (cumulative.isEmpty() || !cumulative.last().startsWith(
            QStringLiteral("步%1 ").arg(plan.walk.size() - 1)))) {
        cumulative.append(QStringLiteral("步%1 %2 累计=%3")
            .arg(plan.walk.size() - 1)
            .arg(formatCellCoord(maze, plan.walk.last()))
            .arg(plan.cumulativeValues.last()));
    }

    QString report;
    report += QStringLiteral("DP过程说明\n");
    report += QStringLiteral("1. 在整棵迷宫树上选择全局最大资源连通区域，不强制经过 S 或 E。\n");
    report += QStringLiteral("   最优资源区域：%1\n").arg(formatCellList(maze, plan.backboneCells));
    report += QStringLiteral("2. 对每个方向的子树自底向上计算收益 gain(cell)。\n");
    report += QStringLiteral("   规则：gain = 当前资源值 + 子分支 max(0, gain)，收益 <= 0 的分支不进入最优路线。\n");
    report += QStringLiteral("3. 被选正收益分支：%1 个，分支收益合计 %2。\n")
        .arg(selected.size()).arg(selectedGain);
    for (const QString &line : selected.mid(0, 12)) {
        report += QStringLiteral("   + %1\n").arg(line);
    }
    if (selected.size() > 12) {
        report += QStringLiteral("   ... 另有 %1 个正收益分支\n").arg(selected.size() - 12);
    }
    report += QStringLiteral("4. 放弃分支：%1 个（非正收益，避免踩陷阱或低收益区域）。\n").arg(rejectedCount);
    for (const QString &line : rejected) {
        report += QStringLiteral("   - %1\n").arg(line);
    }
    report += QStringLiteral("5. 路径累计资源：\n");
    for (const QString &line : cumulative) {
        report += QStringLiteral("   %1\n").arg(line);
    }
    report += QStringLiteral("最终最大资源 = %1").arg(plan.maxValue);
    return report;
}

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    buildUi();
    generateMaze();
}

void MainWindow::stopAiWorker() {
    if (!aiWorkerThread_) return;
    // Disconnect BEFORE quit/wait so the finished lambda won't fire on a deleted thread.
    disconnect(aiWorkerThread_, nullptr, nullptr, nullptr);
    // Schedule deletion on the worker thread while its event loop is still running,
    // so the DeferredDelete event is processed before quit stops the loop.
    if (aiWorker_) { aiWorker_->deleteLater(); aiWorker_ = nullptr; }
    aiWorkerThread_->quit();
    aiWorkerThread_->wait();
    aiWorkerThread_->deleteLater();
    aiWorkerThread_ = nullptr;
    // Re-enable the generate button in case it was disabled during a running AI.
    if (generateButton_) generateButton_->setEnabled(true);
}

void MainWindow::stopOptimizer() {
    if (!optimizerThread_) return;
    // Signal the GA loop to stop early so quit()+wait() returns quickly.
    if (optimizer_) optimizer_->stop();
    // Disconnect ALL signals (both thread-sender and optimizer-sender) so
    // the finished/generationFinished lambdas won't fire during/after cleanup.
    if (optimizer_) disconnect(optimizer_, nullptr, nullptr, nullptr);
    disconnect(optimizerThread_, nullptr, nullptr, nullptr);
    // Schedule deletion on the worker thread while its event loop is still running,
    // so the DeferredDelete event is processed before quit stops the loop.
    if (optimizer_) { optimizer_->deleteLater(); optimizer_ = nullptr; }
    optimizerThread_->quit();
    optimizerThread_->wait();
    optimizerThread_->deleteLater();
    optimizerThread_ = nullptr;
    // Re-enable the generate button in case it was disabled during a running GA.
    if (generateButton_) generateButton_->setEnabled(true);
}

MainWindow::~MainWindow() {
    stopAiWorker();
    stopOptimizer();
}

void MainWindow::buildUi() {
    setWindowTitle(QStringLiteral("算法驱动的迷宫设计"));
    resize(1280, 820);

    auto *splitter = new QSplitter(this);
    splitter->setHandleWidth(2);
    mazeWidget_ = new MazeWidget(splitter);

    auto *scrollArea = new QScrollArea(splitter);
    scrollArea->setWidgetResizable(true);
    scrollArea->setMinimumWidth(380);
    scrollArea->setMaximumWidth(460);
    scrollArea->setFrameShape(QFrame::NoFrame);
    auto *panel = new QWidget;
    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(16, 16, 16, 16);
    panelLayout->setSpacing(10);

    auto *title = new QLabel(QStringLiteral("迷宫设计控制台"));
    title->setObjectName(QStringLiteral("panelTitle"));
    panelLayout->addWidget(title);

    auto *legend = new QLabel(QStringLiteral(
        "S 起点   B BOSS   E 终点   ● 金币(+50)   ▲ 陷阱(-30)"));
    legend->setObjectName(QStringLiteral("legendLabel"));
    panelLayout->addWidget(legend);

    auto makeSeparator = []() {
        auto *sep = new QFrame;
        sep->setFrameShape(QFrame::HLine);
        sep->setObjectName(QStringLiteral("separator"));
        return sep;
    };

    auto *generationGroup = new QGroupBox(QStringLiteral("① 多方法生成完美迷宫"));
    generationGroup->setObjectName(QStringLiteral("taskGroup"));
    auto *generationOuter = new QVBoxLayout(generationGroup);
    generationOuter->setSpacing(8);

    // ── top row: left (algorithm) | right (optimization) ──
    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(12);

    auto *leftPanel = new QVBoxLayout;
    leftPanel->setSpacing(4);
    auto *generationForm = new QFormLayout;
    generationForm->setSpacing(4);
    generationForm->setLabelAlignment(Qt::AlignRight);
    algorithmBox_ = new QComboBox;
    algorithmBox_->setObjectName(QStringLiteral("inputControl"));
    algorithmBox_->addItems({QStringLiteral("分治"),
                             QStringLiteral("贪心 / Kruskal MST"),
                             QStringLiteral("回溯 / DFS"),
                             QStringLiteral("分支限界 / BFS")});
    rowsSpin_ = new QSpinBox;
    rowsSpin_->setObjectName(QStringLiteral("inputControl"));
    rowsSpin_->setRange(5, 31); rowsSpin_->setSingleStep(2); rowsSpin_->setValue(15);
    columnsSpin_ = new QSpinBox;
    columnsSpin_->setObjectName(QStringLiteral("inputControl"));
    columnsSpin_->setRange(5, 31); columnsSpin_->setSingleStep(2); columnsSpin_->setValue(15);
    seedSpin_ = new QSpinBox;
    seedSpin_->setObjectName(QStringLiteral("inputControl"));
    seedSpin_->setRange(1, 999999999); seedSpin_->setValue(202506);
    animationSpin_ = new QSpinBox;
    animationSpin_->setObjectName(QStringLiteral("inputControl"));
    animationSpin_->setRange(1, 200); animationSpin_->setValue(12);
    animationSpin_->setSuffix(QStringLiteral(" ms"));
    generationForm->addRow(QStringLiteral("算法"), algorithmBox_);
    generationForm->addRow(QStringLiteral("行数"), rowsSpin_);
    generationForm->addRow(QStringLiteral("列数"), columnsSpin_);
    generationForm->addRow(QStringLiteral("种子"), seedSpin_);
    generationForm->addRow(QStringLiteral("速度"), animationSpin_);
    leftPanel->addLayout(generationForm);

    generateButton_ = new QPushButton(QStringLiteral("生成并可视化迷宫"));
    generateButton_->setObjectName(QStringLiteral("primaryButton"));
    topRow->addLayout(leftPanel);

    // Right panel — optimization
    auto *rightPanel = new QVBoxLayout;
    rightPanel->setSpacing(4);
    optEnableCheck_ = new QCheckBox(QStringLiteral("启用遗传算法优化"));
    optEnableCheck_->setObjectName(QStringLiteral("inputControl"));
    optEnableCheck_->setToolTip(QStringLiteral("默认关闭：GA 优化会消耗大量 CPU。"));
    optEnableCheck_->setChecked(false);
    rightPanel->addWidget(optEnableCheck_);

    auto *optForm = new QFormLayout;
    optForm->setSpacing(4); optForm->setLabelAlignment(Qt::AlignRight);
    optPopSpin_ = new QSpinBox;
    optPopSpin_->setObjectName(QStringLiteral("inputControl"));
    optPopSpin_->setRange(4, 100); optPopSpin_->setValue(16);
    optGenSpin_ = new QSpinBox;
    optGenSpin_->setObjectName(QStringLiteral("inputControl"));
    optGenSpin_->setRange(5, 500); optGenSpin_->setValue(30);
    optMutSpin_ = new QSpinBox;
    optMutSpin_->setObjectName(QStringLiteral("inputControl"));
    optMutSpin_->setRange(0, 100); optMutSpin_->setValue(15);
    optMutSpin_->setSuffix(QStringLiteral(" %"));
    optForm->addRow(QStringLiteral("种群"), optPopSpin_);
    optForm->addRow(QStringLiteral("代数"), optGenSpin_);
    optForm->addRow(QStringLiteral("变异率"), optMutSpin_);

    optAlgoLabel_ = new QLabel(QStringLiteral("初始：同基础算法"));
    optAlgoLabel_->setObjectName(QStringLiteral("infoCard"));
    optForm->addRow(optAlgoLabel_);

    optAdversarialCheck_ = new QCheckBox(QStringLiteral("对抗模式"));
    optAdversarialCheck_->setObjectName(QStringLiteral("inputControl"));
    optAdversarialCheck_->setToolTip(QStringLiteral("陷阱放分叉口，金币藏分支深处"));
    optAdversarialCheck_->setChecked(true);
    optForm->addRow(optAdversarialCheck_);
    rightPanel->addLayout(optForm);
    topRow->addLayout(rightPanel);
    generationOuter->addLayout(topRow);

    // Resource placement belongs to maze generation; DP below only solves the chosen layout.
    auto *resourceConfig = new QGridLayout;
    resourceConfig->setHorizontalSpacing(8);
    resourceConfig->setVerticalSpacing(6);
    coinSpin_ = new QSpinBox;
    coinSpin_->setObjectName(QStringLiteral("inputControl"));
    coinSpin_->setRange(0, 300);
    coinSpin_->setValue(8);
    trapSpin_ = new QSpinBox;
    trapSpin_->setObjectName(QStringLiteral("inputControl"));
    trapSpin_->setRange(0, 300);
    trapSpin_->setValue(6);
    coinValueSpin_ = new QSpinBox;
    coinValueSpin_->setObjectName(QStringLiteral("inputControl"));
    coinValueSpin_->setRange(1, 9999);
    coinValueSpin_->setValue(50);
    trapValueSpin_ = new QSpinBox;
    trapValueSpin_->setObjectName(QStringLiteral("inputControl"));
    trapValueSpin_->setRange(1, 9999);
    trapValueSpin_->setValue(30);

    resourceConfig->addWidget(new QLabel(QStringLiteral("金币数量")), 0, 0);
    resourceConfig->addWidget(coinSpin_, 0, 1);
    resourceConfig->addWidget(new QLabel(QStringLiteral("陷阱数量")), 0, 2);
    resourceConfig->addWidget(trapSpin_, 0, 3);
    resourceConfig->addWidget(new QLabel(QStringLiteral("金币价值")), 1, 0);
    resourceConfig->addWidget(coinValueSpin_, 1, 1);
    resourceConfig->addWidget(new QLabel(QStringLiteral("陷阱扣分")), 1, 2);
    resourceConfig->addWidget(trapValueSpin_, 1, 3);
    resourceConfig->setColumnStretch(1, 1);
    resourceConfig->setColumnStretch(3, 1);
    generationOuter->addLayout(resourceConfig);

    generationOuter->addWidget(generateButton_);
    auto *exportMazeCheckButton = new QPushButton(QStringLiteral("导出迷宫检查 JSON"));
    exportMazeCheckButton->setObjectName(QStringLiteral("secondaryButton"));
    generationOuter->addWidget(exportMazeCheckButton);

    auto updateLegend = [legend, this]() {
        legend->setText(QStringLiteral(
            "S 起点   B BOSS   E 终点   ● 金币(+%1)   ▲ 陷阱(-%2)")
            .arg(coinValueSpin_->value())
            .arg(trapValueSpin_->value()));
    };
    connect(coinValueSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, updateLegend);
    connect(trapValueSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, updateLegend);
    updateLegend();

    // ── bottom: results ──
    validationLabel_ = new QLabel;
    validationLabel_->setObjectName(QStringLiteral("resultLabel"));
    validationLabel_->setWordWrap(true);
    generationOuter->addWidget(validationLabel_);

    auto *complexityLabel = new QLabel;
    complexityLabel->setObjectName(QStringLiteral("infoCard"));
    complexityLabel->setWordWrap(true);
    generationOuter->addWidget(complexityLabel);

    optProgressLabel_ = new QLabel(QStringLiteral("GA 目标 = 贪心AI得分区分度 + 难度适中（D⁰·⁵⁵ × B⁰·³⁰ × C⁰·¹⁵）"));
    optProgressLabel_->setObjectName(QStringLiteral("infoCard"));
    optProgressLabel_->setWordWrap(true);
    generationOuter->addWidget(optProgressLabel_);

    optCompareLabel_ = new QLabel;
    optCompareLabel_->setObjectName(QStringLiteral("resultLabel"));
    optCompareLabel_->setWordWrap(true);
    optCompareLabel_->setVisible(false);
    generationOuter->addWidget(optCompareLabel_);

    optSaveButton_ = new QPushButton(QStringLiteral("保存最佳迷宫"));
    optSaveButton_->setObjectName(QStringLiteral("secondaryButton"));
    optSaveButton_->setEnabled(false);
    generationOuter->addWidget(optSaveButton_);
    connect(optSaveButton_, &QPushButton::clicked, this, &MainWindow::saveOptimizedMaze);

    panelLayout->addWidget(generationGroup);

    const QStringList complexityDescriptions{
        QStringLiteral("分治  O(V) 时间 / O(V) 空间\n递归划分矩形区域，每次连接两个子迷宫一次；迷宫存储、校验与输出占 O(V)，递归栈均衡时约 O(log V)，最坏 O(V)。"),
        QStringLiteral("随机 Kruskal / MST  O(E α(V)) 时间 / O(V+E) 空间\n候选墙边随机打乱后用并查集判连通，未连通则打通墙壁；网格图 E=O(V)，实际接近线性。若采用标准 Kruskal 排序边权，则时间为 O(E log E)。"),
        QStringLiteral("回溯 DFS  O(V+E) 时间 / O(V) 空间\n从起点深入访问未访问邻格，遇到死路回退；每个房间访问一次，每条候选边检查常数次，网格图中近似 O(V)。"),
        QStringLiteral("BFS / 分支限界  O(E log E) 时间 / O(V+E) 空间\n本实现用优先队列维护候选分支并做常数次随机重启；若改为普通队列 BFS，则为 O(V+E)。")};
    auto updateComplexity = [=](int index) {
        complexityLabel->setText(complexityDescriptions.value(index));
    };
    connect(algorithmBox_, &QComboBox::currentIndexChanged, this, updateComplexity);
    updateComplexity(algorithmBox_->currentIndex());
    connect(generateButton_, &QPushButton::clicked, this, [this]() {
        generateMaze();
        if (optEnableCheck_->isChecked() && maze_.cellCount() > 0) {
            preOptMaze_ = maze_;
            runOptimizer();
        }
    });
    connect(exportMazeCheckButton, &QPushButton::clicked,
            this, &MainWindow::exportMazeCheckJson);

    // GA master switch: lock/unlock the optimization sub-panel.
    auto updateOptPanelEnabled = [this](bool enabled) {
        if (optPopSpin_) optPopSpin_->setEnabled(enabled);
        if (optGenSpin_) optGenSpin_->setEnabled(enabled);
        if (optMutSpin_) optMutSpin_->setEnabled(enabled);
        // generateButton_ handles its own state via optEnableCheck_
    };
    connect(optEnableCheck_, &QCheckBox::toggled, this, updateOptPanelEnabled);

    // Lock optimization panel initially (GA off by default).
    if (!optEnableCheck_->isChecked()) {
        optPopSpin_->setEnabled(false);
        optGenSpin_->setEnabled(false);
        optMutSpin_->setEnabled(false);
    }

    panelLayout->addWidget(makeSeparator());

    auto *resourceGroup = new QGroupBox(QStringLiteral("② 动态规划资源收集"));
    resourceGroup->setObjectName(QStringLiteral("taskGroup"));
    auto *resourceLayout = new QVBoxLayout(resourceGroup);
    resourceLayout->setSpacing(8);
    auto *resourceButtons = new QHBoxLayout;
    resourceButtons->setSpacing(8);
    auto *solveResourceButton = new QPushButton(QStringLiteral("DP 求最优路径"));
    solveResourceButton->setObjectName(QStringLiteral("primaryButton"));
    auto *exportResourceButton = new QPushButton(QStringLiteral("导出DP路径 JSON"));
    exportResourceButton->setObjectName(QStringLiteral("secondaryButton"));
    resourceButtons->addWidget(solveResourceButton);
    resourceButtons->addWidget(exportResourceButton);
    resourceLayout->addLayout(resourceButtons);
    resourceResultLabel_ = new QLabel(QStringLiteral("尚未求解"));
    resourceResultLabel_->setObjectName(QStringLiteral("resultLabel"));
    resourceResultLabel_->setWordWrap(true);
    resourceLayout->addWidget(resourceResultLabel_);
    resourceProcessOutput_ = new QPlainTextEdit;
    resourceProcessOutput_->setObjectName(QStringLiteral("outputConsole"));
    resourceProcessOutput_->setReadOnly(true);
    resourceProcessOutput_->setMaximumHeight(180);
    resourceProcessOutput_->setPlaceholderText(QStringLiteral("点击「DP 求最优路径」查看主路径、分支收益和累计资源过程"));
    resourceLayout->addWidget(resourceProcessOutput_);
    panelLayout->addWidget(resourceGroup);
    connect(solveResourceButton, &QPushButton::clicked, this, &MainWindow::solveResources);
    connect(exportResourceButton, &QPushButton::clicked,
            this, &MainWindow::exportResourcePathJson);

    panelLayout->addWidget(makeSeparator());

    auto *bossGroup = new QGroupBox(QStringLiteral("③ 分支限界 BOSS 战"));
    bossGroup->setObjectName(QStringLiteral("taskGroup"));
    auto *bossLayout = new QVBoxLayout(bossGroup);
    bossLayout->setSpacing(8);
    auto *bossForm = new QFormLayout;
    bossForm->setSpacing(6);
    bossForm->setLabelAlignment(Qt::AlignRight);
    bossHealthEdit_ = new QLineEdit(QStringLiteral("11,13,9,15"));
    bossHealthEdit_->setObjectName(QStringLiteral("inputControl"));
    bossHealthEdit_->setPlaceholderText(QStringLiteral("如 35,45,60"));
    bossHealthEdit_->setToolTip(QStringLiteral("各 BOSS 血量，逗号分隔"));
    bossHealthEdit_->setMinimumWidth(320);
    skillsEdit_ = new QLineEdit(
        QStringLiteral("强力攻击:8:4;普通攻击:2:0;连击:4:2;重击:6:3"));
    skillsEdit_->setObjectName(QStringLiteral("inputControl"));
    skillsEdit_->setPlaceholderText(QStringLiteral("名称:伤害:冷却;..."));
    skillsEdit_->setToolTip(QStringLiteral("如 普通攻击:5:0;重击:10:2"));
    skillsEdit_->setMinimumWidth(320);
    damageModeBox_ = new QComboBox;
    damageModeBox_->setObjectName(QStringLiteral("inputControl"));
    damageModeBox_->addItem(QStringLiteral("伤害不溢出（测试口径）"),
                            static_cast<int>(DamageOverflowMode::NoOverflow));
    damageModeBox_->addItem(QStringLiteral("伤害溢出（PPT口径）"),
                            static_cast<int>(DamageOverflowMode::Overflow));
    damageModeBox_->setToolTip(QStringLiteral("不溢出：多余伤害不传递；溢出：多余伤害顺延到下一个 BOSS"));
    roundLimitModeBox_ = new QComboBox;
    roundLimitModeBox_->setObjectName(QStringLiteral("inputControl"));
    roundLimitModeBox_->addItem(QStringLiteral("最小回合数"), 0);
    roundLimitModeBox_->addItem(QStringLiteral("最小回合数 + 1"), 1);
    roundLimitModeBox_->setCurrentIndex(1);
    roundLimitModeBox_->setToolTip(QStringLiteral("限定回合数只能为最小回合数或最小回合数 + 1"));
    bossForm->addRow(QStringLiteral("血量"), bossHealthEdit_);
    bossForm->addRow(QStringLiteral("技能"), skillsEdit_);
    bossForm->addRow(QStringLiteral("伤害模式"), damageModeBox_);
    bossForm->addRow(QStringLiteral("限定回合"), roundLimitModeBox_);
    bossLayout->addLayout(bossForm);
    auto *bossButtons = new QHBoxLayout;
    bossButtons->setSpacing(8);
    auto *solveBossButton = new QPushButton(QStringLiteral("求解最优序列"));
    solveBossButton->setObjectName(QStringLiteral("primaryButton"));
    auto *battleAnimationButton = new QPushButton(QStringLiteral("战斗动画"));
    battleAnimationButton->setObjectName(QStringLiteral("accentButton"));
    auto *exportBossButton = new QPushButton(QStringLiteral("导出BOSS检查 JSON"));
    exportBossButton->setObjectName(QStringLiteral("secondaryButton"));
    bossButtons->addWidget(solveBossButton);
    bossButtons->addWidget(battleAnimationButton);
    bossButtons->addWidget(exportBossButton);
    bossLayout->addLayout(bossButtons);
    bossOutput_ = new QPlainTextEdit;
    bossOutput_->setObjectName(QStringLiteral("outputConsole"));
    bossOutput_->setReadOnly(true);
    bossOutput_->setMaximumHeight(140);
    bossOutput_->setPlaceholderText(QStringLiteral("点击「求解最优序列」查看结果"));
    bossLayout->addWidget(bossOutput_);
    panelLayout->addWidget(bossGroup);
    connect(solveBossButton, &QPushButton::clicked, this, &MainWindow::solveBossBattle);
    connect(battleAnimationButton, &QPushButton::clicked,
            this, &MainWindow::showBattleAnimation);
    connect(exportBossButton, &QPushButton::clicked,
            this, &MainWindow::exportBossBattleJson);

    panelLayout->addWidget(makeSeparator());

    auto *aiGroup = new QGroupBox(QStringLiteral("AI 玩家探险"));
    aiGroup->setObjectName(QStringLiteral("taskGroup"));
    auto *aiLayout = new QVBoxLayout(aiGroup);
    aiLayout->setSpacing(8);
    auto *aiButton = new QPushButton(QStringLiteral("运行贪心 AI"));
    aiButton->setObjectName(QStringLiteral("accentButton"));
    aiLayout->addWidget(aiButton);
    aiStatusLabel_ = new QLabel(QStringLiteral("⏳ AI 正在运行..."));
    aiStatusLabel_->setObjectName(QStringLiteral("infoCard"));
    aiStatusLabel_->setVisible(false);
    aiLayout->addWidget(aiStatusLabel_);
    aiResultLabel_ = new QLabel;
    aiResultLabel_->setObjectName(QStringLiteral("infoCard"));
    aiResultLabel_->setWordWrap(true);
    aiResultLabel_->setVisible(false);
    aiLayout->addWidget(aiResultLabel_);
    panelLayout->addWidget(aiGroup);
    connect(aiButton, &QPushButton::clicked, this, &MainWindow::runAiPlayer);

    panelLayout->addWidget(makeSeparator());

    auto *loadButton = new QPushButton(QStringLiteral("加载迷宫 JSON"));
    loadButton->setObjectName(QStringLiteral("secondaryButton"));
    panelLayout->addWidget(loadButton);
    connect(loadButton, &QPushButton::clicked, this, &MainWindow::loadMaze);

    auto *exportButton = new QPushButton(QStringLiteral("导出 AI 玩家 JSON"));
    exportButton->setObjectName(QStringLiteral("secondaryButton"));
    panelLayout->addWidget(exportButton);
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::exportMaze);
    panelLayout->addStretch();

    scrollArea->setWidget(panel);
    splitter->addWidget(mazeWidget_);
    splitter->addWidget(scrollArea);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    setCentralWidget(splitter);

    generationTimer_ = new QTimer(this);
    connect(generationTimer_, &QTimer::timeout, this, [this] {
        ++revealedEdges_;
        mazeWidget_->setRevealCount(revealedEdges_);
        if (revealedEdges_ >= maze_.generationSteps().size()) {
            generationTimer_->stop();
        }
    });
    aiPathTimer_ = new QTimer(this);
    aiPathTimer_->setInterval(24);
    connect(aiPathTimer_, &QTimer::timeout, this, [this] {
        ++revealedAiPoints_;
        mazeWidget_->setAiPath(lastAiResult_.walk, revealedAiPoints_);
        if (revealedAiPoints_ >= lastAiResult_.walk.size()) {
            aiPathTimer_->stop();
        }
    });

    pathTimer_ = new QTimer(this);
    pathTimer_->setInterval(24);
    connect(pathTimer_, &QTimer::timeout, this, [this] {
        ++revealedPathPoints_;
        mazeWidget_->setSolutionPath(lastPlan_.walk, revealedPathPoints_);
        if (revealedPathPoints_ >= lastPlan_.walk.size()) {
            pathTimer_->stop();
        }
    });

    setStyleSheet(QStringLiteral(R"(
        QMainWindow { background: #FFF8F0; }
        QSplitter::handle { background: #E8D5C0; }

        QScrollArea { background: #FFF8F0; border: none; }
        QScrollArea > QWidget > QWidget { background: #FFF8F0; }

        #panelTitle {
            color: #4A2C17; font-size: 18px; font-weight: 700;
            padding: 4px 0 2px 0; letter-spacing: 1px;
        }
        #legendLabel {
            color: #8B6F5E; font-size: 11px; padding: 2px 0 6px 0;
        }
        #separator {
            border: none; border-top: 1px solid #E8D5C0;
            margin: 4px 0;
        }

        QGroupBox#taskGroup {
            color: #5C3D2E; font-size: 13px; font-weight: 600;
            border: 1px solid #D4B896; border-radius: 8px;
            margin-top: 14px; padding: 14px 10px 10px 10px;
            background: #FEF5E7;
        }
        QGroupBox#taskGroup::title {
            subcontrol-origin: margin; left: 12px; padding: 0 6px;
            color: #D4760A;
        }

        #infoCard {
            background: #FFF5E6; color: #8B5E3C;
            padding: 10px; border-radius: 6px; font-size: 11px;
            border: 1px solid #E8D5C0;
        }

        QLabel { color: #5C3D2E; font-size: 12px; }
        QLabel#resultLabel {
            color: #2E7D32; font-size: 12px;
            padding: 6px 8px; background: #E8F5E9;
            border-radius: 5px; border: 1px solid #A5D6A7;
        }

        QFormLayout QLabel { color: #7A5C4A; font-size: 12px; }

        QSpinBox#inputControl, QComboBox#inputControl, QLineEdit#inputControl {
            background: #FFFFFF; color: #4A2C17;
            border: 1px solid #D4B896; border-radius: 5px;
            padding: 5px 8px; font-size: 12px; min-height: 22px;
        }
        QSpinBox#inputControl:focus, QComboBox#inputControl:focus,
        QLineEdit#inputControl:focus {
            border-color: #E07B39;
        }
        QComboBox#inputControl::drop-down {
            border: none; width: 20px;
        }
        QComboBox#inputControl::down-arrow {
            image: none; border-left: 4px solid transparent;
            border-right: 4px solid transparent; border-top: 5px solid #8B6F5E;
            margin-right: 6px;
        }
        QComboBox#inputControl QAbstractItemView {
            background: #FFFFFF; color: #4A2C17;
            selection-background-color: #FFE8CC;
            border: 1px solid #D4B896;
        }

        QPlainTextEdit#outputConsole {
            background: #FFF8F0; color: #5C3D2E;
            border: 1px solid #D4B896; border-radius: 6px;
            padding: 8px; font-family: 'Menlo', 'Consolas', monospace;
            font-size: 11px;
        }

        QPushButton#primaryButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #F0943A, stop:1 #D4760A);
            color: #FFFFFF; font-weight: 700; font-size: 12px;
            border: none; border-radius: 6px; padding: 8px 14px;
        }
        QPushButton#primaryButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #F5A856, stop:1 #E08A2A);
        }
        QPushButton#primaryButton:pressed { background: #B86408; }

        QPushButton#secondaryButton {
            background: #F0E0CC; color: #5C3D2E; font-size: 12px;
            border: 1px solid #D4B896; border-radius: 6px; padding: 8px 14px;
        }
        QPushButton#secondaryButton:hover { background: #E8D5C0; }
        QPushButton#secondaryButton:pressed { background: #D4C4A8; }

        QPushButton#accentButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #6AAF6D, stop:1 #4A9E4D);
            color: #FFFFFF; font-weight: 700; font-size: 12px;
            border: none; border-radius: 6px; padding: 8px 14px;
        }
        QPushButton#accentButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #7CC47F, stop:1 #5AB45D);
        }
        QPushButton#accentButton:pressed { background: #3A8E3D; }

        QStatusBar { background: #FFF8F0; color: #8B6F5E; font-size: 11px; }
        QStatusBar::item { border: none; }
    )"));
}

// 生成按钮主流程：读取 UI 中选择的算法、尺寸和随机种子，调用 MazeModel::generate() 生成迷宫。
void MainWindow::generateMaze() {
    stopOptimizer();   // stop any running GA before replacing the maze
    stopAiWorker();    // stop any running AI before replacing the maze
    generationTimer_->stop();
    pathTimer_->stop();
    aiPathTimer_->stop();
    mazeWidget_->clearAiPath();
    const auto algorithm = static_cast<MazeAlgorithm>(algorithmBox_->currentIndex());
    const int matrixRows = rowsSpin_->value() % 2 == 0
        ? std::min(rowsSpin_->maximum(), rowsSpin_->value() + 1)
        : rowsSpin_->value();
    const int matrixColumns = columnsSpin_->value() % 2 == 0
        ? std::min(columnsSpin_->maximum(), columnsSpin_->value() + 1)
        : columnsSpin_->value();
    rowsSpin_->setValue(matrixRows);
    columnsSpin_->setValue(matrixColumns);
    maze_.generate((matrixRows - 1) / 2, (matrixColumns - 1) / 2, algorithm,
                   static_cast<quint32>(seedSpin_->value()));
    maze_.placeResources(coinSpin_->value(), trapSpin_->value(),
                         static_cast<quint32>(seedSpin_->value() + 1),
                         coinValueSpin_->value(), -trapValueSpin_->value());
    lastPlan_ = {};
    lastBossResult_ = {};
    resourceResultLabel_->setText(QStringLiteral("尚未求解"));
    if (resourceProcessOutput_) resourceProcessOutput_->clear();
    mazeWidget_->setMaze(maze_);
    revealedEdges_ = 0;
    mazeWidget_->setRevealCount(0);
    generationTimer_->setInterval(animationSpin_->value());
    generationTimer_->start();
    updateValidation();
    // Clear stale GA state so old comparison tables / save buttons don't persist.
    hasOptimizedMaze_ = false;
    optimizedMaze_ = {};
    preOptMaze_ = {};
    optCompareLabel_->setVisible(false);
    optSaveButton_->setEnabled(false);
    ++generationId_;   // bump so any in-flight GA callback can detect staleness
}

void MainWindow::solveResources() {
    if (maze_.cellCount() == 0) {
        return;
    }
    generationTimer_->stop();
    mazeWidget_->setRevealCount(maze_.generationSteps().size());
    lastPlan_ = maze_.optimalResourceWalk();
    const QString pathText = formatMatrixPath(maze_, lastPlan_.walk);
    const int collectedCells = lastPlan_.collectedCells.size();
    QVector<int> selectedBranchCells;
    QVector<int> rejectedBranchRoots;
    QSet<int> selectedSet;
    for (const ResourceBranchDecision &decision : lastPlan_.branchDecisions) {
        if (decision.selected) {
            for (int cell : decision.cells) {
                if (!selectedSet.contains(cell)) {
                    selectedSet.insert(cell);
                    selectedBranchCells.append(cell);
                }
            }
        } else {
            rejectedBranchRoots.append(decision.rootCell);
        }
    }
    resourceResultLabel_->setText(
        QStringLiteral("<b>最多资源：%1</b>  |  行走 %2 步  |  首经 %3 格<br/>"
                       "<span style='color:#7c2d12;font-size:11px'>"
                       "最优资源路径（矩阵坐标）：%4</span><br/>"
                       "<span style='color:#94a3b8;font-size:11px'>路径允许回访，金币/陷阱只计一次</span>")
            .arg(lastPlan_.maxValue)
            .arg(std::max(0, static_cast<int>(lastPlan_.walk.size()) - 1))
            .arg(collectedCells)
            .arg(pathText));
    if (resourceProcessOutput_) {
        resourceProcessOutput_->setPlainText(buildDpProcessReport(maze_, lastPlan_));
    }
    mazeWidget_->setDpProcessHighlights(lastPlan_.backboneCells,
                                        selectedBranchCells,
                                        rejectedBranchRoots);
    revealedPathPoints_ = 1;
    mazeWidget_->setSolutionPath(lastPlan_.walk, revealedPathPoints_);
    pathTimer_->start();
}

QVector<int> MainWindow::parseBossHealth(bool *ok) const {
    QVector<int> values;
    bool valid = true;
    const QStringList parts = bossHealthEdit_->text().split(
        QRegularExpression(QStringLiteral("[,，;；\\s]+")), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        bool converted = false;
        const int value = part.toInt(&converted);
        if (!converted || value <= 0) {
            valid = false;
            break;
        }
        values.append(value);
    }
    valid = valid && !values.isEmpty();
    if (ok) {
        *ok = valid;
    }
    return valid ? values : QVector<int>{};
}

QVector<BossSkill> MainWindow::parseSkills(bool *ok) const {
    QVector<BossSkill> skills;
    bool valid = true;
    const QStringList entries = skillsEdit_->text().split(
        QRegularExpression(QStringLiteral("[;；\\n]+")), Qt::SkipEmptyParts);
    for (const QString &entry : entries) {
        const QStringList fields = entry.split(
            QRegularExpression(QStringLiteral("[:：,，]+")), Qt::SkipEmptyParts);
        if (fields.size() != 3) {
            valid = false;
            break;
        }
        bool damageOk = false;
        bool cooldownOk = false;
        const int damage = fields[1].trimmed().toInt(&damageOk);
        const int cooldown = fields[2].trimmed().toInt(&cooldownOk);
        if (!damageOk || !cooldownOk || damage <= 0 || cooldown < 0
            || fields[0].trimmed().isEmpty()) {
            valid = false;
            break;
        }
        skills.append({fields[0].trimmed(), damage, cooldown});
    }
    valid = valid && !skills.isEmpty()
        && std::any_of(skills.begin(), skills.end(), [](const BossSkill &skill) {
               return skill.cooldown == 0;
           });
    if (ok) {
        *ok = valid;
    }
    return valid ? skills : QVector<BossSkill>{};
}

DamageOverflowMode MainWindow::selectedDamageMode() const {
    if (!damageModeBox_) {
        return DamageOverflowMode::NoOverflow;
    }
    bool ok = false;
    const int value = damageModeBox_->currentData().toInt(&ok);
    return ok ? static_cast<DamageOverflowMode>(value) : DamageOverflowMode::NoOverflow;
}

int MainWindow::selectedRoundExtraTurns() const {
    if (!roundLimitModeBox_) {
        return 1;
    }
    bool ok = false;
    const int value = roundLimitModeBox_->currentData().toInt(&ok);
    return ok ? std::clamp(value, 0, 1) : 1;
}

void MainWindow::solveBossBattle() {
    bool healthOk = false;
    bool skillsOk = false;
    const QVector<int> health = parseBossHealth(&healthOk);
    const QVector<BossSkill> skills = parseSkills(&skillsOk);
    if (!healthOk || !skillsOk) {
        QMessageBox::warning(this, QStringLiteral("输入格式错误"),
                             QStringLiteral("血量需为正整数列表；技能格式为 名称:伤害:冷却，且至少有一个无冷却技能。"));
        return;
    }

    const DamageOverflowMode damageMode = selectedDamageMode();
    lastBossResult_ = BossSolver::solveWithMaze(
        maze_, health, skills, selectedRoundExtraTurns(), damageMode);
    if (!lastBossResult_.solved
        || !BossSolver::verify(health, skills, lastBossResult_.skillSequence, damageMode)) {
        bossOutput_->setPlainText(QStringLiteral("未找到可验证的技能序列。"));
        return;
    }

    QStringList sequenceNames;
    for (int skillIndex : lastBossResult_.skillSequence) {
        sequenceNames.append(skills[skillIndex].name);
    }
    const QString damageModeName = damageMode == DamageOverflowMode::Overflow
        ? QStringLiteral("伤害溢出")
        : QStringLiteral("伤害不溢出");
    bossOutput_->setPlainText(
        QStringLiteral(
            "── 求解结果 ──────────────────────\n"
            "最少回合数    %1\n"
            "限定回合数    %2\n"
            "复活金币      %3\n"
            "DP 最大金币   %4\n"
            "伤害模式      %5\n"
            "────────────────────────────────\n"
            "最优序列      %6\n"
            "────────────────────────────────\n"
            "搜索展开 %7    剪枝 %8")
            .arg(lastBossResult_.minimumTurns)
            .arg(lastBossResult_.roundLimit)
            .arg(lastBossResult_.coinConsumption)
            .arg(lastBossResult_.maxCoinsFromDP)
            .arg(damageModeName)
            .arg(sequenceNames.join(QStringLiteral(" → ")))
            .arg(lastBossResult_.expandedStates)
            .arg(lastBossResult_.prunedStates));
}

void MainWindow::showBattleAnimation() {
    bool healthOk = false;
    bool skillsOk = false;
    const QVector<int> health = parseBossHealth(&healthOk);
    const QVector<BossSkill> skills = parseSkills(&skillsOk);
    if (!healthOk || !skillsOk) {
        QMessageBox::warning(this, QStringLiteral("输入格式错误"),
                             QStringLiteral("请先填写正确的 BOSS 血量与技能参数。"));
        return;
    }

    const DamageOverflowMode damageMode = selectedDamageMode();
    BossFullResult fullResult = BossSolver::solveWithMaze(
        maze_, health, skills, selectedRoundExtraTurns(), damageMode);
    if (!fullResult.solved
        || !BossSolver::verify(health, skills, fullResult.skillSequence, damageMode)) {
        QMessageBox::warning(this, QStringLiteral("无法开始战斗"),
                             QStringLiteral("当前参数没有得到可验证的技能序列。"));
        return;
    }

    lastBossResult_ = fullResult;
    BossResult basicResult;
    basicResult.solved = fullResult.solved;
    basicResult.minimumTurns = fullResult.minimumTurns;
    basicResult.skillSequence = fullResult.skillSequence;
    basicResult.expandedStates = fullResult.expandedStates;
    basicResult.prunedStates = fullResult.prunedStates;

    auto *battleWindow = new BattleWindow(
        health, skills, basicResult,
        fullResult.roundLimit,
        fullResult.coinConsumption, damageMode, this);
    battleWindow->show();
    battleWindow->raise();
    battleWindow->activateWindow();
}

void MainWindow::loadMaze() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("加载迷宫 JSON"), QString(),
        QStringLiteral("JSON 文件 (*.json)"));
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, QStringLiteral("加载失败"),
                              QStringLiteral("无法打开文件：%1").arg(file.errorString()));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::critical(this, QStringLiteral("格式错误"),
                              QStringLiteral("JSON 解析失败：%1").arg(parseError.errorString()));
        return;
    }

    const QJsonObject root = doc.object();
    MazeModel loaded;
    bool ok = false;

    if (root.contains(QStringLiteral("maze"))) {
        QString err;
        ok = MazeModel::fromExpandedGrid(root.value(QStringLiteral("maze")).toArray(),
                                          loaded, &err);
        if (!ok) {
            QMessageBox::critical(this, QStringLiteral("格式错误"), err);
            return;
        }
    } else if (root.value(QStringLiteral("format")).toString()
                   == QStringLiteral("algorithm-maze-v1")) {
        SavedMazeInfo info;
        ok = MazeSaver::loadMazeFromJson(root, info);
        if (!ok) {
            QMessageBox::critical(this, QStringLiteral("格式错误"),
                                  QStringLiteral("无法解析原生迷宫格式"));
            return;
        }
        loaded = info.maze;
    } else if (root.value(QStringLiteral("format")).toString()
                   == QStringLiteral("ga-result-v1")) {
        const QJsonObject bestMaze = root.value(QStringLiteral("bestMaze")).toObject();
        SavedMazeInfo info;
        ok = MazeSaver::loadMazeFromJson(bestMaze, info);
        if (!ok) {
            QMessageBox::critical(this, QStringLiteral("格式错误"),
                                  QStringLiteral("无法解析 GA 结果格式"));
            return;
        }
        loaded = info.maze;
    } else {
        QMessageBox::critical(this, QStringLiteral("格式错误"),
                              QStringLiteral("未识别的 JSON 格式"));
        return;
    }

    generationTimer_->stop();
    pathTimer_->stop();
    aiPathTimer_->stop();
    stopOptimizer();
    stopAiWorker();

    maze_ = loaded;
    lastPlan_ = {};
    lastBossResult_ = {};
    lastAiResult_ = {};
    revealedEdges_ = 0;
    revealedPathPoints_ = 0;
    revealedAiPoints_ = 0;

    mazeWidget_->setMaze(maze_);
    mazeWidget_->clearAiPath();
    mazeWidget_->clearSolutionPath();
    if (aiStatusLabel_) aiStatusLabel_->setVisible(false);
    resourceResultLabel_->setText(QStringLiteral("尚未求解"));
    if (resourceProcessOutput_) resourceProcessOutput_->clear();

    rowsSpin_->setValue(maze_.rows() * 2 + 1);
    columnsSpin_->setValue(maze_.columns() * 2 + 1);
    updateValidation();
    // Clear stale GA state from any previous optimization run.
    hasOptimizedMaze_ = false;
    optimizedMaze_ = {};
    preOptMaze_ = {};
    optCompareLabel_->setVisible(false);
    optSaveButton_->setEnabled(false);

    if (root.contains(QStringLiteral("B"))) {
        const QJsonArray bosses = root.value(QStringLiteral("B")).toArray();
        QStringList healthParts;
        for (const QJsonValue &v : bosses) healthParts.append(QString::number(v.toInt()));
        bossHealthEdit_->setText(healthParts.join(QLatin1Char(',')));
    }
    if (root.contains(QStringLiteral("PlayerSkills"))) {
        const QJsonArray skills = root.value(QStringLiteral("PlayerSkills")).toArray();
        QStringList skillParts;
        for (const QJsonValue &v : skills) {
            const QJsonArray pair = v.toArray();
            if (pair.size() >= 2) {
                skillParts.append(QStringLiteral("技能%1:%2:%3")
                    .arg(skillParts.size() + 1)
                    .arg(pair[0].toInt())
                    .arg(pair[1].toInt()));
            }
        }
        if (!skillParts.isEmpty()) skillsEdit_->setText(skillParts.join(QLatin1Char(';')));
    }

    statusBar()->showMessage(QStringLiteral("已加载：%1").arg(path), 5000);
}

void MainWindow::runAiPlayer() {
    if (maze_.cellCount() == 0 || aiWorkerThread_) {
        return;
    }
    generateButton_->setEnabled(false);  // prevent re-entry while AI is running
    const int genAtStart = generationId_;  // capture for staleness detection
    generationTimer_->stop();
    aiPathTimer_->stop();
    mazeWidget_->clearAiPath();
    lastAiResult_ = {};

    aiStatusLabel_->setText(QStringLiteral("⏳ AI 正在运行..."));
    aiStatusLabel_->setVisible(true);
    if (aiResultLabel_) aiResultLabel_->setVisible(false);

    auto *thread = new QThread;
    thread->setStackSize(8 * 1024 * 1024);
    auto mazeCopy = std::make_shared<MazeModel>(maze_);
    auto resultCopy = std::make_shared<PlayResult>();

    auto *worker = new QObject;
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, [worker, mazeCopy, resultCopy]() {
        *resultCopy = GreedyPlayer::play(*mazeCopy);
        worker->deleteLater();  // schedule deletion while event loop is running
        QMetaObject::invokeMethod(worker, [worker]() { worker->thread()->quit(); });
    });

    connect(thread, &QThread::finished, this, [this, worker, thread, resultCopy, genAtStart]() {
        // Discard stale results from an AI run started before the latest
        // generateMaze() call (which calls stopAiWorker() first).
        if (genAtStart != generationId_) {
            thread->deleteLater();
            aiWorker_ = nullptr;
            aiWorkerThread_ = nullptr;
            return;
        }
        // Async cleanup — never block the main thread with wait().
        aiWorker_ = nullptr;
        aiWorkerThread_ = nullptr;
        lastAiResult_ = *resultCopy;

        if (!lastAiResult_.reachedEnd) {
            aiStatusLabel_->setText(QStringLiteral("❌ AI 未能到达终点"));
            statusBar()->showMessage(QStringLiteral("AI 玩家未能到达终点"), 5000);
        } else {
            aiStatusLabel_->setVisible(false);
            revealedAiPoints_ = 1;
            mazeWidget_->setAiPath(lastAiResult_.walk, revealedAiPoints_);
            aiPathTimer_->start();
            aiResultLabel_->setText(QStringLiteral(
                "<b>AI 探险结果</b><br/>"
                "剩余资源价值：<b>%1</b><br/>"
                "所用步数：<b>%2</b><br/>"
                "效率比值（价值/步数）：<b>%3</b><br/>"
                "<span style='font-size:11px'>金币 %4 | 陷阱 %5</span>")
                .arg(lastAiResult_.remainingResource)
                .arg(lastAiResult_.totalSteps)
                .arg(lastAiResult_.score, 0, 'f', 3)
                .arg(lastAiResult_.collectedCoins)
                .arg(lastAiResult_.triggeredTraps));
            aiResultLabel_->setVisible(true);
            statusBar()->showMessage(
                QStringLiteral("AI 玩家完成：score=%1, 步数=%2, 金币=%3, 陷阱=%4")
                    .arg(lastAiResult_.score, 0, 'f', 2)
                    .arg(lastAiResult_.totalSteps)
                    .arg(lastAiResult_.collectedCoins)
                    .arg(lastAiResult_.triggeredTraps),
                8000);
        }
        generateButton_->setEnabled(true);  // re-enable after AI completes
        thread->deleteLater();
    });

    aiWorker_ = worker;
    aiWorkerThread_ = thread;
    thread->start();
}

void MainWindow::updateValidation() {
    QString reason;
    const bool valid = maze_.validatePerfect(&reason);
    const MazeStatistics stats = maze_.statistics();
    validationLabel_->setText(
        QStringLiteral("<b>%1</b><br/>"
                       "<span style='font-size:11px'>矩阵 %2×%3  |  路径 %4 步  |  死路 %5  |  分叉 %6  |  最长走廊 %7</span>")
            .arg(reason)
            .arg(maze_.rows() * 2 + 1)
            .arg(maze_.columns() * 2 + 1)
            .arg(stats.solutionLength)
            .arg(stats.deadEnds)
            .arg(stats.junctions)
            .arg(stats.longestCorridor));
    validationLabel_->setStyleSheet(valid ? QStringLiteral("color:#2E7D32;")
                                          : QStringLiteral("color:#C62828;"));
}

void MainWindow::exportMazeCheckJson() {
    if (maze_.cellCount() == 0) {
        return;
    }

    const QJsonObject root = buildMazeCheckInput(maze_);

    const QString defaultName = QStringLiteral("maze_check_%1x%2.json")
                                    .arg(maze_.rows() * 2 + 1)
                                    .arg(maze_.columns() * 2 + 1);
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出迷宫检查 JSON"), defaultName,
        QStringLiteral("JSON 文件 (*.json)"));
    if (path.isEmpty()) {
        return;
    }

    QString error;
    if (!writeJsonObject(path, root, &error)) {
        QMessageBox::critical(this, QStringLiteral("导出失败"),
                              QStringLiteral("无法写入文件：%1").arg(error));
        return;
    }
    statusBar()->showMessage(QStringLiteral("已导出迷宫检查 JSON：%1").arg(path), 5000);
}

void MainWindow::exportResourcePathJson() {
    if (maze_.cellCount() == 0) {
        return;
    }
    if (lastPlan_.walk.isEmpty()) {
        lastPlan_ = maze_.optimalResourceWalk();
    }

    const QJsonObject root = buildResourcePathCheckInput(maze_, lastPlan_.walk);

    const QString defaultName = QStringLiteral("resource_path_%1x%2.json")
                                    .arg(maze_.rows() * 2 + 1)
                                    .arg(maze_.columns() * 2 + 1);
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出 DP 资源路径 JSON"), defaultName,
        QStringLiteral("JSON 文件 (*.json)"));
    if (path.isEmpty()) {
        return;
    }

    QString error;
    if (!writeJsonObject(path, root, &error)) {
        QMessageBox::critical(this, QStringLiteral("导出失败"),
                              QStringLiteral("无法写入文件：%1").arg(error));
        return;
    }
    statusBar()->showMessage(QStringLiteral("已导出 DP 资源路径 JSON：%1").arg(path), 5000);
}

void MainWindow::exportBossBattleJson() {
    bool healthOk = false;
    bool skillsOk = false;
    const QVector<int> health = parseBossHealth(&healthOk);
    const QVector<BossSkill> skills = parseSkills(&skillsOk);
    if (!healthOk || !skillsOk) {
        QMessageBox::warning(this, QStringLiteral("无法导出"),
                             QStringLiteral("请先填写正确的 BOSS 血量与技能参数。"));
        return;
    }

    const DamageOverflowMode damageMode = selectedDamageMode();
    lastBossResult_ = BossSolver::solveWithMaze(
        maze_, health, skills, selectedRoundExtraTurns(), damageMode);
    if (!lastBossResult_.solved
        || !BossSolver::verify(health, skills, lastBossResult_.skillSequence, damageMode)) {
        QMessageBox::warning(this, QStringLiteral("无法导出"),
                             QStringLiteral("当前参数没有可验证的最优技能序列。"));
        return;
    }

    const QJsonObject root = buildBossBattleCheckInput(
        health, skills, lastBossResult_.skillSequence);

    const QString defaultName = QStringLiteral("boss_battle_check.json");
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出 BOSS 检查 JSON"), defaultName,
        QStringLiteral("JSON 文件 (*.json)"));
    if (path.isEmpty()) {
        return;
    }

    QString error;
    if (!writeJsonObject(path, root, &error)) {
        QMessageBox::critical(this, QStringLiteral("导出失败"),
                              QStringLiteral("无法写入文件：%1").arg(error));
        return;
    }
    statusBar()->showMessage(QStringLiteral("已导出 BOSS 检查 JSON：%1").arg(path), 5000);
}

void MainWindow::exportMaze() {
    if (maze_.cellCount() == 0) {
        return;
    }
    bool healthOk = false;
    bool skillsOk = false;
    const QVector<int> health = parseBossHealth(&healthOk);
    const QVector<BossSkill> skills = parseSkills(&skillsOk);
    if (!healthOk || !skillsOk) {
        QMessageBox::warning(this, QStringLiteral("无法导出"),
                             QStringLiteral("请先填写正确的 BOSS 血量与玩家技能。"));
        return;
    }
    const DamageOverflowMode damageMode = selectedDamageMode();
    lastBossResult_ = BossSolver::solveWithMaze(
        maze_, health, skills, selectedRoundExtraTurns(), damageMode);
    if (!lastBossResult_.solved
        || !BossSolver::verify(health, skills, lastBossResult_.skillSequence, damageMode)) {
        QMessageBox::warning(this, QStringLiteral("无法导出"),
                             QStringLiteral("当前 BOSS 参数没有可行技能序列。"));
        return;
    }

    const QString defaultName = QStringLiteral("maze_%1_%2.json")
                                    .arg(maze_.rows() * 2 + 1)
                                    .arg(maze_.columns() * 2 + 1);
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出 AI 玩家输入"), defaultName,
        QStringLiteral("JSON 文件 (*.json)"));
    if (path.isEmpty()) {
        return;
    }

    const QByteArray exportedJson = serializeAiPlayerInput(
        maze_, health, skills, lastBossResult_.roundLimit, lastBossResult_.coinConsumption);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(exportedJson) < 0
        || !file.commit()) {
        QMessageBox::critical(this, QStringLiteral("导出失败"), file.errorString());
        return;
    }
    statusBar()->showMessage(QStringLiteral("已导出：%1").arg(path), 5000);
}

// GA 线程优化入口：把当前 UI 参数封装成 OptimizerConfig，在线程中进化迷宫并回传最优结果。
void MainWindow::runOptimizer() {
    if (!optEnableCheck_->isChecked() || maze_.cellCount() == 0 || optimizerThread_) {
        return;
    }

    generateButton_->setEnabled(false);  // prevent re-entry while GA is running
    const int genAtStart = generationId_;  // capture for staleness detection

    OptimizerConfig cfg;
    cfg.rows = (rowsSpin_->value() - 1) / 2;
    cfg.columns = (columnsSpin_->value() - 1) / 2;
    cfg.populationSize = optPopSpin_->value();
    cfg.generations = optGenSpin_->value();
    cfg.mutationRate = optMutSpin_->value() / 100.0;
    cfg.coinCount = coinSpin_->value();
    cfg.trapCount = trapSpin_->value();
    cfg.coinValue = coinValueSpin_->value();
    cfg.trapValue = -trapValueSpin_->value();
    cfg.seed = static_cast<quint32>(seedSpin_->value());
    cfg.useMixedAlgorithms = false;
    cfg.baseAlgorithm = static_cast<MazeAlgorithm>(algorithmBox_->currentIndex());
    cfg.useAdversarialPlacement = optAdversarialCheck_->isChecked();
    cfg.useSmartPlacement = !cfg.useAdversarialPlacement;
    cfg.useEnhancedFitness = true;
    preOptMaze_ = maze_;  // snapshot for before/after comparison
    cfg.topoWeight = 0.3;

    optProgressLabel_->setText(QStringLiteral("正在优化... 第 0 / %1 代").arg(cfg.generations));

    auto *optimizer = new MazeOptimizer;
    optimizer->setConfig(cfg);
    optimizer_ = optimizer;

    auto *thread = new QThread;
    thread->setStackSize(8 * 1024 * 1024);
    optimizer->moveToThread(thread);

    connect(optimizer, &MazeOptimizer::generationFinished, this,
            [this, cfg](const OptimizerStats &stats) {
                optProgressLabel_->setText(
                    QStringLiteral("第 %1 / %2 代  |  适应度 %3  |  平均 %4<br/>"
                                   "DP最优 %5  |  Greedy %6  |  Regret %7")
                        .arg(stats.generation)
                        .arg(cfg.generations)
                        .arg(stats.bestFitness, 0, 'f', 1)
                        .arg(stats.avgFitness, 0, 'f', 1)
                        .arg(stats.bestDpScore)
                        .arg(stats.bestGreedyScore)
                        .arg(stats.bestDpScore - stats.bestGreedyScore));
            });


    connect(optimizer, &MazeOptimizer::finished, this,
            [this, thread, optimizer, cfg, genAtStart](const MazeModel &bestMaze) {
                // Discard stale results from a GA that was running before the
                // latest generateMaze() call (which calls stopOptimizer() first).
                if (genAtStart != generationId_) {
                    thread->deleteLater();
                    optimizer_ = nullptr;
                    optimizerThread_ = nullptr;
                    return;
                }
                optimizedMaze_ = bestMaze;
                hasOptimizedMaze_ = true;
                lastOptConfig_ = cfg;
                optSaveButton_->setEnabled(true);
                generationTimer_->stop();  // stop animation on previous maze
                maze_ = optimizedMaze_;
                mazeWidget_->setMaze(maze_);
                mazeWidget_->clearAiPath();
                updateValidation();

                ResourcePlan dp = optimizedMaze_.optimalResourceWalk();
                int worstGreedy = std::numeric_limits<int>::max();
                const QVector<GreedyStrategy> strategies = {
                    GreedyStrategy::ValuePerStep,
                    GreedyStrategy::CautiousCollector,
                    GreedyStrategy::AvoidTraps,
                    GreedyStrategy::EndGoalFirst
                };
                for (GreedyStrategy s : strategies) {
                    PlayResult result = GreedyPlayer::play(optimizedMaze_, {}, {}, 0, s);
                    worstGreedy = std::min(worstGreedy, result.remainingResource);
                }
                lastOptDpScore_ = dp.maxValue;
                lastOptGreedyScore_ = worstGreedy;
                lastOptFitness_ = dp.maxValue - worstGreedy;


                // Auto-show comparison between pre-optimization and optimized maze.
                if (preOptMaze_.cellCount() > 0) {
                    EvaluatorConfig ec;
                    ec.skipPlacement = true;
                    ec.topoWeight = 0.3;
                    EvalResult before = MazeEvaluator::evaluate(preOptMaze_, ec);
                    EvalResult after = MazeEvaluator::evaluate(optimizedMaze_, ec);
                    QString report;
                    report += QStringLiteral(
                        "<b>对比评估（优化前 vs 优化后）</b><br/>"
                        "<table style='border-spacing:8px 2px'>"
                        "<tr><td></td><td><b>优化前</b></td><td><b>优化后</b></td></tr>"
                        "<tr><td>金币漏拾率</td>"
                            "<td>%1%</td><td>%2%</td></tr>"
                        "<tr><td>陷阱命中率</td>"
                            "<td>%3%</td><td>%4%</td></tr>"
                        "<tr><td>路径迂回度</td>"
                            "<td>%5%</td><td>%6%</td></tr>"
                        "<tr><td>区分度 D</td>"
                            "<td>%7</td><td>%8</td></tr>"
                        "<tr><td>稳定性 C</td>"
                            "<td>%9</td><td>%10</td></tr>"
                        "<tr><td>难度适中 B</td>"
                            "<td>%11</td><td>%12</td></tr>"
                        "<tr><td>AI得分均值比</td>"
                            "<td>%13</td><td>%15</td></tr>"
                        "<tr><td>AI得分极差</td>"
                            "<td>%14</td><td>%16</td></tr>"
                        "<tr><td><b>适应度 finalFitness</b></td>"
                            "<td>%17</td><td style='color:#2E7D32'><b>%18</b></td></tr>"
                        "</table>")
                        .arg(before.coinMissRate * 100, 0, 'f', 1)
                        .arg(after.coinMissRate * 100, 0, 'f', 1)
                        .arg(before.trapHitRate * 100, 0, 'f', 1)
                        .arg(after.trapHitRate * 100, 0, 'f', 1)
                        .arg(before.pathInefficiency * 100, 0, 'f', 1)
                        .arg(after.pathInefficiency * 100, 0, 'f', 1)
                        .arg(before.designDiscrimination, 0, 'f', 2)
                        .arg(after.designDiscrimination, 0, 'f', 2)
                        .arg(before.designStability, 0, 'f', 2)
                        .arg(after.designStability, 0, 'f', 2)
                        .arg(before.designBalance, 0, 'f', 2)
                        .arg(after.designBalance, 0, 'f', 2)
                        .arg(before.meanAIScoreRatio, 0, 'f', 2)
                        .arg(before.aiScoreSpread, 0, 'f', 2)
                        .arg(after.meanAIScoreRatio, 0, 'f', 2)
                        .arg(after.aiScoreSpread, 0, 'f', 2)
                        .arg(before.finalFitness, 0, 'f', 1)
                        .arg(after.finalFitness, 0, 'f', 1);
                    bool improved = after.finalFitness > before.finalFitness;
                    report += improved
                        ? QStringLiteral("<br/><b style='color:#2E7D32'>✓ 优化有效（适应度提升）</b>")
                        : QStringLiteral("<br/><b style='color:#C62828'>✗ 优化未提升适应度</b>");
                    optCompareLabel_->setText(report);
                    optCompareLabel_->setVisible(true);
                }

                // Async cleanup — never block the main thread.
                generateButton_->setEnabled(true);  // re-enable after GA completes
                thread->deleteLater();
                optimizer_ = nullptr;
                optimizerThread_ = nullptr;
            });

    connect(thread, &QThread::started, optimizer, [optimizer]() {
        optimizer->run();
        optimizer->deleteLater();  // schedule deletion while event loop is running
        QThread::currentThread()->quit();
    });

    optimizerThread_ = thread;
    thread->start();
}

void MainWindow::applyOptimizedMaze() {
    if (!hasOptimizedMaze_) {
        return;
    }
    generationTimer_->stop();
    pathTimer_->stop();
    aiPathTimer_->stop();
    stopAiWorker();

    maze_ = optimizedMaze_;
    lastPlan_ = {};
    lastBossResult_ = {};
    lastAiResult_ = {};
    revealedPathPoints_ = 0;
    revealedAiPoints_ = 0;

    resourceResultLabel_->setText(QStringLiteral("已应用优化迷宫，等待 DP 求解"));
    if (resourceProcessOutput_) resourceProcessOutput_->clear();
    mazeWidget_->setMaze(maze_);
    mazeWidget_->clearAiPath();
    if (aiStatusLabel_) aiStatusLabel_->setVisible(false);
    if (aiResultLabel_) aiResultLabel_->setVisible(false);

    revealedEdges_ = static_cast<int>(maze_.generationSteps().size());
    mazeWidget_->setRevealCount(revealedEdges_);
    updateValidation();
    statusBar()->showMessage(QStringLiteral("已应用遗传优化迷宫"), 5000);
}

void MainWindow::saveOptimizedMaze() {
    if (!hasOptimizedMaze_) {
        return;
    }
    const QString defaultName = QStringLiteral("ga_maze_%1x%2_f%3.json")
                                    .arg(lastOptConfig_.rows)
                                    .arg(lastOptConfig_.columns)
                                    .arg(lastOptFitness_);
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存最佳优化迷宫"), defaultName,
        QStringLiteral("JSON 文件 (*.json)"));
    if (path.isEmpty()) {
        return;
    }
    if (MazeSaver::saveGAResult(path, optimizedMaze_, lastOptConfig_,
                                lastOptFitness_, lastOptDpScore_,
                                lastOptGreedyScore_)) {
        statusBar()->showMessage(QStringLiteral("已保存最佳迷宫：%1").arg(path), 5000);
    } else {
        QMessageBox::critical(this, QStringLiteral("保存失败"),
                              QStringLiteral("无法写入文件：%1").arg(path));
    }
}
