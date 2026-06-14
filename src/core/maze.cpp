#include "core/maze.h"

#include <QJsonArray>
#include <QQueue>
#include <QSet>

#include <algorithm>
#include <functional>
#include <numeric>

namespace {

class DisjointSet {
public:
    explicit DisjointSet(int size) : parent_(size), rank_(size, 0) {
        std::iota(parent_.begin(), parent_.end(), 0);
    }

    int find(int value) {
        if (parent_[value] != value) {
            parent_[value] = find(parent_[value]);
        }
        return parent_[value];
    }

    bool unite(int first, int second) {
        first = find(first);
        second = find(second);
        if (first == second) {
            return false;
        }
        if (rank_[first] < rank_[second]) {
            std::swap(first, second);
        }
        parent_[second] = first;
        if (rank_[first] == rank_[second]) {
            ++rank_[first];
        }
        return true;
    }

private:
    QVector<int> parent_;
    QVector<int> rank_;
};

quint64 edgeKey(int first, int second) {
    if (first > second) {
        std::swap(first, second);
    }
    return (static_cast<quint64>(static_cast<quint32>(first)) << 32)
        | static_cast<quint32>(second);
}

} // namespace

void MazeModel::reset(int rows, int columns, quint32 seed) {
    rows_ = std::max(2, rows);
    columns_ = std::max(2, columns);
    bossCell_ = -1;
    hasBoss_ = false;
    passages_.fill({false, false, false, false}, cellCount());
    resources_.fill(0, cellCount());
    generationSteps_.clear();
    random_.seed(seed);
}

void MazeModel::generate(int rows, int columns, MazeAlgorithm algorithm, quint32 seed) {
    reset(rows, columns, seed);
    switch (algorithm) {
    case MazeAlgorithm::DivideAndConquer:
        generateDivideAndConquer(0, rows_ - 1, 0, columns_ - 1);
        break;
    case MazeAlgorithm::KruskalMst:
        generateKruskal();
        break;
    case MazeAlgorithm::DepthFirstSearch:
        generateDepthFirst();
        break;
    case MazeAlgorithm::BreadthFirstSearch:
        generateBreadthFirst();
        break;
    }

    QVector<int> parent(cellCount(), -1);
    QQueue<int> q;
    q.enqueue(startCell());
    parent[startCell()] = startCell();
    while (!q.isEmpty() && parent[endCell()] < 0) {
        int cur = q.dequeue();
        for (int next : neighbors(cur)) {
            if (parent[next] < 0) {
                parent[next] = cur;
                q.enqueue(next);
            }
        }
    }
    for (int cell = endCell(); parent[cell] != startCell(); cell = parent[cell]) {
        bossCell_ = cell;
    }
    hasBoss_ = true;
}

QVector<int> MazeModel::gridNeighbors(int cell) const {
    QVector<int> result;
    const int row = rowOf(cell);
    const int column = columnOf(cell);
    if (row > 0) {
        result.append(index(row - 1, column));
    }
    if (column + 1 < columns_) {
        result.append(index(row, column + 1));
    }
    if (row + 1 < rows_) {
        result.append(index(row + 1, column));
    }
    if (column > 0) {
        result.append(index(row, column - 1));
    }
    return result;
}

void MazeModel::carve(int first, int second) {
    const int rowDelta = rowOf(second) - rowOf(first);
    const int columnDelta = columnOf(second) - columnOf(first);
    int direction = -1;
    if (rowDelta == -1 && columnDelta == 0) {
        direction = Up;
    } else if (rowDelta == 0 && columnDelta == 1) {
        direction = Right;
    } else if (rowDelta == 1 && columnDelta == 0) {
        direction = Down;
    } else if (rowDelta == 0 && columnDelta == -1) {
        direction = Left;
    }
    if (direction < 0) {
        return;
    }
    passages_[first][direction] = true;
    passages_[second][(direction + 2) % 4] = true;
    generationSteps_.append({first, second});
}

void MazeModel::generateDivideAndConquer(int top, int bottom, int left, int right) {
    if (top == bottom && left == right) {
        return;
    }

    const int height = bottom - top + 1;
    const int width = right - left + 1;
    if (width >= height && width > 1) {
        std::uniform_int_distribution<int> splitDistribution(left, right - 1);
        const int split = splitDistribution(random_);
        generateDivideAndConquer(top, bottom, left, split);
        generateDivideAndConquer(top, bottom, split + 1, right);
        std::uniform_int_distribution<int> openingDistribution(top, bottom);
        const int openingRow = openingDistribution(random_);
        carve(index(openingRow, split), index(openingRow, split + 1));
    } else {
        std::uniform_int_distribution<int> splitDistribution(top, bottom - 1);
        const int split = splitDistribution(random_);
        generateDivideAndConquer(top, split, left, right);
        generateDivideAndConquer(split + 1, bottom, left, right);
        std::uniform_int_distribution<int> openingDistribution(left, right);
        const int openingColumn = openingDistribution(random_);
        carve(index(split, openingColumn), index(split + 1, openingColumn));
    }
}

