#include "aiplayerformat.h"

#include <QJsonArray>

QJsonObject buildAiPlayerInput(const MazeModel &maze,
                               const QVector<int> &bossHealth,
                               const QVector<BossSkill> &skills,
                               int roundLimit,
                               int coinConsumption) {
    QJsonObject root;

    QJsonArray matrix;
    for (const QString &line : maze.expandedGrid()) {
        QJsonArray row;
        for (const QChar character : line) {
            row.append(QString(character));
        }
        matrix.append(row);
    }
    root.insert(QStringLiteral("maze"), matrix);

    QJsonArray bosses;
    for (int health : bossHealth) {
        bosses.append(health);
    }
    root.insert(QStringLiteral("B"), bosses);

    QJsonArray playerSkills;
    for (const BossSkill &skill : skills) {
        QJsonArray item;
        item.append(skill.damage);
        item.append(skill.cooldown);
        playerSkills.append(item);
    }
    root.insert(QStringLiteral("PlayerSkills"), playerSkills);
    root.insert(QStringLiteral("minRouds"), roundLimit);
    root.insert(QStringLiteral("CoinConsumption"), coinConsumption);
    return root;
}
