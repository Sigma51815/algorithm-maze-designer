#include "maze_saver.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTextStream>

bool MazeSaver::saveMaze(const QString &path, const MazeModel &maze,
                         const QString &name, const QString &source,
                         int fitness, int dpScore,
                         int greedyScore, int rlScore) {
    QJsonObject json = mazeToJson(maze, name, source, fitness,
                                  dpScore, greedyScore, rlScore);
    QJsonDocument doc(json);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(doc.toJson(QJsonDocument::Indented));
    return file.commit();
}

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

bool MazeSaver::saveQTable(const QString &path, const double qTable[][4],
                           int stateSize, const RLConfig &config) {
    QJsonObject json;
    json["format"] = QStringLiteral("rl-qtable-v1");
    json["stateSize"] = stateSize;
    json["actionCount"] = 4;
    json["trainEpisodes"] = config.trainEpisodes;
    json["alpha"] = config.alpha;
    json["gamma"] = config.gamma;
    json["epsilonStart"] = config.epsilonStart;
    json["epsilonEnd"] = config.epsilonEnd;
    json["epsilonDecay"] = config.epsilonDecay;
    json["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonArray tableArray;
    for (int i = 0; i < stateSize; ++i) {
        for (int j = 0; j < 4; ++j) {
            tableArray.append(qTable[i][j]);
        }
    }
    json["qTable"] = tableArray;

    QJsonDocument doc(json);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(doc.toJson(QJsonDocument::Compact));
    return file.commit();
}

bool MazeSaver::loadQTable(const QString &path, double qTable[][4],
                           int stateSize, RLConfig &config) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    QJsonObject json = doc.object();
    if (json["format"].toString() != QStringLiteral("rl-qtable-v1")) {
        return false;
    }
    if (json["stateSize"].toInt() != stateSize) {
        return false;
    }

    config.trainEpisodes = json["trainEpisodes"].toInt();
    config.alpha = json["alpha"].toDouble();
    config.gamma = json["gamma"].toDouble();
    config.epsilonStart = json["epsilonStart"].toDouble();
    config.epsilonEnd = json["epsilonEnd"].toDouble();
    config.epsilonDecay = json["epsilonDecay"].toDouble();

    QJsonArray tableArray = json["qTable"].toArray();
    int idx = 0;
    for (int i = 0; i < stateSize; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (idx < tableArray.size()) {
                qTable[i][j] = tableArray[idx++].toDouble();
            }
        }
    }
    return true;
}

bool MazeSaver::saveCoEvolResult(const QString &path,
                                 const CoEvolResult &result,
                                 const CoEvolConfig &config) {
    QJsonObject json;
    json["format"] = QStringLiteral("coevolution-result-v1");
    json["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonObject cfgObj;
    cfgObj["cycles"] = config.cycles;
    cfgObj["gaGenerations"] = config.gaGenerations;
    cfgObj["gaPopulation"] = config.gaPopulation;
    cfgObj["gaMutationRate"] = config.gaMutationRate;
    cfgObj["rlTrainEpisodes"] = config.rlTrainEpisodes;
    cfgObj["rlTopK"] = config.rlTopK;
    cfgObj["mazeRows"] = config.mazeRows;
    cfgObj["mazeCols"] = config.mazeCols;
    cfgObj["baseSeed"] = static_cast<qint64>(config.baseSeed);
    cfgObj["coinCount"] = config.coinCount;
    cfgObj["trapCount"] = config.trapCount;
    json["config"] = cfgObj;

    QJsonObject resultObj;
    resultObj["bestFitness"] = result.bestFitness;
    QJsonArray fitnessArray;
    for (int f : result.cycleBestFitness) {
        fitnessArray.append(f);
    }
    resultObj["cycleBestFitness"] = fitnessArray;
    QJsonArray rlArray;
    for (int s : result.rlScores) {
        rlArray.append(s);
    }
    resultObj["rlScores"] = rlArray;
    json["result"] = resultObj;

    json["bestMaze"] = mazeToJson(result.bestMaze, "coevolution-best",
                                  "coevolution", result.bestFitness, 0, 0,
                                  result.rlScores.isEmpty() ? 0 : result.rlScores.last());

    QJsonDocument doc(json);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(doc.toJson(QJsonDocument::Indented));
    return file.commit();
}

bool MazeSaver::saveGAResult(const QString &path,
                             const MazeModel &maze,
                             const OptimizerConfig &config,
                             int fitness, int dpScore, int greedyScore) {
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
    cfgObj["enableRL"] = config.enableRL;
    cfgObj["rlEpisodes"] = config.rlEpisodes;
    cfgObj["rlTopK"] = config.rlTopK;
    json["config"] = cfgObj;

    json["bestMaze"] = mazeToJson(maze, "ga-best", "ga",
                                  fitness, dpScore, greedyScore, 0);

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
                                  int fitness, int dpScore,
                                  int greedyScore, int rlScore) {
    QJsonObject json = maze.toJson();
    json["name"] = name;
    json["source"] = source;
    json["fitness"] = fitness;
    json["dpScore"] = dpScore;
    json["greedyScore"] = greedyScore;
    json["rlScore"] = rlScore;
    json["savedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    return json;
}

bool MazeSaver::loadMazeFromJson(const QJsonObject &json, SavedMazeInfo &info) {
    if (!json.contains("format") || !json["format"].toString().startsWith("algorithm-maze")) {
        return false;
    }
    info.name = json["name"].toString();
    info.source = json["source"].toString();
    info.fitness = json["fitness"].toInt();
    info.dpScore = json["dpScore"].toInt();
    info.greedyScore = json["greedyScore"].toInt();
    info.rlScore = json["rlScore"].toInt();
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

    return true;
}
