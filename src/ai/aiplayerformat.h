#pragma once

#include "include/bosssolver.h"
#include "include/maze.h"

#include <QJsonObject>
#include <QByteArray>

QJsonObject buildAiPlayerInput(const MazeModel &maze,
                               const QVector<int> &bossHealth,
                               const QVector<BossSkill> &skills,
                               int roundLimit,
                               int coinConsumption);

QByteArray serializeAiPlayerInput(const MazeModel &maze,
                                  const QVector<int> &bossHealth,
                                  const QVector<BossSkill> &skills,
                                  int roundLimit,
                                  int coinConsumption);