void MazeModel::generateKruskal() {
    QVector<MazeEdge> candidates;
    for (int row = 0; row < rows_; ++row) {
        for (int column = 0; column < columns_; ++column) {
            const int cell = index(row, column);
            if (column + 1 < columns_) {
                candidates.append({cell, index(row, column + 1)});
            }
            if (row + 1 < rows_) {
                candidates.append({cell, index(row + 1, column)});
            }
        }
    }
    std::shuffle(candidates.begin(), candidates.end(), random_);

    DisjointSet sets(cellCount());
    for (const MazeEdge &edge : candidates) {
        if (sets.unite(edge.from, edge.to)) {
            carve(edge.from, edge.to);
        }
    }
}

void MazeModel::generateDepthFirst() {
    QVector<bool> visited(cellCount(), false);
    QVector<int> stack{startCell()};
    visited[startCell()] = true;

    while (!stack.isEmpty()) {
        const int current = stack.last();
        QVector<int> choices;
        for (int next : gridNeighbors(current)) {
            if (!visited[next]) {
                choices.append(next);
            }
        }
        if (choices.isEmpty()) {
            stack.removeLast();
            continue;
        }
        std::shuffle(choices.begin(), choices.end(), random_);
        const int next = choices.first();
        visited[next] = true;
        carve(current, next);
        stack.append(next);
    }
}

void MazeModel::generateBreadthFirst() {
    QVector<bool> visited(cellCount(), false);
    QQueue<int> queue;
    queue.enqueue(startCell());
    visited[startCell()] = true;

    while (!queue.isEmpty()) {
        const int current = queue.dequeue();
        QVector<int> choices = gridNeighbors(current);
        std::shuffle(choices.begin(), choices.end(), random_);
        for (int next : choices) {
            if (visited[next]) {
                continue;
            }
            visited[next] = true;
            carve(current, next);
            queue.enqueue(next);
        }
    }
}

int MazeModel::resourceAt(int cell) const {
    return cell >= 0 && cell < resources_.size() ? resources_[cell] : 0;
}

bool MazeModel::isOpen(int first, int second) const {
    if (first < 0 || second < 0 || first >= cellCount() || second >= cellCount()) {
        return false;
    }
    const int rowDelta = rowOf(second) - rowOf(first);
    const int columnDelta = columnOf(second) - columnOf(first);
    if (rowDelta == -1 && columnDelta == 0) {
        return passages_[first][Up];
    }
    if (rowDelta == 0 && columnDelta == 1) {
        return passages_[first][Right];
    }
    if (rowDelta == 1 && columnDelta == 0) {
        return passages_[first][Down];
    }
    if (rowDelta == 0 && columnDelta == -1) {
        return passages_[first][Left];
    }
    return false;
}

QVector<int> MazeModel::neighbors(int cell) const {
    QVector<int> result;
    for (int candidate : gridNeighbors(cell)) {
        if (isOpen(cell, candidate)) {
            result.append(candidate);
        }
    }
    return result;
}

void MazeModel::placeResources(int coinCount, int trapCount, quint32 seed) {
    if (cellCount() <= 2) {
        return;
    }
    resources_.fill(0, cellCount());
    QVector<int> available;
    for (int cell = 1; cell < endCell(); ++cell) {
        if (cell == bossCell_) {
            continue;
        }
        available.append(cell);
    }
    std::mt19937 engine(seed);
    std::shuffle(available.begin(), available.end(), engine);
    const int availableCount = static_cast<int>(available.size());
    coinCount = std::clamp(coinCount, 0, availableCount);
    trapCount = std::clamp(trapCount, 0, availableCount - coinCount);
    for (int i = 0; i < coinCount; ++i) {
        resources_[available[i]] = 50;
    }
    for (int i = coinCount; i < coinCount + trapCount; ++i) {
        resources_[available[i]] = -30;
    }
}

void MazeModel::setBossCell(int cell) {
    if (cell >= 0 && cell < cellCount() && cell != startCell() && cell != endCell()) {
        bossCell_ = cell;
        hasBoss_ = true;
    }
}

void MazeModel::consumeResource(int cell) {
    if (cell >= 0 && cell < cellCount()) {
        resources_[cell] = 0;
    }
}

