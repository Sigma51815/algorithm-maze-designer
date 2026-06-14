#include "mainwindow.h"

#include "ai/aiplayerformat.h"
#include "battlewindow.h"
#include "mazewidget.h"

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
    setWindowTitle(QStringLiteral("算法驱动的迷宫设计 - Qt 图形化课程设计"));
    resize(1220, 780);

    auto *splitter = new QSplitter(this);
    mazeWidget_ = new MazeWidget(splitter);

    auto *scrollArea = new QScrollArea(splitter);
    scrollArea->setWidgetResizable(true);
    scrollArea->setMinimumWidth(350);
    scrollArea->setMaximumWidth(430);
    auto *panel = new QWidget;
    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(14, 14, 14, 14);
    panelLayout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("迷宫设计任务控制台"));
    QFont titleFont = title->font();
    titleFont.setPointSize(15);
    titleFont.setBold(true);
    title->setFont(titleFont);
    panelLayout->addWidget(title);

    auto *legend = new QLabel(QStringLiteral(
        "S 起点  B 独立 BOSS 格  E 终点  ● 金币(+50)  ▲ 陷阱(-30)\n"
        "蓝线为动态规划得到的最优资源收集行走路径。"));
    legend->setWordWrap(true);
    legend->setStyleSheet(QStringLiteral("color:#52616b;"));
    panelLayout->addWidget(legend);

    auto *generationGroup = new QGroupBox(QStringLiteral("任务 1：多方法生成完美迷宫"));
    auto *generationLayout = new QVBoxLayout(generationGroup);
    auto *generationForm = new QFormLayout;
    algorithmBox_ = new QComboBox;
    algorithmBox_->addItems({QStringLiteral("分治"),
                             QStringLiteral("贪心 / Kruskal 最小生成树"),
                             QStringLiteral("回溯 / DFS"),
                             QStringLiteral("分支限界 / 严格分层 BFS")});
    rowsSpin_ = new QSpinBox;
    rowsSpin_->setRange(5, 31);
    rowsSpin_->setSingleStep(2);
    rowsSpin_->setValue(15);
    columnsSpin_ = new QSpinBox;
    columnsSpin_->setRange(5, 31);
    columnsSpin_->setSingleStep(2);
    columnsSpin_->setValue(15);
    seedSpin_ = new QSpinBox;
    seedSpin_->setRange(1, 999999999);
    seedSpin_->setValue(202506);
    animationSpin_ = new QSpinBox;
    animationSpin_->setRange(1, 200);
    animationSpin_->setValue(12);
    animationSpin_->setSuffix(QStringLiteral(" ms/步"));
    generationForm->addRow(QStringLiteral("算法"), algorithmBox_);
    generationForm->addRow(QStringLiteral("输出矩阵行数"), rowsSpin_);
    generationForm->addRow(QStringLiteral("输出矩阵列数"), columnsSpin_);
    generationForm->addRow(QStringLiteral("随机种子"), seedSpin_);
    generationForm->addRow(QStringLiteral("动画速度"), animationSpin_);
    generationLayout->addLayout(generationForm);

    auto *generateButton = new QPushButton(QStringLiteral("生成并播放过程"));
    generationLayout->addWidget(generateButton);
    validationLabel_ = new QLabel;
    validationLabel_->setWordWrap(true);
    generationLayout->addWidget(validationLabel_);

    auto *complexityLabel = new QLabel;
    complexityLabel->setWordWrap(true);
    complexityLabel->setStyleSheet(QStringLiteral(
        "background:#eef5f5; color:#264b4b; padding:8px; border-radius:5px;"));
    generationLayout->addWidget(complexityLabel);
    panelLayout->addWidget(generationGroup);

    const QStringList complexityDescriptions{
        QStringLiteral("分治：递归拆分矩形区域，每次只连接两个子迷宫一次。时间 O(V)，递归空间 O(log V)（均衡时），结构分区明显。"),
        QStringLiteral("Kruskal MST：随机边排序并用并查集防环。时间 O(E log E)，空间 O(V+E)，分支较均衡。"),
        QStringLiteral("回溯 DFS：深入未访问邻格，遇到死路回退。时间 O(V+E)，空间 O(V)，长走廊较多。"),
        QStringLiteral("BFS 分支限界：候选迷宫严格按深度层扩展，随机代价只打散同层分支，visited 剪去重复与成环状态；固定次数随机重启后选取解路径更长的迷宫。时间 O(E log E)，空间 O(E)。")};
    auto updateComplexity = [=](int index) {
        complexityLabel->setText(complexityDescriptions.value(index));
    };
    connect(algorithmBox_, &QComboBox::currentIndexChanged, this, updateComplexity);
    updateComplexity(algorithmBox_->currentIndex());
    connect(generateButton, &QPushButton::clicked, this, &MainWindow::generateMaze);

    auto *resourceGroup = new QGroupBox(QStringLiteral("任务 2：动态规划资源收集"));
    auto *resourceLayout = new QVBoxLayout(resourceGroup);
    auto *resourceForm = new QFormLayout;
    coinSpin_ = new QSpinBox;
    coinSpin_->setRange(0, 300);
    coinSpin_->setValue(8);
    trapSpin_ = new QSpinBox;
    trapSpin_->setRange(0, 300);
    trapSpin_->setValue(6);
    resourceForm->addRow(QStringLiteral("金币数量"), coinSpin_);
    resourceForm->addRow(QStringLiteral("陷阱数量"), trapSpin_);
    resourceLayout->addLayout(resourceForm);
    auto *resourceButtons = new QHBoxLayout;
    auto *placeButton = new QPushButton(QStringLiteral("重新布置"));
    auto *solveResourceButton = new QPushButton(QStringLiteral("DP 求最优路径"));
    resourceButtons->addWidget(placeButton);
    resourceButtons->addWidget(solveResourceButton);
    resourceLayout->addLayout(resourceButtons);
    resourceResultLabel_ = new QLabel(QStringLiteral("尚未求解"));
    resourceResultLabel_->setWordWrap(true);
    resourceLayout->addWidget(resourceResultLabel_);
    panelLayout->addWidget(resourceGroup);
    connect(placeButton, &QPushButton::clicked, this, &MainWindow::placeResources);
    connect(solveResourceButton, &QPushButton::clicked, this, &MainWindow::solveResources);

    auto *bossGroup = new QGroupBox(QStringLiteral("任务 3：分支限界 BOSS 战"));
    auto *bossLayout = new QVBoxLayout(bossGroup);
    auto *bossForm = new QFormLayout;
    bossHealthEdit_ = new QLineEdit(QStringLiteral("11,13,9,15"));
    skillsEdit_ = new QLineEdit(
        QStringLiteral("强力攻击:8:4;普通攻击:2:0;连击:4:2;重击:6:3"));
    skillsEdit_->setToolTip(QStringLiteral("格式：名称:伤害:冷却；至少一个技能冷却为 0"));
    extraTurnsSpin_ = new QSpinBox;
    extraTurnsSpin_->setRange(1, 9999);
    extraTurnsSpin_->setValue(20);
    reviveCostSpin_ = new QSpinBox;
    reviveCostSpin_->setRange(0, 1000);
    reviveCostSpin_->setValue(5);
    bossForm->addRow(QStringLiteral("BOSS 血量"), bossHealthEdit_);
    bossForm->addRow(QStringLiteral("技能 名称:伤害:冷却"), skillsEdit_);
    bossForm->addRow(QStringLiteral("限定回合数 minRouds"), extraTurnsSpin_);
    bossForm->addRow(QStringLiteral("失败金币消耗"), reviveCostSpin_);
    bossLayout->addLayout(bossForm);
    auto *solveBossButton = new QPushButton(QStringLiteral("分支限界求最优技能序列"));
    bossLayout->addWidget(solveBossButton);
    auto *battleAnimationButton = new QPushButton(QStringLiteral("打开可视化战斗动画"));
    bossLayout->addWidget(battleAnimationButton);
    bossOutput_ = new QPlainTextEdit;
    bossOutput_->setReadOnly(true);
    bossOutput_->setMaximumHeight(150);
    bossOutput_->setPlaceholderText(QStringLiteral("求解结果会显示在这里"));
    bossLayout->addWidget(bossOutput_);
    panelLayout->addWidget(bossGroup);
    connect(solveBossButton, &QPushButton::clicked, this, &MainWindow::solveBossBattle);
    connect(battleAnimationButton, &QPushButton::clicked,
            this, &MainWindow::showBattleAnimation);

    auto *aiGroup = new QGroupBox(QStringLiteral("AI 玩家探险"));
    auto *aiLayout = new QVBoxLayout(aiGroup);
    auto *aiButton = new QPushButton(QStringLiteral("运行贪心 AI 玩家"));
    aiLayout->addWidget(aiButton);
    panelLayout->addWidget(aiGroup);
    connect(aiButton, &QPushButton::clicked, this, &MainWindow::runAiPlayer);

    auto *exportButton = new QPushButton(QStringLiteral("导出 AI 玩家输入 JSON"));
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

    setStyleSheet(QStringLiteral(
        "QMainWindow{background:#f4f7f8;}"
        "QGroupBox{font-weight:600; border:1px solid #cfdadd; border-radius:7px;"
        " margin-top:10px; padding:12px 8px 8px 8px; background:white;}"
        "QGroupBox::title{subcontrol-origin:margin; left:10px; padding:0 5px;}"
        "QPushButton{background:#167d7d; color:white; border:0; border-radius:5px; padding:7px;}"
        "QPushButton:hover{background:#116565;}"));
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
        QStringLiteral("最多资源：%1；行走 %2 步；首次经过 %3 个格子。路径允许回访，金币/陷阱只计一次。")
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
        QStringLiteral("最少回合数：%1\n限定回合数：%2\n复活所需金币：%3\n"
                       "最优技能序列：%4\n搜索状态：%5，剪枝：%6")
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
        QStringLiteral("%1\n输出矩阵：%2×%3；挑战性：解路径 %4 步，死路 %5，分叉点 %6，最长走廊 %7")
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
