#include "battlewindow.h"

#include <QAbstractAnimation>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

QColor bossColor(int index) {
    static const QVector<QColor> colors{QColor("#7457d6"), QColor("#d35b58"),
                                        QColor("#384b78"), QColor("#34846c")};
    return colors[index % colors.size()];
}

void drawHealthBar(QPainter &painter,
                   const QRectF &rect,
                   int value,
                   int maximum,
                   const QColor &color) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(28, 40, 55, 210));
    painter.drawRoundedRect(rect, 8, 8);
    const qreal ratio = maximum > 0
        ? std::clamp(static_cast<qreal>(value) / maximum, 0.0, 1.0)
        : 0.0;
    QRectF fill = rect.adjusted(4, 4, -4, -4);
    fill.setWidth(fill.width() * ratio);
    painter.setBrush(color);
    painter.drawRoundedRect(fill, 5, 5);
}

} // namespace

BattleScene::BattleScene(QWidget *parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QSize BattleScene::minimumSizeHint() const {
    return {820, 450};
}

void BattleScene::setBattleState(int bossIndex,
                                 int bossCount,
                                 int health,
                                 int maximumHealth,
                                 int turn,
                                 int turnLimit,
                                 int coins) {
    bossIndex_ = bossIndex;
    bossCount_ = bossCount;
    health_ = health;
    maximumHealth_ = maximumHealth;
    turn_ = turn;
    turnLimit_ = turnLimit;
    reviveCost_ = coins;
    update();
}

void BattleScene::animateAttack(const BossSkill &skill,
                                int damage,
                                const std::function<void()> &finished) {
    activeSkill_ = skill;
    damage_ = damage;
    auto *animation = new QPropertyAnimation(this, "attackProgress", this);
    animation->setDuration(850);
    animation->setStartValue(0.0);
    animation->setKeyValueAt(0.35, 0.42);
    animation->setKeyValueAt(0.7, 0.82);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::InOutCubic);
    connect(animation, &QPropertyAnimation::finished, this, [this, finished] {
        attackProgress_ = 0.0;
        activeSkill_ = {};
        damage_ = 0;
        update();
        finished();
    });
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void BattleScene::setAttackProgress(qreal progress) {
    attackProgress_ = progress;
    update();
}

void BattleScene::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QFont baseFont(QStringLiteral("Microsoft YaHei"));
    baseFont.setPixelSize(15);
    painter.setFont(baseFont);
    const QRectF area = rect();

    QLinearGradient sky(0, 0, 0, height());
    sky.setColorAt(0.0, QColor("#85c7e8"));
    sky.setColorAt(0.55, QColor("#d5eff5"));
    sky.setColorAt(1.0, QColor("#7bbf88"));
    painter.fillRect(area, sky);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 80));
    for (int i = 0; i < 6; ++i) {
        const qreal x = width() * (0.08 + i * 0.18);
        const qreal y = height() * (0.12 + (i % 2) * 0.08);
        painter.drawEllipse(QPointF(x, y), 42, 16);
        painter.drawEllipse(QPointF(x + 30, y - 5), 35, 19);
    }

    QPainterPath hills;
    hills.moveTo(0, height() * 0.62);
    hills.cubicTo(width() * 0.18, height() * 0.35,
                  width() * 0.35, height() * 0.72,
                  width() * 0.54, height() * 0.48);
    hills.cubicTo(width() * 0.72, height() * 0.28,
                  width() * 0.88, height() * 0.58,
                  width(), height() * 0.42);
    hills.lineTo(width(), height());
    hills.lineTo(0, height());
    painter.setBrush(QColor("#65a56f"));
    painter.drawPath(hills);

    painter.setBrush(QColor(43, 80, 54, 100));
    painter.drawEllipse(QRectF(width() * 0.08, height() * 0.72,
                               width() * 0.34, height() * 0.13));
    painter.drawEllipse(QRectF(width() * 0.62, height() * 0.57,
                               width() * 0.3, height() * 0.12));

    const qreal lunge = attackProgress_ < 0.5
        ? attackProgress_ * width() * 0.12
        : (1.0 - attackProgress_) * width() * 0.12;
    const QPointF hero(width() * 0.25 + lunge, height() * 0.62);
    const QPointF boss(width() * 0.76, height() * 0.46);
    const bool hitFlash = attackProgress_ > 0.55 && attackProgress_ < 0.82;

    painter.save();
    painter.translate(hero);
    painter.setPen(QPen(QColor("#183a58"), 5, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(QColor("#f4d35e"));
    painter.drawEllipse(QPointF(0, -45), 48, 43);
    painter.setBrush(QColor("#2d84b8"));
    painter.drawRoundedRect(QRectF(-42, -18, 84, 82), 24, 24);
    painter.setBrush(QColor("#fff2cf"));
    painter.drawEllipse(QPointF(-17, -51), 7, 9);
    painter.drawEllipse(QPointF(17, -51), 7, 9);
    painter.setBrush(QColor("#243a4d"));
    painter.drawEllipse(QPointF(-17, -51), 3, 5);
    painter.drawEllipse(QPointF(17, -51), 3, 5);
    painter.drawArc(QRectF(-13, -44, 26, 16), 200 * 16, 140 * 16);
    painter.setBrush(QColor("#f4d35e"));
    painter.drawPolygon(QPolygonF{QPointF(-38, -78), QPointF(-13, -78),
                                  QPointF(-28, -46)});
    painter.drawPolygon(QPolygonF{QPointF(38, -78), QPointF(13, -78),
                                  QPointF(28, -46)});
    painter.restore();

    painter.save();
    painter.translate(boss);
    if (hitFlash) {
        painter.setOpacity(0.35 + std::abs(std::sin(attackProgress_ * 40.0)) * 0.45);
    }
    painter.setPen(QPen(QColor("#22283a"), 5));
    painter.setBrush(bossColor(bossIndex_));
    QPainterPath body;
    body.moveTo(-62, 55);
    body.cubicTo(-86, 5, -60, -73, 0, -86);
    body.cubicTo(61, -72, 88, 2, 61, 55);
    body.closeSubpath();
    painter.drawPath(body);
    painter.setBrush(QColor("#f5e6c8"));
    painter.drawEllipse(QPointF(-22, -24), 11, 14);
    painter.drawEllipse(QPointF(22, -24), 11, 14);
    painter.setBrush(QColor("#c52c46"));
    painter.drawEllipse(QPointF(-22, -22), 5, 8);
    painter.drawEllipse(QPointF(22, -22), 5, 8);
    painter.setBrush(QColor("#e7d7b5"));
    painter.drawPolygon(QPolygonF{QPointF(-55, -58), QPointF(-88, -92),
                                  QPointF(-30, -73)});
    painter.drawPolygon(QPolygonF{QPointF(55, -58), QPointF(88, -92),
                                  QPointF(30, -73)});
    painter.restore();

    if (!activeSkill_.name.isEmpty()) {
        const qreal effectProgress = std::clamp((attackProgress_ - 0.25) / 0.45, 0.0, 1.0);
        const QPointF effectPoint = hero + (boss - hero) * effectProgress;
        QColor effectColor = activeSkill_.damage >= 15 ? QColor("#ff7b3a")
                            : activeSkill_.cooldown > 0 ? QColor("#8a6cff")
                                                       : QColor("#f8df55");
        painter.setPen(QPen(effectColor, 7, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(hero + QPointF(30, -35), effectPoint);
        painter.setPen(QPen(QColor(255, 255, 255, 210), 3));
        painter.setBrush(QColor(effectColor.red(), effectColor.green(),
                                effectColor.blue(), 180));
        painter.drawEllipse(effectPoint, 15 + activeSkill_.damage * 0.7,
                            15 + activeSkill_.damage * 0.7);
    }

    if (hitFlash) {
    QFont damageFont(QStringLiteral("Microsoft YaHei"));
    damageFont.setPixelSize(34);
        damageFont.setBold(true);
        painter.setFont(damageFont);
        painter.setPen(QColor("#fff6d5"));
        painter.drawText(QRectF(boss.x() - 90, boss.y() - 145, 180, 50),
                         Qt::AlignCenter, QStringLiteral("-%1").arg(damage_));
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 225));
    painter.drawRoundedRect(QRectF(24, 20, 300, 82), 14, 14);
    painter.drawRoundedRect(QRectF(width() - 374, 20, 350, 82), 14, 14);
    painter.setPen(QColor("#203345"));
    QFont infoFont(QStringLiteral("Microsoft YaHei"));
    infoFont.setBold(true);
    infoFont.setPixelSize(18);
    painter.setFont(infoFont);
    painter.drawText(QRectF(42, 28, 260, 25), QStringLiteral("算法精灵  Lv. 50"));
    painter.drawText(QRectF(width() - 352, 28, 310, 25),
                     Qt::AlignLeft, QStringLiteral("迷宫守卫 %1/%2")
                                        .arg(bossIndex_ + 1)
                                        .arg(bossCount_));
    drawHealthBar(painter, QRectF(width() - 352, 62, 250, 20), health_, maximumHealth_,
                  health_ * 3 < maximumHealth_ ? QColor("#ef5350") : QColor("#62c96b"));
    painter.setPen(QColor("#203345"));
    painter.drawText(QRectF(width() - 96, 57, 62, 28), Qt::AlignCenter,
                     QStringLiteral("%1/%2").arg(std::max(0, health_)).arg(maximumHealth_));
    painter.setPen(QColor("#36566d"));
    painter.drawText(QRectF(42, 59, 260, 28),
                     QStringLiteral("回合 %1/%2   复活费 %3 金币")
                         .arg(turn_)
                         .arg(turnLimit_)
                         .arg(reviveCost_));
}

BattleWindow::BattleWindow(const QVector<int> &bossHealth,
                           const QVector<BossSkill> &skills,
                           const BossResult &optimalResult,
                           int turnLimit,
                           int reviveCoins,
                           DamageOverflowMode damageMode,
                           QWidget *parent)
    : QMainWindow(parent),
      initialHealth_(bossHealth),
      skills_(skills),
      optimalResult_(optimalResult),
      turnLimit_(turnLimit),
      reviveCoins_(reviveCoins),
      damageMode_(damageMode) {
    setAttribute(Qt::WA_DeleteOnClose);
    buildUi();
    resetBattle();
}

void BattleWindow::buildUi() {
    setWindowTitle(QStringLiteral("迷宫守卫挑战 - 最优技能序列可视化"));
    resize(980, 720);
    auto *central = new QWidget;
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);
    scene_ = new BattleScene;
    layout->addWidget(scene_, 1);

    auto *dialogBox = new QWidget;
    dialogBox->setObjectName(QStringLiteral("dialogBox"));
    auto *dialogLayout = new QVBoxLayout(dialogBox);
    roundLabel_ = new QLabel;
    modeLabel_ = new QLabel;
    messageLabel_ = new QLabel;
    messageLabel_->setWordWrap(true);
    dialogLayout->addWidget(roundLabel_);
    dialogLayout->addWidget(messageLabel_);
    dialogLayout->addWidget(modeLabel_);
    layout->addWidget(dialogBox);

    auto *skillGrid = new QGridLayout;
    for (int i = 0; i < skills_.size(); ++i) {
        auto *button = new QPushButton;
        button->setMinimumHeight(56);
        button->setProperty("skillButton", true);
        connect(button, &QPushButton::clicked, this, [this, i] { useSkill(i); });
        skillButtons_.append(button);
        skillGrid->addWidget(button, i / 2, i % 2);
    }
    layout->addLayout(skillGrid);

    auto *controls = new QHBoxLayout;
    autoButton_ = new QPushButton(QStringLiteral("自动播放最优序列"));
    auto *resetButton = new QPushButton(QStringLiteral("重新开始"));
    controls->addWidget(autoButton_);
    controls->addWidget(resetButton);
    layout->addLayout(controls);
    connect(autoButton_, &QPushButton::clicked, this, [this] {
        autoPlaying_ ? stopAutoPlay() : startAutoPlay();
    });
    connect(resetButton, &QPushButton::clicked, this, &BattleWindow::resetBattle);

    autoTimer_ = new QTimer(this);
    autoTimer_->setSingleShot(true);
    autoTimer_->setInterval(420);
    connect(autoTimer_, &QTimer::timeout, this, &BattleWindow::advanceAutoPlay);

    setCentralWidget(central);
    setStyleSheet(QStringLiteral(
        "QMainWindow{background:#183145;}"
        "*{font-family:'Microsoft YaHei','Microsoft JhengHei','SimHei';}"
        "#dialogBox{background:rgba(255,255,255,235); border:3px solid #4e6e86;"
        " border-radius:12px;}"
        "#dialogBox QLabel{color:#21384a; font-size:15px;}"
        "QPushButton{background:#318d9f; color:white; border:2px solid #b8eef3;"
        " border-radius:10px; padding:9px; font-weight:600;}"
        "QPushButton[skillButton=true]{text-align:left; background:#445b91;}"
        "QPushButton:hover{background:#287481;}"
        "QPushButton:disabled{background:#52606b; color:#aeb8bf; border-color:#6d7880;}"));
}

int BattleWindow::firstLivingBoss() const {
    for (int i = 0; i < currentHealth_.size(); ++i) {
        if (currentHealth_[i] > 0) {
            return i;
        }
    }
    return -1;
}

void BattleWindow::resetBattle() {
    stopAutoPlay();
    currentHealth_ = initialHealth_;
    cooldowns_.fill(0, skills_.size());
    currentBoss_ = 0;
    turn_ = 0;
    autoStep_ = 0;
    animating_ = false;
    QStringList sequence;
    for (int skillIndex : optimalResult_.skillSequence) {
        sequence.append(skills_[skillIndex].name);
    }
    messageLabel_->setText(
        QStringLiteral("迷宫守卫出现了！最优序列：%1")
            .arg(sequence.join(QStringLiteral(" → "))));
    refreshUi();
}

void BattleWindow::useSkill(int skillIndex) {
    if (animating_ || firstLivingBoss() < 0 || turn_ >= turnLimit_
        || skillIndex < 0 || skillIndex >= skills_.size() || cooldowns_[skillIndex] > 0) {
        return;
    }
    currentBoss_ = firstLivingBoss();
    int damage = std::min(skills_[skillIndex].damage, currentHealth_[currentBoss_]);
    if (damageMode_ == DamageOverflowMode::Overflow) {
        damage = 0;
        int remainingDamage = skills_[skillIndex].damage;
        for (int target = currentBoss_; target < currentHealth_.size() && remainingDamage > 0; ++target) {
            const int dealt = std::min(currentHealth_[target], remainingDamage);
            damage += dealt;
            remainingDamage -= dealt;
        }
    }
    animating_ = true;
    messageLabel_->setText(QStringLiteral("算法精灵使用了 %1！").arg(skills_[skillIndex].name));
    refreshUi();
    scene_->animateAttack(skills_[skillIndex], damage,
                          [this, skillIndex, damage] { finishTurn(skillIndex, damage); });
}

void BattleWindow::finishTurn(int skillIndex, int damage) {
    if (damageMode_ == DamageOverflowMode::Overflow) {
        int remainingDamage = damage;
        for (int target = currentBoss_; target < currentHealth_.size() && remainingDamage > 0; ++target) {
            const int dealt = std::min(currentHealth_[target], remainingDamage);
            currentHealth_[target] -= dealt;
            remainingDamage -= dealt;
        }
    } else {
        currentHealth_[currentBoss_] -= damage;
    }
    ++turn_;
    for (int &cooldown : cooldowns_) {
        cooldown = std::max(0, cooldown - 1);
    }
    cooldowns_[skillIndex] = skills_[skillIndex].cooldown;
    animating_ = false;

    const bool defeated = currentHealth_[currentBoss_] <= 0;
    const int nextBoss = firstLivingBoss();
    if (nextBoss < 0) {
        messageLabel_->setText(QStringLiteral("胜利！全部迷宫守卫已在 %1 回合内被击败。")
                                   .arg(turn_));
        stopAutoPlay();
    } else if (turn_ >= turnLimit_) {
        messageLabel_->setText(QStringLiteral("超过限定回合，挑战失败。复活需要 %1 金币。")
                                   .arg(reviveCoins_));
        stopAutoPlay();
    } else if (defeated) {
        currentBoss_ = nextBoss;
        messageLabel_->setText(QStringLiteral("守卫 %1 被击败，守卫 %2 登场！")
                                   .arg(currentBoss_)
                                   .arg(currentBoss_ + 1));
    } else {
        messageLabel_->setText(QStringLiteral("造成 %1 点伤害，守卫仍有 %2 点生命。")
                                   .arg(damage)
                                   .arg(currentHealth_[currentBoss_]));
    }
    refreshUi();
    if (autoPlaying_) {
        autoTimer_->start();
    }
}

void BattleWindow::startAutoPlay() {
    if (turn_ != 0 || firstLivingBoss() < 0) {
        resetBattle();
    }
    autoPlaying_ = true;
    autoStep_ = 0;
    autoButton_->setText(QStringLiteral("暂停自动播放"));
    modeLabel_->setText(QStringLiteral("模式：分支限界最优序列自动演示"));
    refreshUi();
    autoTimer_->start(200);
}

void BattleWindow::stopAutoPlay() {
    autoPlaying_ = false;
    if (autoTimer_) {
        autoTimer_->stop();
    }
    if (autoButton_) {
        autoButton_->setText(QStringLiteral("自动播放最优序列"));
    }
    if (modeLabel_) {
        modeLabel_->setText(QStringLiteral("模式：手动选择技能"));
    }
    refreshUi();
}

void BattleWindow::advanceAutoPlay() {
    if (!autoPlaying_ || animating_ || autoStep_ >= optimalResult_.skillSequence.size()) {
        if (autoStep_ >= optimalResult_.skillSequence.size()) {
            stopAutoPlay();
        }
        return;
    }
    useSkill(optimalResult_.skillSequence[autoStep_++]);
}

void BattleWindow::refreshUi() {
    currentBoss_ = std::max(0, firstLivingBoss());
    const bool finished = firstLivingBoss() < 0;
    const int health = finished ? 0 : currentHealth_[currentBoss_];
    const int maximum = initialHealth_.isEmpty() ? 1 : initialHealth_[currentBoss_];
    scene_->setBattleState(currentBoss_, initialHealth_.size(), health, maximum, turn_,
                           turnLimit_, reviveCoins_);
    roundLabel_->setText(QStringLiteral(
                             "最优回合：%1    当前回合：%2    限定回合：%3    搜索/剪枝：%4/%5")
                             .arg(optimalResult_.minimumTurns)
                             .arg(turn_)
                             .arg(turnLimit_)
                             .arg(optimalResult_.expandedStates)
                             .arg(optimalResult_.prunedStates));
    if (modeLabel_->text().isEmpty()) {
        modeLabel_->setText(QStringLiteral("模式：手动选择技能"));
    }
    for (int i = 0; i < skillButtons_.size(); ++i) {
        const QString cooldownText = cooldowns_.value(i) > 0
            ? QStringLiteral("冷却 %1").arg(cooldowns_[i])
            : QStringLiteral("可用");
        skillButtons_[i]->setText(QStringLiteral("%1    伤害 %2\n冷却 %3 回合 · %4")
                                      .arg(skills_[i].name)
                                      .arg(skills_[i].damage)
                                      .arg(skills_[i].cooldown)
                                      .arg(cooldownText));
        skillButtons_[i]->setEnabled(!animating_ && !autoPlaying_ && !finished
                                     && turn_ < turnLimit_ && cooldowns_.value(i) == 0);
    }
}
