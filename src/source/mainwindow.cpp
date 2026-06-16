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
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>
#include <memory>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    buildUi();
    generateMaze();
}

void MainWindow::stopAiWorker() {
    if (!aiWorkerThread_) return;
    disconnect(aiWorkerThread_, &QThread::finished, nullptr, nullptr);
    aiWorkerThread_->quit();
    aiWorkerThread_->wait();
    delete aiWorker_;
    aiWorker_ = nullptr;
    delete aiWorkerThread_;
    aiWorkerThread_ = nullptr;
}

void MainWindow::stopOptimizer() {
    if (!optimizerThread_) return;
    disconnect(optimizerThread_, nullptr, nullptr, nullptr);
    optimizerThread_->quit();
    optimizerThread_->wait();
    delete optimizer_;
    optimizer_ = nullptr;
    delete optimizerThread_;
    optimizerThread_ = nullptr;
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
    auto *generationLayout = new QVBoxLayout(generationGroup);
    generationLayout->setSpacing(8);
    auto *generationForm = new QFormLayout;
    generationForm->setSpacing(6);
    generationForm->setLabelAlignment(Qt::AlignRight);
    algorithmBox_ = new QComboBox;
    algorithmBox_->setObjectName(QStringLiteral("inputControl"));
    algorithmBox_->addItems({QStringLiteral("分治"),
                             QStringLiteral("贪心 / Kruskal MST"),
                             QStringLiteral("回溯 / DFS"),
                             QStringLiteral("分支限界 / BFS")});
    rowsSpin_ = new QSpinBox;
    rowsSpin_->setObjectName(QStringLiteral("inputControl"));
    rowsSpin_->setRange(5, 31);
    rowsSpin_->setSingleStep(2);
    rowsSpin_->setValue(15);
    columnsSpin_ = new QSpinBox;
    columnsSpin_->setObjectName(QStringLiteral("inputControl"));
    columnsSpin_->setRange(5, 31);
    columnsSpin_->setSingleStep(2);
    columnsSpin_->setValue(15);
    seedSpin_ = new QSpinBox;
    seedSpin_->setObjectName(QStringLiteral("inputControl"));
    seedSpin_->setRange(1, 999999999);
    seedSpin_->setValue(202506);
    animationSpin_ = new QSpinBox;
    animationSpin_->setObjectName(QStringLiteral("inputControl"));
    animationSpin_->setRange(1, 200);
    animationSpin_->setValue(12);
    animationSpin_->setSuffix(QStringLiteral(" ms"));
    generationForm->addRow(QStringLiteral("算法"), algorithmBox_);
    generationForm->addRow(QStringLiteral("行数"), rowsSpin_);
    generationForm->addRow(QStringLiteral("列数"), columnsSpin_);
    generationForm->addRow(QStringLiteral("种子"), seedSpin_);
    generationForm->addRow(QStringLiteral("速度"), animationSpin_);
    generationLayout->addLayout(generationForm);

    auto *generateButton = new QPushButton(QStringLiteral("生成并播放过程"));
    generateButton->setObjectName(QStringLiteral("primaryButton"));
    generationLayout->addWidget(generateButton);
    validationLabel_ = new QLabel;
    validationLabel_->setObjectName(QStringLiteral("resultLabel"));
    validationLabel_->setWordWrap(true);
    generationLayout->addWidget(validationLabel_);

    auto *complexityLabel = new QLabel;
    complexityLabel->setObjectName(QStringLiteral("infoCard"));
    complexityLabel->setWordWrap(true);
    generationLayout->addWidget(complexityLabel);
    panelLayout->addWidget(generationGroup);

    const QStringList complexityDescriptions{
        QStringLiteral("分治  O(V) 时间 / O(log V) 空间\n递归拆分矩形区域，每次只连接两个子迷宫一次。结构分区明显。"),
        QStringLiteral("Kruskal MST  O(E log E) 时间 / O(V+E) 空间\n随机边排序并用并查集防环。分支较均衡。"),
        QStringLiteral("回溯 DFS  O(V+E) 时间 / O(V) 空间\n深入未访问邻格，遇到死路回退。长走廊较多。"),
        QStringLiteral("BFS 分支限界  O(E log E) 时间 / O(E) 空间\n候选迷宫按深度层扩展，多次随机重启选最优。")};
    auto updateComplexity = [=](int index) {
        complexityLabel->setText(complexityDescriptions.value(index));
    };
    connect(algorithmBox_, &QComboBox::currentIndexChanged, this, updateComplexity);
    updateComplexity(algorithmBox_->currentIndex());
    connect(generateButton, &QPushButton::clicked, this, &MainWindow::generateMaze);

    // ── sub-separator: 遗传算法优化（合并进①） ──
    auto *optSep = new QFrame;
    optSep->setFrameShape(QFrame::HLine);
    optSep->setObjectName(QStringLiteral("separator"));
    generationLayout->addWidget(optSep);

    optEnableCheck_ = new QCheckBox(QStringLiteral("启用遗传算法优化"));
    optEnableCheck_->setObjectName(QStringLiteral("inputControl"));
    optEnableCheck_->setToolTip(QStringLiteral(
        "默认关闭：GA 优化与 RL 训练会消耗大量 CPU。本地机型请谨慎开启。"));
    optEnableCheck_->setChecked(false);
    generationLayout->addWidget(optEnableCheck_);

    auto *optForm = new QFormLayout;
    optForm->setSpacing(6);
    optForm->setLabelAlignment(Qt::AlignRight);
    optPopSpin_ = new QSpinBox;
    optPopSpin_->setObjectName(QStringLiteral("inputControl"));
    optPopSpin_->setRange(4, 100);
    optPopSpin_->setValue(16);
    optGenSpin_ = new QSpinBox;
    optGenSpin_->setObjectName(QStringLiteral("inputControl"));
    optGenSpin_->setRange(5, 500);
    optGenSpin_->setValue(30);
    optMutSpin_ = new QSpinBox;
    optMutSpin_->setObjectName(QStringLiteral("inputControl"));
    optMutSpin_->setRange(0, 100);
    optMutSpin_->setValue(15);
    optMutSpin_->setSuffix(QStringLiteral(" %"));
    optAlgoLabel_ = new QLabel(QStringLiteral("四种算法均匀混合"));
    optAlgoLabel_->setObjectName(QStringLiteral("infoCard"));
    optAlgoLabel_->setToolTip(QStringLiteral(
        "GA 初始种群按分治 / Kruskal / DFS / BFS 四种基础算法均匀轮转生成，"
        "在四种算法的基础上进行进化优化。"));
    optForm->addRow(QStringLiteral("种群"), optPopSpin_);
    optForm->addRow(QStringLiteral("代数"), optGenSpin_);
    optForm->addRow(QStringLiteral("变异率"), optMutSpin_);
    optForm->addRow(QStringLiteral("初始种群"), optAlgoLabel_);

    optSmartPlaceCheck_ = new QCheckBox(QStringLiteral("智能资源分布"));
    optSmartPlaceCheck_->setObjectName(QStringLiteral("inputControl"));
    optSmartPlaceCheck_->setChecked(true);
    optSmartPlaceCheck_->setToolTip(QStringLiteral(
        "基于迷宫拓扑分析放置资源：金币放死胡同深处诱导AI，陷阱放分叉口迫使决策"));
    optForm->addRow(QStringLiteral("资源策略"), optSmartPlaceCheck_);

    optRLCheck_ = new QCheckBox(QStringLiteral("启用 Q-Learning 精调"));
    optRLCheck_->setObjectName(QStringLiteral("inputControl"));
    optRLCheck_->setToolTip(QStringLiteral(
        "默认关闭：每代 GA 后用 RL 微调 top-k 个体，CPU 负载显著增加。"));
    optForm->addRow(QStringLiteral("RL"), optRLCheck_);
    optRLEpisodesSpin_ = new QSpinBox;
    optRLEpisodesSpin_->setObjectName(QStringLiteral("inputControl"));
    optRLEpisodesSpin_->setRange(10, 500);
    optRLEpisodesSpin_->setValue(50);
    optRLEpisodesSpin_->setEnabled(false);
    optRLTopKSpin_ = new QSpinBox;
    optRLTopKSpin_->setObjectName(QStringLiteral("inputControl"));
    optRLTopKSpin_->setRange(1, 20);
    optRLTopKSpin_->setValue(3);
    optRLTopKSpin_->setEnabled(false);
    optForm->addRow(QStringLiteral("RL 回合"), optRLEpisodesSpin_);
    optForm->addRow(QStringLiteral("RL Top-K"), optRLTopKSpin_);

    connect(optRLCheck_, &QCheckBox::toggled, this, [this](bool checked) {
        if (!optEnableCheck_->isChecked()) {
            optRLEpisodesSpin_->setEnabled(false);
            optRLTopKSpin_->setEnabled(false);
            return;
        }
        optRLEpisodesSpin_->setEnabled(checked);
        optRLTopKSpin_->setEnabled(checked);
    });

    // GA master switch: lock/unlock the optimization sub-panel.
    auto updateOptPanelEnabled = [this](bool enabled) {
        if (optPopSpin_) optPopSpin_->setEnabled(enabled);
        if (optGenSpin_) optGenSpin_->setEnabled(enabled);
        if (optMutSpin_) optMutSpin_->setEnabled(enabled);
        if (optRLCheck_) optRLCheck_->setEnabled(enabled);
        if (optRunButton_) optRunButton_->setEnabled(enabled);
        if (!enabled) {
            if (optRLEpisodesSpin_) optRLEpisodesSpin_->setEnabled(false);
            if (optRLTopKSpin_) optRLTopKSpin_->setEnabled(false);
        } else if (optRLCheck_ && optRLCheck_->isChecked()) {
            if (optRLEpisodesSpin_) optRLEpisodesSpin_->setEnabled(true);
            if (optRLTopKSpin_) optRLTopKSpin_->setEnabled(true);
        }
    };
    connect(optEnableCheck_, &QCheckBox::toggled, this, updateOptPanelEnabled);

    generationLayout->addLayout(optForm);
    auto *optButtons = new QHBoxLayout;
    optButtons->setSpacing(8);
    optRunButton_ = new QPushButton(QStringLiteral("运行优化"));
    optRunButton_->setObjectName(QStringLiteral("primaryButton"));
    optStopButton_ = new QPushButton(QStringLiteral("停止"));
    optStopButton_->setObjectName(QStringLiteral("secondaryButton"));
    optStopButton_->setEnabled(false);
    optButtons->addWidget(optRunButton_);
    optButtons->addWidget(optStopButton_);
    generationLayout->addLayout(optButtons);
    optProgressLabel_ = new QLabel(QStringLiteral("PAIRED 适应度 = DP最优 − Greedy玩家得分"));
    optProgressLabel_->setObjectName(QStringLiteral("infoCard"));
    optProgressLabel_->setWordWrap(true);
    generationLayout->addWidget(optProgressLabel_);
    optResultLabel_ = new QLabel;
    optResultLabel_->setObjectName(QStringLiteral("resultLabel"));
    optResultLabel_->setWordWrap(true);
    optResultLabel_->setVisible(false);
    generationLayout->addWidget(optResultLabel_);
    optTopoLabel_ = new QLabel;
    optTopoLabel_->setObjectName(QStringLiteral("infoCard"));
    optTopoLabel_->setWordWrap(true);
    optTopoLabel_->setVisible(false);
    generationLayout->addWidget(optTopoLabel_);
    auto *optButtonRow = new QHBoxLayout;
    optButtonRow->setSpacing(8);
    auto *applyOptButton = new QPushButton(QStringLiteral("应用优化迷宫"));
    applyOptButton->setObjectName(QStringLiteral("accentButton"));
    applyOptButton->setEnabled(false);
    optApplyButton_ = applyOptButton;
    auto *saveOptButton = new QPushButton(QStringLiteral("保存最佳迷宫"));
    saveOptButton->setObjectName(QStringLiteral("secondaryButton"));
    saveOptButton->setEnabled(false);
    optSaveButton_ = saveOptButton;
    optButtonRow->addWidget(applyOptButton);
    optButtonRow->addWidget(saveOptButton);
    generationLayout->addLayout(optButtonRow);
    connect(optRunButton_, &QPushButton::clicked, this, &MainWindow::runOptimizer);
    connect(optApplyButton_, &QPushButton::clicked, this, &MainWindow::applyOptimizedMaze);
    connect(optSaveButton_, &QPushButton::clicked, this, &MainWindow::saveOptimizedMaze);
    // Lock the optimization sub-panel initially (GA off by default).
    if (!optEnableCheck_->isChecked()) {
        optPopSpin_->setEnabled(false);
        optGenSpin_->setEnabled(false);
        optMutSpin_->setEnabled(false);
        optRLCheck_->setEnabled(false);
        optRunButton_->setEnabled(false);
    }

    panelLayout->addWidget(makeSeparator());

    auto *resourceGroup = new QGroupBox(QStringLiteral("② 动态规划资源收集"));
    resourceGroup->setObjectName(QStringLiteral("taskGroup"));
    auto *resourceLayout = new QVBoxLayout(resourceGroup);
    resourceLayout->setSpacing(8);
    auto *resourceForm = new QFormLayout;
    resourceForm->setSpacing(6);
    resourceForm->setLabelAlignment(Qt::AlignRight);
    coinSpin_ = new QSpinBox;
    coinSpin_->setObjectName(QStringLiteral("inputControl"));
    coinSpin_->setRange(0, 300);
    coinSpin_->setValue(8);
    trapSpin_ = new QSpinBox;
    trapSpin_->setObjectName(QStringLiteral("inputControl"));
    trapSpin_->setRange(0, 300);
    trapSpin_->setValue(6);
    resourceForm->addRow(QStringLiteral("金币"), coinSpin_);
    resourceForm->addRow(QStringLiteral("陷阱"), trapSpin_);
    resourceLayout->addLayout(resourceForm);
    auto *resourceButtons = new QHBoxLayout;
    resourceButtons->setSpacing(8);
    auto *placeButton = new QPushButton(QStringLiteral("重新布置"));
    placeButton->setObjectName(QStringLiteral("secondaryButton"));
    auto *solveResourceButton = new QPushButton(QStringLiteral("DP 求最优路径"));
    solveResourceButton->setObjectName(QStringLiteral("primaryButton"));
    resourceButtons->addWidget(placeButton);
    resourceButtons->addWidget(solveResourceButton);
    resourceLayout->addLayout(resourceButtons);
    resourceResultLabel_ = new QLabel(QStringLiteral("尚未求解"));
    resourceResultLabel_->setObjectName(QStringLiteral("resultLabel"));
    resourceResultLabel_->setWordWrap(true);
    resourceLayout->addWidget(resourceResultLabel_);
    panelLayout->addWidget(resourceGroup);
    connect(placeButton, &QPushButton::clicked, this, &MainWindow::placeResources);
    connect(solveResourceButton, &QPushButton::clicked, this, &MainWindow::solveResources);

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
    skillsEdit_ = new QLineEdit(
        QStringLiteral("强力攻击:8:4;普通攻击:2:0;连击:4:2;重击:6:3"));
    skillsEdit_->setObjectName(QStringLiteral("inputControl"));
    skillsEdit_->setPlaceholderText(QStringLiteral("名称:伤害:冷却;..."));
    skillsEdit_->setToolTip(QStringLiteral("如 普通攻击:5:0;重击:10:2"));
    bossForm->addRow(QStringLiteral("血量"), bossHealthEdit_);
    bossForm->addRow(QStringLiteral("技能"), skillsEdit_);
    bossLayout->addLayout(bossForm);
    auto *bossButtons = new QHBoxLayout;
    bossButtons->setSpacing(8);
    auto *solveBossButton = new QPushButton(QStringLiteral("求解最优序列"));
    solveBossButton->setObjectName(QStringLiteral("primaryButton"));
    auto *battleAnimationButton = new QPushButton(QStringLiteral("战斗动画"));
    battleAnimationButton->setObjectName(QStringLiteral("accentButton"));
    bossButtons->addWidget(solveBossButton);
    bossButtons->addWidget(battleAnimationButton);
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

void MainWindow::generateMaze() {
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
                         static_cast<quint32>(seedSpin_->value() + 1));
    lastPlan_ = {};
    lastBossResult_ = {};
    resourceResultLabel_->setText(QStringLiteral("尚未求解"));
    mazeWidget_->setMaze(maze_);
    revealedEdges_ = 0;
    mazeWidget_->setRevealCount(0);
    generationTimer_->setInterval(animationSpin_->value());
    generationTimer_->start();
    updateValidation();
}

