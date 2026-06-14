#include "aiplayerformat.h"

#include <QJsonArray>

namespace {

QByteArray numberArray(const QVector<int> &values) {
    QByteArray result("[");
    for (int i = 0; i < values.size(); ++i) {
        if (i > 0) {
            result += ',';
        }
        result += QByteArray::number(values[i]);
    }
    result += ']';
    return result;
}

} // namespace

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

QByteArray serializeAiPlayerInput(const MazeModel &maze,
                                  const QVector<int> &bossHealth,
                                  const QVector<BossSkill> &skills,
                                  int roundLimit,
                                  int coinConsumption) {
    QByteArray output("{\n  \"maze\": [\n");
    const QStringList grid = maze.expandedGrid();
    for (int rowIndex = 0; rowIndex < grid.size(); ++rowIndex) {
        output += "    [";
        const QString &line = grid[rowIndex];
        for (int column = 0; column < line.size(); ++column) {
            if (column > 0) {
                output += ',';
            }
            output += "\"";
            output += line[column].toLatin1();
            output += "\"";
        }
        output += rowIndex + 1 < grid.size() ? "],\n" : "]\n";
    }
    output += "  ],\n  \"B\":" + numberArray(bossHealth) + ",\n";
    output += "  \"PlayerSkills\":[";
    for (int i = 0; i < skills.size(); ++i) {
        if (i > 0) {
            output += ',';
        }
        output += '[' + QByteArray::number(skills[i].damage) + ','
            + QByteArray::number(skills[i].cooldown) + ']';
    }
    output += "],\n  \"minRouds\": " + QByteArray::number(roundLimit) + ",\n";
    output += "  \"CoinConsumption\": " + QByteArray::number(coinConsumption) + "\n}\n";
    return output;
}
