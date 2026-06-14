#include "ai/rl_player.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQueue>
#include <QSaveFile>
#include <QSet>

#include <algorithm>
#include <limits>

RLPlayer::RLPlayer() {
    for (auto &row : qTable_) {
        for (auto &val : row) {
            val = 0.0;
        }
    }
}

void RLPlayer::resetQTable() {
    for (auto &row : qTable_) {
        for (auto &val : row) {
            val = 0.0;
        }
    }
}

bool RLPlayer::saveQTable(const QString &path, const RLConfig &config) const {
    QJsonObject json;
    json["format"] = QStringLiteral("rl-qtable-v1");
    json["stateSize"] = StateSize;
    json["actionCount"] = ActionCount;
    json["trainEpisodes"] = config.trainEpisodes;
    json["alpha"] = config.alpha;
    json["gamma"] = config.gamma;
    json["epsilonStart"] = config.epsilonStart;
    json["epsilonEnd"] = config.epsilonEnd;
    json["epsilonDecay"] = config.epsilonDecay;
    json["coinCount"] = config.coinCount;
    json["trapCount"] = config.trapCount;
    json["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonArray tableArray;
    for (int i = 0; i < StateSize; ++i) {
        for (int j = 0; j < ActionCount; ++j) {
            tableArray.append(qTable_[i][j]);
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

bool RLPlayer::loadQTable(const QString &path, RLConfig &config) {
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
    if (json["stateSize"].toInt() != StateSize) {
        return false;
    }

    config.trainEpisodes = json["trainEpisodes"].toInt();
    config.alpha = json["alpha"].toDouble();
    config.gamma = json["gamma"].toDouble();
    config.epsilonStart = json["epsilonStart"].toDouble();
    config.epsilonEnd = json["epsilonEnd"].toDouble();
    config.epsilonDecay = json["epsilonDecay"].toDouble();
    config.coinCount = json["coinCount"].toInt(config.coinCount);
    config.trapCount = json["trapCount"].toInt(config.trapCount);

    QJsonArray tableArray = json["qTable"].toArray();
    int idx = 0;
    for (int i = 0; i < StateSize && idx < tableArray.size(); ++i) {
        for (int j = 0; j < ActionCount && idx < tableArray.size(); ++j) {
            qTable_[i][j] = tableArray[idx++].toDouble();
        }
    }
    return true;
}

int RLPlayer::encodeState(const MazeModel &maze, int cell, const QSet<int> &visited) const {
    const int rows = maze.rows();
    const int cols = maze.columns();
    const int cr = cell / cols;
    const int cc = cell % cols;

    int viewPattern = 0;
    int viewPow = 1;
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            if (dr == 0 && dc == 0) continue;
            const int r = cr + dr;
            const int c = cc + dc;
            int cellState = 0;
            if (r >= 0 && r < rows && c >= 0 && c < cols) {
                const int neighbor = r * cols + c;
                const int value = maze.resourceAt(neighbor);
                if (value != 0 && !visited.contains(neighbor)) {
                    cellState = 2;
                } else {
                    cellState = 1;
                }
            }
            viewPattern += cellState * viewPow;
            viewPow *= 3;
        }
    }

    int passable = 0;
    const int dr[] = {-1, 1, 0, 0};
    const int dc[] = {0, 0, -1, 1};
    for (int d = 0; d < 4; ++d) {
        const int nr = cr + dr[d];
        const int nc = cc + dc[d];
        if (nr >= 0 && nr < rows && nc >= 0 && nc < cols) {
            if (maze.isOpen(cell, nr * cols + nc)) {
                passable |= (1 << d);
            }
        }
    }

    return viewPattern * PassableBits + passable;
}

int RLPlayer::chooseAction(int state, double epsilon, std::mt19937 &rng) const {
    if (std::uniform_real_distribution<double>(0.0, 1.0)(rng) < epsilon) {
        return std::uniform_int_distribution<int>(0, 3)(rng);
    }
    double maxVal = qTable_[state][0];
    QVector<int> best = {0};
    for (int i = 1; i < 4; ++i) {
        if (qTable_[state][i] > maxVal + 1e-9) {
            maxVal = qTable_[state][i];
            best = {i};
        } else if (std::abs(qTable_[state][i] - maxVal) < 1e-9) {
            best.append(i);
        }
    }
    std::uniform_int_distribution<int> dist(0, best.size() - 1);
    return best[dist(rng)];
}

void RLPlayer::trainOnMaze(const MazeModel &maze, const RLConfig &config) {
    const int dr[] = {-1, 1, 0, 0};
    const int dc[] = {0, 0, -1, 1};
    const int rows = maze.rows();
    const int cols = maze.columns();
    const int endR = maze.endCell() / cols;
    const int endC = maze.endCell() % cols;

    auto manhattan = [&](int cell) -> int {
        return std::abs(cell / cols - endR) + std::abs(cell % cols - endC);
    };

    std::mt19937 rng(std::random_device{}());
    double epsilon = config.epsilonStart;

    for (int ep = 0; ep < config.trainEpisodes; ++ep) {
        int current = maze.startCell();
        QSet<int> visited;
        visited.insert(current);
        int prevDist = manhattan(current);

        for (int step = 0; step < config.playMaxSteps; ++step) {
            if (current == maze.endCell()) {
                break;
            }

            const int state = encodeState(maze, current, visited);
            const int action = chooseAction(state, epsilon, rng);

            const int cr = current / cols;
            const int cc = current % cols;
            const int nr = cr + dr[action];
            const int nc = cc + dc[action];

            if (nr < 0 || nr >= rows || nc < 0 || nc >= cols
                || !maze.isOpen(current, nr * cols + nc)) {
                const double maxNext = *std::max_element(
                    qTable_[state], qTable_[state] + ActionCount);
                qTable_[state][action] += config.alpha
                    * (-0.02 + config.gamma * maxNext - qTable_[state][action]);
                continue;
            }

            const int next = nr * cols + nc;
            current = next;
            const int newDist = manhattan(current);

            double reward = -0.002;

            if (newDist < prevDist) {
                reward += 0.01;
            }
            prevDist = newDist;

            if (!visited.contains(current)) {
                visited.insert(current);
                const int value = maze.resourceAt(current);
                if (current != maze.startCell() && current != maze.endCell()) {
                    if (value > 0) {
                        reward += 2.0;
                    } else if (value < 0) {
                        reward -= 1.2;
                    }
                }
            }

            if (current == maze.endCell()) {
                reward += 5.0;
            }

            const int nextState = encodeState(maze, current, visited);
            const double maxNext = *std::max_element(
                qTable_[nextState], qTable_[nextState] + ActionCount);
            qTable_[state][action] += config.alpha
                * (reward + config.gamma * maxNext - qTable_[state][action]);
        }

        epsilon *= config.epsilonDecay;
        epsilon = std::max(epsilon, config.epsilonEnd);
    }
}

void RLPlayer::trainOnMazes(const QVector<MazeModel> &mazes, const RLConfig &config) {
    for (const auto &maze : mazes) {
        trainOnMaze(maze, config);
    }
}

RLPlayResult RLPlayer::play(const MazeModel &maze, const RLConfig &config) const {
    RLPlayResult result;
    const int dr[] = {-1, 1, 0, 0};
    const int dc[] = {0, 0, -1, 1};
    const int rows = maze.rows();
    const int cols = maze.columns();

    std::mt19937 rng(std::random_device{}());
    int current = maze.startCell();
    QSet<int> visited;
    visited.insert(current);
    result.path.append(current);

    for (int step = 0; step < config.playMaxSteps; ++step) {
        if (current == maze.endCell()) {
            break;
        }

        const int state = encodeState(maze, current, visited);
        const int action = chooseAction(state, 0.05, rng);

        const int cr = current / cols;
        const int cc = current % cols;
        const int nr = cr + dr[action];
        const int nc = cc + dc[action];

        if (nr < 0 || nr >= rows || nc < 0 || nc >= cols) {
            continue;
        }
        const int next = nr * cols + nc;
        if (!maze.isOpen(current, next)) {
            continue;
        }

        current = next;
        if (!visited.contains(current)) {
            visited.insert(current);
            const int value = maze.resourceAt(current);
            if (current != maze.startCell() && current != maze.endCell()) {
                result.totalResource += value;
            }
        }
        result.path.append(current);
        ++result.steps;
    }

    return result;
}

QVector<MazeModel> RLPlayer::generateDiverseMazes(int count, quint32 baseSeed) {
    QVector<MazeModel> mazes;
    const MazeAlgorithm algos[] = {
        MazeAlgorithm::KruskalMst, MazeAlgorithm::DepthFirstSearch,
        MazeAlgorithm::BreadthFirstSearch, MazeAlgorithm::DivideAndConquer};
    for (int i = 0; i < count; ++i) {
        MazeModel m;
        m.generate(15, 15, algos[i % 4], baseSeed + i * 137);
        m.placeResources(30, 20, baseSeed + i * 251 + 10000);
        mazes.append(m);
    }
    return mazes;
}

QVector<MazeModel> RLPlayer::generateEasyMazes(int count, quint32 baseSeed) {
    QVector<MazeModel> mazes;
    for (int i = 0; i < count; ++i) {
        MazeModel m;
        m.generate(15, 15, MazeAlgorithm::KruskalMst, baseSeed + i * 137);
        m.placeResources(30, 10, baseSeed + i * 251 + 10000);
        mazes.append(m);
    }
    return mazes;
}

QVector<MazeModel> RLPlayer::generateHardMazes(int count, quint32 baseSeed) {
    QVector<MazeModel> mazes;
    const MazeAlgorithm algos[] = {
        MazeAlgorithm::DepthFirstSearch, MazeAlgorithm::BreadthFirstSearch};
    for (int i = 0; i < count; ++i) {
        MazeModel m;
        m.generate(15, 15, algos[i % 2], baseSeed + i * 137);
        m.placeResources(20, 30, baseSeed + i * 251 + 10000);
        mazes.append(m);
    }
    return mazes;
}
