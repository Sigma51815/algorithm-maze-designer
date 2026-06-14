#include "mainwindow.h"

#include "ai/aiplayerformat.h"
#include "battlewindow.h"
#include "maze_optimizer.h"
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
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    buildUi();
    generateMaze();
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
    extraTurnsSpin_ = new QSpinBox;
    extraTurnsSpin_->setObjectName(QStringLiteral("inputControl"));
    extraTurnsSpin_->setRange(1, 9999);
    extraTurnsSpin_->setValue(20);
    reviveCostSpin_ = new QSpinBox;
    reviveCostSpin_->setObjectName(QStringLiteral("inputControl"));
    reviveCostSpin_->setRange(0, 1000);
    reviveCostSpin_->setValue(5);
    bossForm->addRow(QStringLiteral("血量"), bossHealthEdit_);
    bossForm->addRow(QStringLiteral("技能"), skillsEdit_);
    bossForm->addRow(QStringLiteral("回合"), extraTurnsSpin_);
    bossForm->addRow(QStringLiteral("复活"), reviveCostSpin_);
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
    panelLayout->addWidget(aiGroup);
    connect(aiButton, &QPushButton::clicked, this, &MainWindow::runAiPlayer);

    panelLayout->addWidget(makeSeparator());

    auto *optGroup = new QGroupBox(QStringLiteral("⑤ 遗传算法迷宫优化"));
    optGroup->setObjectName(QStringLiteral("taskGroup"));
    auto *optLayout = new QVBoxLayout(optGroup);
    optLayout->setSpacing(8);
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
    optAlgoBox_ = new QComboBox;
    optAlgoBox_->setObjectName(QStringLiteral("inputControl"));
    optAlgoBox_->addItems({QStringLiteral("分治"),
                           QStringLiteral("Kruskal MST"),
                           QStringLiteral("DFS"),
                           QStringLiteral("BFS 分支限界")});
    optAlgoBox_->setCurrentIndex(3);
    optForm->addRow(QStringLiteral("种群"), optPopSpin_);
    optForm->addRow(QStringLiteral("代数"), optGenSpin_);
    optForm->addRow(QStringLiteral("变异率"), optMutSpin_);
    optForm->addRow(QStringLiteral("基算法"), optAlgoBox_);

    optRLCheck_ = new QCheckBox(QStringLiteral("启用 Q-Learning 精调"));
    optRLCheck_->setObjectName(QStringLiteral("inputControl"));
    optRLCheck_->setToolTip(QStringLiteral("每代 GA 后用 RL 微调 top-k 个体（增加 CPU 负载）"));
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
        optRLEpisodesSpin_->setEnabled(checked);
        optRLTopKSpin_->setEnabled(checked);
    });

    optLayout->addLayout(optForm);
    auto *optButtons = new QHBoxLayout;
    optButtons->setSpacing(8);
    optRunButton_ = new QPushButton(QStringLiteral("运行优化"));
    optRunButton_->setObjectName(QStringLiteral("primaryButton"));
    optStopButton_ = new QPushButton(QStringLiteral("停止"));
    optStopButton_->setObjectName(QStringLiteral("secondaryButton"));
    optStopButton_->setEnabled(false);
    optButtons->addWidget(optRunButton_);
    optButtons->addWidget(optStopButton_);
    optLayout->addLayout(optButtons);
    optProgressLabel_ = new QLabel(QStringLiteral("PAIRED 适应度 = DP最优 − Greedy玩家得分"));
    optProgressLabel_->setObjectName(QStringLiteral("infoCard"));
    optProgressLabel_->setWordWrap(true);
    optLayout->addWidget(optProgressLabel_);
    optResultLabel_ = new QLabel;
    optResultLabel_->setObjectName(QStringLiteral("resultLabel"));
    optResultLabel_->setWordWrap(true);
    optResultLabel_->setVisible(false);
    optLayout->addWidget(optResultLabel_);
    auto *applyOptButton = new QPushButton(QStringLiteral("应用优化迷宫"));
    applyOptButton->setObjectName(QStringLiteral("accentButton"));
    applyOptButton->setEnabled(false);
    optApplyButton_ = applyOptButton;
    optLayout->addWidget(applyOptButton);
    panelLayout->addWidget(optGroup);
    connect(optRunButton_, &QPushButton::clicked, this, &MainWindow::runOptimizer);
    connect(optApplyButton_, &QPushButton::clicked, this, &MainWindow::applyOptimizedMaze);

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
        QMainWindow { background: #0f172a; }
        QSplitter::handle { background: #334155; }

        QScrollArea { background: #0f172a; border: none; }
        QScrollArea > QWidget > QWidget { background: #0f172a; }

        #panelTitle {
            color: #f8fafc; font-size: 18px; font-weight: 700;
            padding: 4px 0 2px 0; letter-spacing: 1px;
        }
        #legendLabel {
            color: #94a3b8; font-size: 11px; padding: 2px 0 6px 0;
        }
        #separator {
            border: none; border-top: 1px solid #1e293b;
            margin: 4px 0;
        }

        QGroupBox#taskGroup {
            color: #e2e8f0; font-size: 13px; font-weight: 600;
            border: 1px solid #334155; border-radius: 8px;
            margin-top: 14px; padding: 14px 10px 10px 10px;
            background: #1e293b;
        }
        QGroupBox#taskGroup::title {
            subcontrol-origin: margin; left: 12px; padding: 0 6px;
            color: #fbbf24;
        }

        #infoCard {
            background: #172554; color: #93c5fd;
            padding: 10px; border-radius: 6px; font-size: 11px;
            border: 1px solid #1e3a5f;
        }

        QLabel { color: #cbd5e1; font-size: 12px; }
        QLabel#resultLabel {
            color: #a7f3d0; font-size: 12px;
            padding: 6px 8px; background: #064e3b;
            border-radius: 5px; border: 1px solid #065f46;
        }

        QFormLayout QLabel { color: #94a3b8; font-size: 12px; }

        QSpinBox#inputControl, QComboBox#inputControl, QLineEdit#inputControl {
            background: #0f172a; color: #e2e8f0;
            border: 1px solid #475569; border-radius: 5px;
            padding: 5px 8px; font-size: 12px; min-height: 22px;
        }
        QSpinBox#inputControl:focus, QComboBox#inputControl:focus,
        QLineEdit#inputControl:focus {
            border-color: #fbbf24;
        }
        QComboBox#inputControl::drop-down {
            border: none; width: 20px;
        }
        QComboBox#inputControl::down-arrow {
            image: none; border-left: 4px solid transparent;
            border-right: 4px solid transparent; border-top: 5px solid #94a3b8;
            margin-right: 6px;
        }
        QComboBox#inputControl QAbstractItemView {
            background: #1e293b; color: #e2e8f0;
            selection-background-color: #334155;
            border: 1px solid #475569;
        }

        QPlainTextEdit#outputConsole {
            background: #0c1222; color: #a7f3d0;
            border: 1px solid #334155; border-radius: 6px;
            padding: 8px; font-family: 'Menlo', 'Consolas', monospace;
            font-size: 11px;
        }

        QPushButton#primaryButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #f59e0b, stop:1 #d97706);
            color: #1c1917; font-weight: 700; font-size: 12px;
            border: none; border-radius: 6px; padding: 8px 14px;
        }
        QPushButton#primaryButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #fbbf24, stop:1 #f59e0b);
        }
        QPushButton#primaryButton:pressed { background: #b45309; }

        QPushButton#secondaryButton {
            background: #334155; color: #e2e8f0; font-size: 12px;
            border: 1px solid #475569; border-radius: 6px; padding: 8px 14px;
        }
        QPushButton#secondaryButton:hover { background: #475569; }
        QPushButton#secondaryButton:pressed { background: #1e293b; }

        QPushButton#accentButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #06b6d4, stop:1 #0891b2);
            color: #0c1222; font-weight: 700; font-size: 12px;
            border: none; border-radius: 6px; padding: 8px 14px;
        }
        QPushButton#accentButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #22d3ee, stop:1 #06b6d4);
        }
        QPushButton#accentButton:pressed { background: #0e7490; }

        QStatusBar { background: #0f172a; color: #94a3b8; font-size: 11px; }
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
                             QStringLiteral("血量需为正整数列表；技能格式为“名称:伤害:冷却”，且至少有一个无冷却技能。"));
        return;
    }

    lastBossResult_ = BossSolver::solve(health, skills);
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
            "────────────────────────────────\n"
            "最优序列      %4\n"
            "────────────────────────────────\n"
            "搜索展开 %5    剪枝 %6")
            .arg(lastBossResult_.minimumTurns)
            .arg(extraTurnsSpin_->value())
            .arg(reviveCostSpin_->value())
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

    lastBossResult_ = BossSolver::solve(health, skills);
    if (!lastBossResult_.solved
        || !BossSolver::verify(health, skills, lastBossResult_.skillSequence)) {
        QMessageBox::warning(this, QStringLiteral("无法开始战斗"),
                             QStringLiteral("当前参数没有得到可验证的技能序列。"));
        return;
    }

    auto *battleWindow = new BattleWindow(
        health, skills, lastBossResult_,
        extraTurnsSpin_->value(),
        reviveCostSpin_->value(), this);
    battleWindow->show();
    battleWindow->raise();
    battleWindow->activateWindow();
}

