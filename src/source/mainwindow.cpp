#include "mainwindow.h"

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
        "S 起点  E 终点/BOSS  ● 金币(+50)  ▲ 陷阱(-30)\n"
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
                             QStringLiteral("分支限界 / 随机代价 BFS")});
    rowsSpin_ = new QSpinBox;
    rowsSpin_->setRange(5, 31);
    rowsSpin_->setValue(15);
    columnsSpin_ = new QSpinBox;
    columnsSpin_->setRange(5, 31);
    columnsSpin_->setValue(15);
    seedSpin_ = new QSpinBox;
    seedSpin_->setRange(1, 999999999);
    seedSpin_->setValue(202506);
    animationSpin_ = new QSpinBox;
    animationSpin_->setRange(1, 200);
    animationSpin_->setValue(12);
    animationSpin_->setSuffix(QStringLiteral(" ms/步"));
    generationForm->addRow(QStringLiteral("算法"), algorithmBox_);
    generationForm->addRow(QStringLiteral("行数"), rowsSpin_);
    generationForm->addRow(QStringLiteral("列数"), columnsSpin_);
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
        QStringLiteral("BFS 分支限界：优先队列按深度下界扩展，每次只接受一条候选边；随机代价打散同层分支，visited 剪去重复与成环状态。时间 O(E log E)，空间 O(E)，形态更接近标准迷宫。")};
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
    coinSpin_->setValue(28);
    trapSpin_ = new QSpinBox;
    trapSpin_->setRange(0, 300);
    trapSpin_->setValue(18);
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
    bossHealthEdit_ = new QLineEdit(QStringLiteral("35,45,60"));
    skillsEdit_ = new QLineEdit(QStringLiteral("普通攻击:5:0;重击:10:2;大招:18:4"));
    skillsEdit_->setToolTip(QStringLiteral("格式：名称:伤害:冷却；至少一个技能冷却为 0"));
    extraTurnsSpin_ = new QSpinBox;
    extraTurnsSpin_->setRange(0, 20);
    extraTurnsSpin_->setValue(2);
    reviveCostSpin_ = new QSpinBox;
    reviveCostSpin_->setRange(0, 10000);
    reviveCostSpin_->setValue(100);
    reviveCostSpin_->setSingleStep(50);
    bossForm->addRow(QStringLiteral("BOSS 血量"), bossHealthEdit_);
    bossForm->addRow(QStringLiteral("技能 名称:伤害:冷却"), skillsEdit_);
    bossForm->addRow(QStringLiteral("限定回合余量"), extraTurnsSpin_);
    bossForm->addRow(QStringLiteral("复活金币"), reviveCostSpin_);
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

    auto *exportButton = new QPushButton(QStringLiteral("导出当前迷宫与算法结果（JSON）"));
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
    const auto algorithm = static_cast<MazeAlgorithm>(algorithmBox_->currentIndex());
    maze_.generate(rowsSpin_->value(), columnsSpin_->value(), algorithm,
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
            .arg(lastBossResult_.minimumTurns + extraTurnsSpin_->value())
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
        lastBossResult_.minimumTurns + extraTurnsSpin_->value(),
        reviveCostSpin_->value(), this);
    battleWindow->show();
    battleWindow->raise();
    battleWindow->activateWindow();
}

void MainWindow::updateValidation() {
    QString reason;
    const bool valid = maze_.validatePerfect(&reason);
    validationLabel_->setText(reason);
    validationLabel_->setStyleSheet(valid ? QStringLiteral("color:#167143;")
                                          : QStringLiteral("color:#b13232;"));
}

void MainWindow::exportMaze() {
    if (maze_.cellCount() == 0) {
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出迷宫"), QStringLiteral("maze_export.json"),
        QStringLiteral("JSON 文件 (*.json)"));
    if (path.isEmpty()) {
        return;
    }

    QJsonObject root = maze_.toJson();
    root.insert(QStringLiteral("generator"), algorithmBox_->currentText());
    root.insert(QStringLiteral("seed"), seedSpin_->value());
    QString validation;
    root.insert(QStringLiteral("perfectMaze"), maze_.validatePerfect(&validation));
    root.insert(QStringLiteral("validation"), validation);

    if (!lastPlan_.walk.isEmpty()) {
        QJsonObject plan;
        plan.insert(QStringLiteral("maximumResource"), lastPlan_.maxValue);
        QJsonArray walk;
        for (int cell : lastPlan_.walk) {
            walk.append(cell);
        }
        plan.insert(QStringLiteral("walk"), walk);
        root.insert(QStringLiteral("dynamicProgrammingPlan"), plan);
    }

    bool healthOk = false;
    bool skillsOk = false;
    const QVector<int> health = parseBossHealth(&healthOk);
    const QVector<BossSkill> skills = parseSkills(&skillsOk);
    if (healthOk && skillsOk) {
        QJsonObject battle;
        QJsonArray bosses;
        for (int value : health) {
            bosses.append(value);
        }
        battle.insert(QStringLiteral("bossHealth"), bosses);
        QJsonArray skillArray;
        for (const BossSkill &skill : skills) {
            QJsonObject item;
            item.insert(QStringLiteral("name"), skill.name);
            item.insert(QStringLiteral("damage"), skill.damage);
            item.insert(QStringLiteral("cooldown"), skill.cooldown);
            skillArray.append(item);
        }
        battle.insert(QStringLiteral("skills"), skillArray);
        battle.insert(QStringLiteral("reviveCoins"), reviveCostSpin_->value());
        if (lastBossResult_.solved) {
            battle.insert(QStringLiteral("minimumTurns"), lastBossResult_.minimumTurns);
            battle.insert(QStringLiteral("turnLimit"),
                          lastBossResult_.minimumTurns + extraTurnsSpin_->value());
            QJsonArray sequence;
            for (int skillIndex : lastBossResult_.skillSequence) {
                sequence.append(skills[skillIndex].name);
            }
            battle.insert(QStringLiteral("optimalSkillSequence"), sequence);
        }
        root.insert(QStringLiteral("bossBattle"), battle);
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0
        || !file.commit()) {
        QMessageBox::critical(this, QStringLiteral("导出失败"), file.errorString());
        return;
    }
    statusBar()->showMessage(QStringLiteral("已导出：%1").arg(path), 5000);
}
