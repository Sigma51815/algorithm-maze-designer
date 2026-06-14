#pragma once

#include <QString>
#include <QVector>

struct BossSkill {
    QString name;
    int damage = 0;
    int cooldown = 0;
};

struct BossResult {
    bool solved = false;
    int minimumTurns = 0;
    QVector<int> skillSequence;
    qint64 expandedStates = 0;
    qint64 prunedStates = 0;
};

class BossSolver {
public:
    static BossResult solve(const QVector<int> &bossHealth, const QVector<BossSkill> &skills);
    static bool verify(const QVector<int> &bossHealth,
                       const QVector<BossSkill> &skills,
                       const QVector<int> &sequence);
};
