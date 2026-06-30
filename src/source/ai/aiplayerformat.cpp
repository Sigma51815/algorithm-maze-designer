// 文件职责：AI 测试系统输入格式构造。
// 将迷宫矩阵、路径、BOSS 血量和技能序列转换为约定的紧凑 JSON 字段。
#include "ai/aiplayerformat.h"

#include <QJsonArray>
#include <QRegularExpression>
#include <QStringList>

#include <cstdlib>
#include <utility>

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

QJsonArray compactMazeRowsJson(const MazeModel &maze) {
    QJsonArray rows;
    for (QString line : maze.compactGrid()) {
        line.replace(QLatin1Char(' '), QLatin1Char('.'));
        line.replace(QLatin1Char('G'), QLatin1Char('C'));
        rows.append(line);
    }
    return rows;
}

QJsonArray pointJson(int row, int column) {
    QJsonArray point;
    point.append(row);
    point.append(column);
    return point;
}

QJsonArray pathJson(const MazeModel &maze, const QVector<int> &walk) {
    QJsonArray path;
    if (maze.columns() <= 0 || walk.isEmpty()) {
        return path;
    }

    auto centerOf = [&](int cell) {
        return std::pair<int, int>{
            (cell / maze.columns()) * 2 + 1,
            (cell % maze.columns()) * 2 + 1};
    };

    auto previous = centerOf(walk.first());
    path.append(pointJson(previous.first, previous.second));
    for (int i = 1; i < walk.size(); ++i) {
        const auto current = centerOf(walk[i]);
        if (std::abs(current.first - previous.first)
                + std::abs(current.second - previous.second) == 2) {
            path.append(pointJson((current.first + previous.first) / 2,
                                  (current.second + previous.second) / 2));
        }
        path.append(pointJson(current.first, current.second));
        previous = current;
    }
    return path;
}

QJsonArray bossHealthJson(const QVector<int> &bossHealth) {
    QJsonArray array;
    for (int health : bossHealth) {
        array.append(health);
    }
    return array;
}

QJsonArray playerSkillsJson(const QVector<BossSkill> &skills) {
    QJsonArray array;
    for (const BossSkill &skill : skills) {
        QJsonArray item;
        item.append(skill.damage);
        item.append(skill.cooldown);
        array.append(item);
    }
    return array;
}

QJsonArray skillSequenceJson(const QVector<int> &skillSequence) {
    QJsonArray array;
    for (int skillIndex : skillSequence) {
        array.append(skillIndex);
    }
    return array;
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

QJsonObject buildMazeCheckInput(const MazeModel &maze) {
    QJsonObject root;
    root.insert(QStringLiteral("maze"), compactMazeRowsJson(maze));
    return root;
}

QJsonObject buildResourcePathCheckInput(const MazeModel &maze,
                                        const QVector<int> &walk) {
    QJsonObject root;
    root.insert(QStringLiteral("maze"), compactMazeRowsJson(maze));
    root.insert(QStringLiteral("path"), pathJson(maze, walk));
    return root;
}

QJsonObject buildBossBattleCheckInput(const QVector<int> &bossHealth,
                                      const QVector<BossSkill> &skills,
                                      const QVector<int> &skillSequence) {
    QJsonObject root;
    root.insert(QStringLiteral("B"), bossHealthJson(bossHealth));
    root.insert(QStringLiteral("PlayerSkills"), playerSkillsJson(skills));
    root.insert(QStringLiteral("SkillSequence"), skillSequenceJson(skillSequence));
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
    output += "  ],\n  \"B\": " + numberArray(bossHealth) + ",\n";
    output += "  \"PlayerSkills\": [";
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

QByteArray serializeMazeBenchmarkText(const MazeBenchmarkResult &benchmark) {
    QString text;
    text += QStringLiteral("BOSS战之前的资源值: %1\n")
                .arg(benchmark.bossBeforeResource);
    text += QStringLiteral("最终剩余资源价值: %1\n")
                .arg(benchmark.finalRemainingResource);
    text += QStringLiteral("步数: %1\n").arg(benchmark.steps);
    text += QStringLiteral("二者比值: %1\n")
                .arg(benchmark.valueStepRatio, 0, 'f', 3);
    return text.toUtf8();
}

QString sanitizedSubmissionLeaderName(const QString &leaderName) {
    QString result = leaderName.trimmed();
    static const QString invalid = QStringLiteral("\\/:*?\"<>|");
    for (const QChar ch : invalid) {
        result.replace(ch, QLatin1Char('_'));
    }
    while (result.contains(QStringLiteral("__"))) {
        result.replace(QStringLiteral("__"), QStringLiteral("_"));
    }
    return result.trimmed().replace(QRegularExpression(QStringLiteral("^_+|_+$")),
                                    QString());
}
