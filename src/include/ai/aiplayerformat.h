#pragma once

#include "bosssolver.h"
#include "maze.h"

#include <QJsonObject>
#include <QByteArray>

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
