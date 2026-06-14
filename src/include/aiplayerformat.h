#pragma once

#include "bosssolver.h"
#include "maze.h"

#include <QJsonObject>

QJsonObject buildAiPlayerInput(const MazeModel &maze,
                               const QVector<int> &bossHealth,
                               const QVector<BossSkill> &skills,
                               int roundLimit,
                               int coinConsumption);
