#pragma once

#include "bosssolver.h"
#include "maze.h"

#include <QJsonObject>
#include <QByteArray>
#include <QString>

struct MazeBenchmarkResult {
    int bossBeforeResource = 0;
    int finalRemainingResource = 0;
    int steps = 0;
    double valueStepRatio = 0.0;
};

QJsonObject buildAiPlayerInput(const MazeModel &maze,
                               const QVector<int> &bossHealth,
                               const QVector<BossSkill> &skills,
                               int roundLimit,
                               int coinConsumption);

QJsonObject buildMazeCheckInput(const MazeModel &maze);

QJsonObject buildResourcePathCheckInput(const MazeModel &maze,
                                        const QVector<int> &walk);

QJsonObject buildBossBattleCheckInput(const QVector<int> &bossHealth,
                                      const QVector<BossSkill> &skills,
                                      const QVector<int> &skillSequence);

QByteArray serializeAiPlayerInput(const MazeModel &maze,
                                  const QVector<int> &bossHealth,
                                  const QVector<BossSkill> &skills,
                                  int roundLimit,
                                  int coinConsumption);

QByteArray serializeMazeBenchmarkText(const MazeBenchmarkResult &benchmark);

QString sanitizedSubmissionLeaderName(const QString &leaderName);
