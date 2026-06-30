// 文件职责：迷宫 JSON 保存和读取。
// 把 MazeModel、资源、BOSS 信息和 GA 结果序列化，供现场复制到测试系统或重新载入。
#include "maze_saver.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTextStream>

// 保存当前迷宫：把结构、资源、起终点、BOSS 和附加元数据写成 JSON。
bool MazeSaver::saveMaze(const QString &path, const MazeModel &maze,
                         const QString &name, const QString &source,
                         double fitness, int dpScore,
                         int greedyScore) {
    QJsonObject json = mazeToJson(maze, name, source, fitness,
                                  dpScore, greedyScore);
    QJsonDocument doc(json);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(doc.toJson(QJsonDocument::Indented));
    return file.commit();
}

// 读取已保存迷宫：解析 JSON 后恢复 MazeModel，便于现场复现同一个迷宫。
bool MazeSaver::loadMaze(const QString &path, SavedMazeInfo &info) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    return loadMazeFromJson(doc.object(), info);
}

bool MazeSaver::saveGAResult(const QString &path,
                             const MazeModel &maze,
                             const OptimizerConfig &config,
                             double fitness, int dpScore, int greedyScore) {
    QJsonObject json;
    json["format"] = QStringLiteral("ga-result-v1");
    json["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonObject cfgObj;
    cfgObj["rows"] = config.rows;
    cfgObj["columns"] = config.columns;
    cfgObj["populationSize"] = config.populationSize;
    cfgObj["generations"] = config.generations;
    cfgObj["mutationRate"] = config.mutationRate;
    cfgObj["tournamentSize"] = config.tournamentSize;
    cfgObj["coinCount"] = config.coinCount;
    cfgObj["trapCount"] = config.trapCount;
    cfgObj["seed"] = static_cast<qint64>(config.seed);
    json["config"] = cfgObj;

    json["bestMaze"] = mazeToJson(maze, "ga-best", "ga",
                                  fitness, dpScore, greedyScore);

    QJsonDocument doc(json);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(doc.toJson(QJsonDocument::Indented));
    return file.commit();
}

QJsonObject MazeSaver::mazeToJson(const MazeModel &maze,
                                  const QString &name,
                                  const QString &source,
                                  double fitness, int dpScore,
                                  int greedyScore) {
    QJsonObject json = maze.toJson();
    json["name"] = name;
    json["source"] = source;
    json["fitness"] = fitness;
    json["dpScore"] = dpScore;
    json["greedyScore"] = greedyScore;
    json["savedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    return json;
}

bool MazeSaver::loadMazeFromJson(const QJsonObject &json, SavedMazeInfo &info) {
    if (!json.contains("format") || !json["format"].toString().startsWith("algorithm-maze")) {
        return false;
    }
    info.name = json["name"].toString();
    info.source = json["source"].toString();
    info.fitness = json["fitness"].toDouble();
    info.dpScore = json["dpScore"].toInt();
    info.greedyScore = json["greedyScore"].toInt();
    info.timestamp = QDateTime::fromString(json["savedAt"].toString(), Qt::ISODate);
    info.metadata = json;

    int rows = json["rows"].toInt();
    int cols = json["columns"].toInt();
    if (rows <= 0 || cols <= 0) {
        return false;
    }

    QJsonArray edgeArray = json["passages"].toArray();
    QVector<MazeEdge> edges;
    for (const QJsonValue &val : edgeArray) {
        QJsonArray edge = val.toArray();
        if (edge.size() == 2) {
            edges.append({edge[0].toInt(), edge[1].toInt()});
        }
    }
    quint32 seed = 42;
    info.maze.setFromEdges(rows, cols, edges, seed);

    QJsonArray resourceArray = json["resources"].toArray();
    QVector<int> resources(info.maze.cellCount(), 0);
    for (const QJsonValue &val : resourceArray) {
        QJsonObject res = val.toObject();
        int cell = res["cell"].toInt();
        int value = res["value"].toInt();
        if (cell >= 0 && cell < info.maze.cellCount()) {
            resources[cell] = value;
        }
    }
    info.maze.setResources(resources);

    const int startCell = json.contains("startCell")
        ? json["startCell"].toInt(-1)
        : info.maze.startCell();
    const int endCell = json.contains("endCell")
        ? json["endCell"].toInt(-1)
        : info.maze.endCell();
    const int bossCell = json["bossAtCell"].toInt(-1);
    const bool hasBoss = json.contains("hasBoss")
        ? json["hasBoss"].toBool(false)
        : bossCell >= 0;
    if (!info.maze.setSpecialCells(startCell, endCell, bossCell, hasBoss)) {
        return false;
    }

    return info.maze.validatePerfect();
}