void MainWindow::placeResources() {
    if (maze_.cellCount() == 0) {
        return;
    }
    pathTimer_->stop();
    maze_.placeResources(coinSpin_->value(), trapSpin_->value(),
                         static_cast<quint32>(seedSpin_->value() + 1));
    lastPlan_ = {};
    resourceResultLabel_->setText(QStringLiteral("资源已重新布置，等待 DP 求解"));
    mazeWidget_->setMaze(maze_);
}

void MainWindow::solveResources() {
    if (maze_.cellCount() == 0) {
        return;
    }
    generationTimer_->stop();
    mazeWidget_->setRevealCount(maze_.generationSteps().size());
    lastPlan_ = maze_.optimalResourceWalk();
    resourceResultLabel_->setText(
        QStringLiteral("<b>最多资源：%1</b>  |  行走 %2 步  |  首经 %3 格<br/>"
                       "<span style='color:#94a3b8;font-size:11px'>路径允许回访，金币/陷阱只计一次</span>")
            .arg(lastPlan_.maxValue)
            .arg(std::max(0, static_cast<int>(lastPlan_.walk.size()) - 1))
            .arg(lastPlan_.collectedCells.size()));
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

    lastBossResult_ = BossSolver::solveWithMaze(maze_, health, skills, 2);
    if (!lastBossResult_.solved
        || !BossSolver::verify(health, skills, lastBossResult_.skillSequence)) {
        bossOutput_->setPlainText(QStringLiteral("未找到可验证的技能序列。"));
        return;
    }

    QStringList sequenceNames;
    for (int skillIndex : lastBossResult_.skillSequence) {
        sequenceNames.append(skills[skillIndex].name);
    }
    bossOutput_->setPlainText(
        QStringLiteral(
            "── 求解结果 ──────────────────────\n"
            "最少回合数    %1\n"
            "限定回合数    %2\n"
            "复活金币      %3\n"
            "DP最优金币    %4\n"
            "────────────────────────────────\n"
            "最优序列      %5\n"
            "────────────────────────────────\n"
            "搜索展开 %6    剪枝 %7")
            .arg(lastBossResult_.minimumTurns)
            .arg(lastBossResult_.roundLimit)
            .arg(lastBossResult_.coinConsumption)
            .arg(lastBossResult_.maxCoinsFromDP)
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

    BossFullResult fullResult = BossSolver::solveWithMaze(maze_, health, skills, 2);
    if (!fullResult.solved
        || !BossSolver::verify(health, skills, fullResult.skillSequence)) {
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
        fullResult.coinConsumption, this);
    battleWindow->show();
    battleWindow->raise();
    battleWindow->activateWindow();
}

void MainWindow::runAiPlayer() {
    if (maze_.cellCount() == 0 || aiWorkerThread_) {
        return;
    }
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
        QMetaObject::invokeMethod(worker, [worker]() { worker->thread()->quit(); });
    });

    connect(thread, &QThread::finished, this, [this, thread, worker, resultCopy]() {
        disconnect(thread, &QThread::finished, nullptr, nullptr);
        thread->quit();
        thread->wait();
        delete worker;
        delete thread;
        aiWorker_ = nullptr;
        aiWorkerThread_ = nullptr;
        lastAiResult_ = *resultCopy;

        if (!lastAiResult_.reachedEnd) {
            aiStatusLabel_->setText(QStringLiteral("❌ AI 未能到达终点"));
            statusBar()->showMessage(QStringLiteral("AI 玩家未能到达终点"), 5000);
            return;
        }

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
    lastBossResult_ = BossSolver::solveWithMaze(maze_, health, skills, 2);
    if (!lastBossResult_.solved) {
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

void MainWindow::runOptimizer() {
    if (!optEnableCheck_->isChecked() || maze_.cellCount() == 0 || optimizerThread_) {
        return;
    }

    OptimizerConfig cfg;
    cfg.rows = (rowsSpin_->value() - 1) / 2;
    cfg.columns = (columnsSpin_->value() - 1) / 2;
    cfg.populationSize = optPopSpin_->value();
    cfg.generations = optGenSpin_->value();
    cfg.mutationRate = optMutSpin_->value() / 100.0;
    cfg.coinCount = coinSpin_->value();
    cfg.trapCount = trapSpin_->value();
    cfg.seed = static_cast<quint32>(seedSpin_->value());
    cfg.enableRL = optRLCheck_->isChecked();
    cfg.rlEpisodes = optRLEpisodesSpin_->value();
    cfg.rlTopK = optRLTopKSpin_->value();
    cfg.useMixedAlgorithms = true;
    cfg.useSmartPlacement = optSmartPlaceCheck_->isChecked();
    cfg.useEnhancedFitness = true;
    cfg.topoWeight = 0.3;

    optRunButton_->setEnabled(false);
    optStopButton_->setEnabled(true);
    optApplyButton_->setEnabled(false);
    optResultLabel_->setVisible(false);
    optProgressLabel_->setText(QStringLiteral("正在优化... 第 0 / %1 代").arg(cfg.generations));

    auto *optimizer = new MazeOptimizer;
    optimizer->setConfig(cfg);
    optimizer_ = optimizer;

    auto *thread = new QThread;
    thread->setStackSize(8 * 1024 * 1024);
    optimizer->moveToThread(thread);

    connect(optimizer, &MazeOptimizer::generationFinished, this,
            [this, cfg](const OptimizerStats &stats) {
                QString rlTag = stats.rlUsed ? QStringLiteral(" + RL精调") : QString();
                optProgressLabel_->setText(
                    QStringLiteral("第 %1 / %2 代%8  |  最优适应度 %3  |  平均 %4<br/>"
                                   "DP最优 %5  |  Greedy %6  |  Regret %7")
                        .arg(stats.generation)
                        .arg(cfg.generations)
                        .arg(stats.bestFitness, 0, 'f', 1)
                        .arg(stats.avgFitness, 0, 'f', 1)
                        .arg(stats.bestDpScore)
                        .arg(stats.bestGreedyScore)
                        .arg(stats.bestDpScore - stats.bestGreedyScore)
                        .arg(rlTag));
            });

    connect(optStopButton_, &QPushButton::clicked, optimizer, &MazeOptimizer::stop,
            Qt::DirectConnection);

    connect(optimizer, &MazeOptimizer::finished, this,
            [this, thread, optimizer, cfg](const MazeModel &bestMaze) {
                optimizedMaze_ = bestMaze;
                hasOptimizedMaze_ = true;
                lastOptConfig_ = cfg;
                optRunButton_->setEnabled(true);
                optStopButton_->setEnabled(false);
                optApplyButton_->setEnabled(true);
                optSaveButton_->setEnabled(true);

                ResourcePlan dp = optimizedMaze_.optimalResourceWalk();
                int worstGreedy = std::numeric_limits<int>::max();
                const QVector<GreedyStrategy> strategies = {
                    GreedyStrategy::ValuePerStep,
                    GreedyStrategy::NearestFirst,
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

                optResultLabel_->setText(
                    QStringLiteral("<b>优化完成</b>  |  Regret = %1<br/>"
                                   "DP最优 %2  |  Greedy %3  |  差距 %4")
                        .arg(lastOptFitness_)
                        .arg(lastOptDpScore_)
                        .arg(lastOptGreedyScore_)
                        .arg(lastOptFitness_));
                optResultLabel_->setVisible(true);

                MazeStatistics stats = optimizedMaze_.statistics();
                double topoDiff = MazeEvaluator::computeTopoDifficulty(optimizedMaze_);
                optTopoLabel_->setText(
                    QStringLiteral("拓扑难度 %1  |  死胡同 %2  |  最长走廊 %3  |  分叉口 %4")
                        .arg(topoDiff, 0, 'f', 2)
                        .arg(stats.deadEnds)
                        .arg(stats.longestCorridor)
                        .arg(stats.junctions));
                optTopoLabel_->setVisible(true);

                disconnect(optimizer, &MazeOptimizer::finished, nullptr, nullptr);
                disconnect(optimizer, &MazeOptimizer::generationFinished, nullptr, nullptr);
                thread->quit();
                thread->wait();
                delete optimizer;
                delete thread;
                optimizer_ = nullptr;
                optimizerThread_ = nullptr;
            });

    connect(thread, &QThread::started, optimizer, [optimizer]() {
        optimizer->run();
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