bool MazeModel::validatePerfect(QString *reason) const {
    if (cellCount() == 0) {
        if (reason) {
            *reason = QStringLiteral("迷宫为空");
        }
        return false;
    }

    int edgeCount = 0;
    for (int cell = 0; cell < cellCount(); ++cell) {
        edgeCount += neighbors(cell).size();
    }
    edgeCount /= 2;

    QVector<bool> visited(cellCount(), false);
    QQueue<int> queue;
    queue.enqueue(startCell());
    visited[startCell()] = true;
    int reached = 0;
    while (!queue.isEmpty()) {
        const int current = queue.dequeue();
        ++reached;
        for (int next : neighbors(current)) {
            if (!visited[next]) {
                visited[next] = true;
                queue.enqueue(next);
            }
        }
    }

    const bool valid = reached == cellCount() && edgeCount == cellCount() - 1;
    if (reason) {
        *reason = valid
            ? QStringLiteral("合法完美迷宫：V=%1，E=%2，连通且 E=V-1")
                  .arg(cellCount())
                  .arg(edgeCount)
            : QStringLiteral("校验失败：到达 %1/%2 个格子，E=%3")
                  .arg(reached)
                  .arg(cellCount())
                  .arg(edgeCount);
    }
    return valid;
}

ResourcePlan MazeModel::optimalResourceWalk() const {
    ResourcePlan result;
    if (cellCount() == 0) {
        return result;
    }

    QVector<int> parent(cellCount(), -1);
    QQueue<int> queue;
    queue.enqueue(startCell());
    parent[startCell()] = startCell();
    while (!queue.isEmpty() && parent[endCell()] < 0) {
        const int current = queue.dequeue();
        for (int next : neighbors(current)) {
            if (parent[next] < 0) {
                parent[next] = current;
                queue.enqueue(next);
            }
        }
    }

    QVector<int> backbone;
    for (int cell = endCell();; cell = parent[cell]) {
        backbone.prepend(cell);
        if (cell == startCell()) {
            break;
        }
    }
    QVector<bool> onBackbone(cellCount(), false);
    for (int cell : backbone) {
        onBackbone[cell] = true;
    }

    QVector<int> gain(cellCount(), 0);
    std::function<int(int, int)> calculateGain = [&](int cell, int from) {
        int value = resourceAt(cell);
        for (int next : neighbors(cell)) {
            if (next == from || onBackbone[next]) {
                continue;
            }
            value += std::max(0, calculateGain(next, cell));
        }
        gain[cell] = value;
        return value;
    };

    for (int cell : backbone) {
        for (int next : neighbors(cell)) {
            if (!onBackbone[next]) {
                calculateGain(next, cell);
            }
        }
    }

    result.walk.append(startCell());
    std::function<void(int, int)> appendExcursion = [&](int cell, int from) {
        result.walk.append(cell);
        for (int next : neighbors(cell)) {
            if (next == from || onBackbone[next] || gain[next] <= 0) {
                continue;
            }
            appendExcursion(next, cell);
        }
        result.walk.append(from);
    };

    for (int i = 0; i + 1 < backbone.size(); ++i) {
        const int current = backbone[i];
        for (int next : neighbors(current)) {
            if (!onBackbone[next] && gain[next] > 0) {
                appendExcursion(next, current);
            }
        }
        result.walk.append(backbone[i + 1]);
    }

    QSet<int> collected;
    for (int cell : result.walk) {
        collected.insert(cell);
    }
    result.collectedCells = collected.values();
    std::sort(result.collectedCells.begin(), result.collectedCells.end());
    for (int cell : result.collectedCells) {
        if (cell != startCell() && cell != endCell()) {
            result.maxValue += resourceAt(cell);
        }
    }
    return result;
}

QStringList MazeModel::expandedGrid() const {
    const int outputRows = rows_ * 2 + 1;
    const int outputColumns = columns_ * 2 + 1;
    QStringList grid;
    for (int row = 0; row < outputRows; ++row) {
        grid.append(QString(outputColumns, QLatin1Char('#')));
    }

    for (int cell = 0; cell < cellCount(); ++cell) {
        const int gridRow = rowOf(cell) * 2 + 1;
        const int gridColumn = columnOf(cell) * 2 + 1;
        QChar marker = QLatin1Char('.');
        if (cell == startCell()) {
            marker = QLatin1Char('S');
        } else if (hasBoss_ && cell == bossCell_) {
            marker = QLatin1Char('B');
        } else if (cell == endCell()) {
            marker = QLatin1Char('E');
        } else if (resourceAt(cell) > 0) {
            marker = QLatin1Char('G');
        } else if (resourceAt(cell) < 0) {
            marker = QLatin1Char('T');
        }
        grid[gridRow][gridColumn] = marker;
        for (int next : neighbors(cell)) {
            const int nextGridRow = rowOf(next) * 2 + 1;
            const int nextGridColumn = columnOf(next) * 2 + 1;
            grid[(gridRow + nextGridRow) / 2][(gridColumn + nextGridColumn) / 2]
                = QLatin1Char('.');
        }
    }
    return grid;
}

