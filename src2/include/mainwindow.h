#pragma once

#include "bosssolver.h"
#include "maze.h"

#include <QMainWindow>

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
    int revealedEdges_ = 0;
    int revealedPathPoints_ = 0;

    void buildUi();
    void generateMaze();
    void placeResources();
    void solveResources();
    void solveBossBattle();
    void showBattleAnimation();
    void exportMaze();
    void updateValidation();
    [[nodiscard]] QVector<int> parseBossHealth(bool *ok = nullptr) const;
    [[nodiscard]] QVector<BossSkill> parseSkills(bool *ok = nullptr) const;
};
