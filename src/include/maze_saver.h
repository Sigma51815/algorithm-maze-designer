#pragma once

#include "ai/rl_player.h"
#include "coevolution.h"
#include "maze.h"
#include "maze_optimizer.h"

#include <QJsonObject>
#include <QString>

struct SavedMazeInfo {
    QString name;
    QString source;           // "ga", "coevolution", "rl", "manual"
    MazeModel maze;
    int fitness = 0;
    int dpScore = 0;
    int greedyScore = 0;
    int rlScore = 0;
    QDateTime timestamp;
    QJsonObject metadata;
};

class MazeSaver {
public:
    static bool saveMaze(const QString &path, const MazeModel &maze,
                         const QString &name = QString(),
                         const QString &source = QString(),
                         int fitness = 0, int dpScore = 0,
                         int greedyScore = 0, int rlScore = 0);

    static bool loadMaze(const QString &path, SavedMazeInfo &info);

    static bool saveQTable(const QString &path, const double qTable[][4],
                           int stateSize, const RLConfig &config);

    static bool loadQTable(const QString &path, double qTable[][4],
                           int stateSize, RLConfig &config);

    static bool saveCoEvolResult(const QString &path,
                                 const CoEvolResult &result,
                                 const CoEvolConfig &config);

    static bool saveGAResult(const QString &path,
                             const MazeModel &maze,
                             const OptimizerConfig &config,
                             int fitness, int dpScore, int greedyScore);

    static QJsonObject mazeToJson(const MazeModel &maze,
                                  const QString &name,
                                  const QString &source,
                                  int fitness, int dpScore,
                                  int greedyScore, int rlScore);

    static bool loadMazeFromJson(const QJsonObject &json, SavedMazeInfo &info);
};