QJsonObject MazeModel::toJson() const {
    QJsonObject object;
    object.insert(QStringLiteral("format"), QStringLiteral("algorithm-maze-v1"));
    object.insert(QStringLiteral("rows"), rows_);
    object.insert(QStringLiteral("columns"), columns_);
    object.insert(QStringLiteral("startCell"), startCell());
    object.insert(QStringLiteral("endCell"), endCell());
    object.insert(QStringLiteral("bossAtCell"), endCell());

    QJsonArray gridArray;
    for (const QString &line : expandedGrid()) {
        gridArray.append(line);
    }
    object.insert(QStringLiteral("expandedMatrix"), gridArray);

    QJsonArray edgeArray;
    QSet<quint64> emitted;
    for (int cell = 0; cell < cellCount(); ++cell) {
        for (int next : neighbors(cell)) {
            const quint64 key = edgeKey(cell, next);
            if (emitted.contains(key)) {
                continue;
            }
            emitted.insert(key);
            QJsonArray edge;
            edge.append(cell);
            edge.append(next);
            edgeArray.append(edge);
        }
    }
    object.insert(QStringLiteral("passages"), edgeArray);

    QJsonArray resourceArray;
    for (int cell = 0; cell < cellCount(); ++cell) {
        if (resourceAt(cell) == 0) {
            continue;
        }
        QJsonObject resource;
        resource.insert(QStringLiteral("cell"), cell);
        resource.insert(QStringLiteral("value"), resourceAt(cell));
        resource.insert(QStringLiteral("type"),
                        resourceAt(cell) > 0 ? QStringLiteral("coin")
                                             : QStringLiteral("trap"));
        resourceArray.append(resource);
    }
    object.insert(QStringLiteral("resources"), resourceArray);
    return object;
}

QStringList MazeModel::compactGrid() const {
    const int gridRows = rows_ * 2 + 1;
    const int gridCols = columns_ * 2 + 1;
    QStringList grid;
    for (int r = 0; r < gridRows; ++r) {
        grid.append(QString(gridCols, QLatin1Char('#')));
    }

    for (int cell = 0; cell < cellCount(); ++cell) {
        const int gr = rowOf(cell) * 2 + 1;
        const int gc = columnOf(cell) * 2 + 1;
        QChar marker = QLatin1Char(' ');
        if (cell == startCell()) {
            marker = QLatin1Char('S');
        } else if (hasBoss_ && cell == bossCell_) {
            marker = QLatin1Char('B');
        } else if (cell == endCell()) {
            marker = QLatin1Char('E');
        } else if (resourceAt(cell) > 0) {
            marker = QLatin1Char('G');
        } else if (resourceAt(cell) < 0) {
            marker = QLatin1Char('T');
        }
        grid[gr][gc] = marker;

        if (passages_[cell][Right] && columnOf(cell) + 1 < columns_) {
            grid[gr][gc + 1] = QLatin1Char(' ');
        }
        if (passages_[cell][Down] && rowOf(cell) + 1 < rows_) {
            grid[gr + 1][gc] = QLatin1Char(' ');
        }
    }
    return grid;
}

QJsonObject MazeModel::toCrossTestJson(const QVector<int> &bossHealth,
                                       const QVector<BossSkill> &skills,
                                       int minRounds,
                                       int coinConsumption) const {
    QJsonObject object;

    QJsonArray mazeArray;
    for (const QString &line : compactGrid()) {
        QJsonArray row;
        for (int i = 0; i < line.size(); ++i) {
            row.append(QString(line[i]));
        }
        mazeArray.append(row);
    }
    object.insert(QStringLiteral("maze"), mazeArray);

    QJsonArray bossArray;
    for (int health : bossHealth) {
        bossArray.append(health);
    }
    object.insert(QStringLiteral("B"), bossArray);

    QJsonArray skillsArray;
    for (const BossSkill &skill : skills) {
        QJsonArray pair;
        pair.append(skill.damage);
        pair.append(skill.cooldown);
        skillsArray.append(pair);
    }
    object.insert(QStringLiteral("PlayerSkills"), skillsArray);

    object.insert(QStringLiteral("minRouds"), minRounds);
    object.insert(QStringLiteral("CoinConsumption"), coinConsumption);
    return object;
}
