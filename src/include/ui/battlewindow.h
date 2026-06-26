#pragma once

#include "bosssolver.h"

#include <QMainWindow>
#include <QWidget>

#include <functional>

class QLabel;
class QPushButton;
class QTimer;

class BattleScene : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal attackProgress READ attackProgress WRITE setAttackProgress)

public:
    explicit BattleScene(QWidget *parent = nullptr);

    void setBattleState(int bossIndex,
                        int bossCount,
                        int health,
                        int maximumHealth,
                        int turn,
                        int turnLimit,
                        int coins);
    void animateAttack(const BossSkill &skill,
                       int damage,
                       const std::function<void()> &finished);
    [[nodiscard]] qreal attackProgress() const { return attackProgress_; }
    void setAttackProgress(qreal progress);

protected:
    void paintEvent(QPaintEvent *event) override;
    QSize minimumSizeHint() const override;

private:
    int bossIndex_ = 0;
    int bossCount_ = 1;
    int health_ = 1;
    int maximumHealth_ = 1;
    int turn_ = 0;
    int turnLimit_ = 1;
    int reviveCost_ = 0;
    int damage_ = 0;
    BossSkill activeSkill_;
    qreal attackProgress_ = 0.0;
};

class BattleWindow : public QMainWindow {
    Q_OBJECT

public:
    BattleWindow(const QVector<int> &bossHealth,
                 const QVector<BossSkill> &skills,
                 const BossResult &optimalResult,
                 int turnLimit,
                 int reviveCoins,
                 DamageOverflowMode damageMode = DamageOverflowMode::NoOverflow,
                 QWidget *parent = nullptr);

private:
    QVector<int> initialHealth_;
    QVector<int> currentHealth_;
    QVector<BossSkill> skills_;
    BossResult optimalResult_;
    QVector<int> cooldowns_;
    int turnLimit_ = 0;
    int reviveCoins_ = 0;
    DamageOverflowMode damageMode_ = DamageOverflowMode::NoOverflow;
    int currentBoss_ = 0;
    int turn_ = 0;
    int autoStep_ = 0;
    bool animating_ = false;
    bool autoPlaying_ = false;

    BattleScene *scene_ = nullptr;
    QLabel *messageLabel_ = nullptr;
    QLabel *roundLabel_ = nullptr;
    QLabel *modeLabel_ = nullptr;
    QVector<QPushButton *> skillButtons_;
    QPushButton *autoButton_ = nullptr;
    QTimer *autoTimer_ = nullptr;

    void buildUi();
    void resetBattle();
    void useSkill(int skillIndex);
    void finishTurn(int skillIndex, int damage);
    void startAutoPlay();
    void stopAutoPlay();
    void advanceAutoPlay();
    void refreshUi();
    [[nodiscard]] int firstLivingBoss() const;
};