void MainWindow::runAiPlayer() {
    if (maze_.cellCount() == 0) {
        return;
    }
    generationTimer_->stop();
    aiPathTimer_->stop();
    mazeWidget_->clearAiPath();
    lastAiResult_ = GreedyPlayer::play(maze_);
    if (!lastAiResult_.reachedEnd) {
        statusBar()->showMessage(QStringLiteral("AI 玩家未能到达终点"), 5000);
        return;
    }
    revealedAiPoints_ = 1;
    mazeWidget_->setAiPath(lastAiResult_.walk, revealedAiPoints_);
    aiPathTimer_->start();
    statusBar()->showMessage(
        QStringLiteral("AI 玩家完成：score=%1, 步数=%2, 金币=%3, 陷阱=%4")
            .arg(lastAiResult_.score, 0, 'f', 2)
            .arg(lastAiResult_.totalSteps)
            .arg(lastAiResult_.collectedCoins)
            .arg(lastAiResult_.triggeredTraps),
        8000);
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
    validationLabel_->setStyleSheet(valid ? QStringLiteral("color:#167143;")
                                          : QStringLiteral("color:#b13232;"));
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
    lastBossResult_ = BossSolver::solve(health, skills);
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
        maze_, health, skills, extraTurnsSpin_->value(), reviveCostSpin_->value());

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
    if (maze_.cellCount() == 0) {
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
    cfg.baseAlgorithm = static_cast<MazeAlgorithm>(optAlgoBox_->currentIndex());
    cfg.seed = static_cast<quint32>(seedSpin_->value());
    cfg.enableRL = optRLCheck_->isChecked();
    cfg.rlEpisodes = optRLEpisodesSpin_->value();
    cfg.rlTopK = optRLTopKSpin_->value();

    optRunButton_->setEnabled(false);
    optStopButton_->setEnabled(true);
    optApplyButton_->setEnabled(false);
    optResultLabel_->setVisible(false);
    optProgressLabel_->setText(QStringLiteral("正在优化... 第 0 / %1 代").arg(cfg.generations));

    auto *optimizer = new MazeOptimizer(this);
    optimizer->setConfig(cfg);

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
                QCoreApplication::processEvents();
            });

    connect(optStopButton_, &QPushButton::clicked, optimizer, &MazeOptimizer::stop);

    connect(optimizer, &MazeOptimizer::finished, this,
            [this, optimizer](const MazeModel &bestMaze) {
                optimizedMaze_ = bestMaze;
                hasOptimizedMaze_ = true;
                optRunButton_->setEnabled(true);
                optStopButton_->setEnabled(false);
                optApplyButton_->setEnabled(true);

                ResourcePlan dp = optimizedMaze_.optimalResourceWalk();
                PlayResult greedy = GreedyPlayer::play(optimizedMaze_);
                optResultLabel_->setText(
                    QStringLiteral("<b>优化完成</b>  |  Regret = %1<br/>"
                                   "DP最优 %2  |  Greedy %3  |  差距 %4")
                        .arg(dp.maxValue - greedy.remainingResource)
                        .arg(dp.maxValue)
                        .arg(greedy.remainingResource)
                        .arg(dp.maxValue - greedy.remainingResource));
                optResultLabel_->setVisible(true);
                optimizer->deleteLater();
            });

    optimizer->run();
}

void MainWindow::applyOptimizedMaze() {
    if (!hasOptimizedMaze_) {
        return;
    }
    maze_ = optimizedMaze_;
    lastPlan_ = {};
    lastBossResult_ = {};
    resourceResultLabel_->setText(QStringLiteral("已应用优化迷宫，等待 DP 求解"));
    mazeWidget_->setMaze(maze_);
    mazeWidget_->clearAiPath();
    revealedEdges_ = static_cast<int>(maze_.generationSteps().size());
    mazeWidget_->setRevealCount(revealedEdges_);
    updateValidation();
    statusBar()->showMessage(QStringLiteral("已应用遗传优化迷宫"), 5000);
}
