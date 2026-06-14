#pragma once

#include "ai/greedy_player.h"
#include "ai/rl_player.h"
#include "bosssolver.h"
#include "maze.h"
#include "maze_optimizer.h"

#include <QMainWindow>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTimer;
class MazeWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    MazeModel maze_;
    ResourcePlan lastPlan_;
    BossResult lastBossResult_;
    PlayResult lastAiResult_;
    MazeWidget *mazeWidget_ = nullptr;
    QComboBox *algorithmBox_ = nullptr;
    QSpinBox *rowsSpin_ = nullptr;
    QSpinBox *columnsSpin_ = nullptr;
    QSpinBox *seedSpin_ = nullptr;
    QSpinBox *animationSpin_ = nullptr;
    QSpinBox *coinSpin_ = nullptr;
    QSpinBox *trapSpin_ = nullptr;
    QLabel *validationLabel_ = nullptr;
    QLabel *resourceResultLabel_ = nullptr;
    QLineEdit *bossHealthEdit_ = nullptr;
    QLineEdit *skillsEdit_ = nullptr;
    QSpinBox *extraTurnsSpin_ = nullptr;
    QSpinBox *reviveCostSpin_ = nullptr;
    QPlainTextEdit *bossOutput_ = nullptr;
    QTimer *generationTimer_ = nullptr;
    QTimer *pathTimer_ = nullptr;
    QTimer *aiPathTimer_ = nullptr;
    int revealedEdges_ = 0;
    int revealedPathPoints_ = 0;
    int revealedAiPoints_ = 0;

    QSpinBox *optPopSpin_ = nullptr;
    QSpinBox *optGenSpin_ = nullptr;
    QSpinBox *optMutSpin_ = nullptr;
    QComboBox *optAlgoBox_ = nullptr;
    QLabel *optProgressLabel_ = nullptr;
    QLabel *optResultLabel_ = nullptr;
    QPushButton *optRunButton_ = nullptr;
    QPushButton *optStopButton_ = nullptr;
    QPushButton *optApplyButton_ = nullptr;
    QPushButton *optSaveButton_ = nullptr;
    QCheckBox *optRLCheck_ = nullptr;
    QSpinBox *optRLEpisodesSpin_ = nullptr;
    QSpinBox *optRLTopKSpin_ = nullptr;
    MazeModel optimizedMaze_;
    bool hasOptimizedMaze_ = false;
    OptimizerConfig lastOptConfig_;
    int lastOptFitness_ = 0;
    int lastOptDpScore_ = 0;
    int lastOptGreedyScore_ = 0;

    void buildUi();
    void generateMaze();
    void placeResources();
    void solveResources();
    void solveBossBattle();
    void showBattleAnimation();
    void exportMaze();
    void runAiPlayer();
    void runOptimizer();
    void applyOptimizedMaze();
    void saveOptimizedMaze();
    void updateValidation();
    [[nodiscard]] QVector<int> parseBossHealth(bool *ok = nullptr) const;
    [[nodiscard]] QVector<BossSkill> parseSkills(bool *ok = nullptr) const;
};
