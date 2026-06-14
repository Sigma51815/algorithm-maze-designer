#include "maze.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QQueue>
#include <QSet>

#include <algorithm>
#include <functional>
#include <numeric>
#include <queue>

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
    startCell_ = 0;
    endCell_ = cellCount() - 1;
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
    chooseDiameterEndpoints();
    hasBoss_ = true;
}

void MazeModel::chooseDiameterEndpoints() {
    auto farthestFrom = [&](int source) {
        QVector<int> distance(cellCount(), -1);
        QQueue<int> queue;
        distance[source] = 0;
        queue.enqueue(source);
        int farthest = source;
        while (!queue.isEmpty()) {
            const int current = queue.dequeue();
            if (distance[current] > distance[farthest]) {
                farthest = current;
            }
            for (int next : neighbors(current)) {
                if (distance[next] >= 0) {
                    continue;
                }
                distance[next] = distance[current] + 1;
                queue.enqueue(next);
            }
        }
        return farthest;
    };

    startCell_ = farthestFrom(0);
    endCell_ = farthestFrom(startCell_);

    QVector<int> parent(cellCount(), -1);
    QQueue<int> queue;
    parent[startCell_] = startCell_;
    queue.enqueue(startCell_);
    while (!queue.isEmpty() && parent[endCell_] < 0) {
        const int current = queue.dequeue();
        for (int next : neighbors(current)) {
            if (parent[next] < 0) {
                parent[next] = current;
                queue.enqueue(next);
            }
        }
    }
    bossCell_ = parent[endCell_] >= 0 && parent[endCell_] != startCell_
        ? parent[endCell_]
        : endCell_;
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
    struct FrontierBranch {
        int lowerBound = 0;
        quint32 tieBreaker = 0;
        int from = -1;
        int to = -1;
        int depth = 0;
        int randomWeight = 0;
        int sourceDegree = 0;
    };
    struct WorseBranch {
        bool operator()(const FrontierBranch &first, const FrontierBranch &second) const {
            if (first.lowerBound != second.lowerBound) {
                return first.lowerBound > second.lowerBound;
            }
            return first.tieBreaker > second.tieBreaker;
        }
    };

    QVector<std::array<bool, 4>> bestPassages;
    QVector<MazeEdge> bestSteps;
    int bestScore = -1;
    const int directDistance = rows_ + columns_ - 2;

    for (int attempt = 0; attempt < 12; ++attempt) {
        passages_.fill({false, false, false, false}, cellCount());
        generationSteps_.clear();

        QVector<bool> visited(cellCount(), false);
        QVector<int> degree(cellCount(), 0);
        std::priority_queue<FrontierBranch, QVector<FrontierBranch>, WorseBranch> frontier;
        std::uniform_int_distribution<int> randomCost(0, 99);
        std::uniform_int_distribution<quint32> tieBreaker;

        auto addBranches = [&](int cell, int depth) {
            QVector<int> choices = gridNeighbors(cell);
            std::shuffle(choices.begin(), choices.end(), random_);
            for (int next : choices) {
                if (visited[next]) {
                    continue;
                }
                const int randomWeight = randomCost(random_);
                const int lowerBound = depth * 10000 + degree[cell] * 100
                    + randomWeight;
                frontier.push({lowerBound, tieBreaker(random_), cell, next, depth,
                               randomWeight, degree[cell]});
            }
        };

        std::uniform_int_distribution<int> rootDistribution(0, cellCount() - 1);
        const int root = rootDistribution(random_);
        visited[root] = true;
        addBranches(root, 1);

        while (!frontier.empty()) {
            FrontierBranch branch = frontier.top();
            frontier.pop();
            if (visited[branch.to]) {
                continue;
            }
            if (branch.sourceDegree != degree[branch.from]) {
                branch.sourceDegree = degree[branch.from];
                branch.lowerBound = branch.depth * 10000 + branch.sourceDegree * 100
                    + branch.randomWeight;
                frontier.push(branch);
                continue;
            }
            visited[branch.to] = true;
            carve(branch.from, branch.to);
            ++degree[branch.from];
            ++degree[branch.to];
            addBranches(branch.to, branch.depth + 1);
        }

        chooseDiameterEndpoints();
        const MazeStatistics stats = statistics();
        const int score = stats.solutionLength * 10 + stats.deadEnds;
        if (score > bestScore) {
            bestScore = score;
            bestPassages = passages_;
            bestSteps = generationSteps_;
        }
        if (stats.solutionLength >= directDistance + 6) {
            break;
        }
    }

    passages_ = bestPassages;
    generationSteps_ = bestSteps;
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
    std::mt19937 engine(seed);

    QVector<int> parent(cellCount(), -1);
    QQueue<int> queue;
    parent[startCell()] = startCell();
    queue.enqueue(startCell());
    while (!queue.isEmpty() && parent[endCell()] < 0) {
        const int current = queue.dequeue();
        for (int next : neighbors(current)) {
            if (parent[next] < 0) {
                parent[next] = current;
                queue.enqueue(next);
            }
        }
    }

    QVector<bool> onMainPath(cellCount(), false);
    for (int cell = endCell();; cell = parent[cell]) {
        onMainPath[cell] = true;
        if (cell == startCell()) {
            break;
        }
    }

    QVector<int> mainPathCells;
    QVector<int> branchCells;
    for (int cell = 0; cell < cellCount(); ++cell) {
        if (cell == startCell() || cell == endCell() || cell == bossCell()) {
            continue;
        }
        (onMainPath[cell] ? mainPathCells : branchCells).append(cell);
    }
    std::shuffle(mainPathCells.begin(), mainPathCells.end(), engine);
    std::shuffle(branchCells.begin(), branchCells.end(), engine);

    const int availableCount = std::max(0, cellCount() - 3);
    coinCount = std::clamp(coinCount, 0, availableCount);
    trapCount = std::clamp(trapCount, 0, availableCount - coinCount);

    const int mainPathTrapCount = std::min(
        static_cast<int>(mainPathCells.size()), (trapCount + 2) / 3);
    for (int i = 0; i < mainPathTrapCount; ++i) {
        resources_[mainPathCells[i]] = -30;
    }

    QVector<int> remaining;
    for (int i = mainPathTrapCount; i < mainPathCells.size(); ++i) {
        remaining.append(mainPathCells[i]);
    }
    remaining += branchCells;
    std::shuffle(remaining.begin(), remaining.end(), engine);
    const int remainingTraps = std::min(trapCount - mainPathTrapCount,
                                         static_cast<int>(remaining.size()));
    for (int i = 0; i < remainingTraps; ++i) {
        resources_[remaining.takeLast()] = -30;
    }

    QVector<int> coinCandidates;
    for (int cell : branchCells) {
        if (resources_[cell] == 0) {
            coinCandidates.append(cell);
        }
    }
    for (int cell : mainPathCells) {
        if (resources_[cell] == 0) {
            coinCandidates.append(cell);
        }
    }
    coinCount = std::min(coinCount, static_cast<int>(coinCandidates.size()));
    for (int i = 0; i < coinCount; ++i) {
        resources_[coinCandidates[i]] = 50;
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

MazeStatistics MazeModel::statistics() const {
    MazeStatistics result;
    if (cellCount() == 0) {
        return result;
    }

    QVector<int> parent(cellCount(), -1);
    QQueue<int> queue;
    parent[startCell()] = startCell();
    queue.enqueue(startCell());
    while (!queue.isEmpty()) {
        const int current = queue.dequeue();
        const int degree = neighbors(current).size();
        if (degree == 1 && current != startCell() && current != endCell()) {
            ++result.deadEnds;
        } else if (degree >= 3) {
            ++result.junctions;
        }
        for (int next : neighbors(current)) {
            if (parent[next] >= 0) {
                continue;
            }
            parent[next] = current;
            queue.enqueue(next);
        }
    }

    for (int cell = endCell(); cell != startCell(); cell = parent[cell]) {
        ++result.solutionLength;
    }

    for (int cell = 0; cell < cellCount(); ++cell) {
        if (neighbors(cell).size() == 2) {
            continue;
        }
        for (int next : neighbors(cell)) {
            int length = 1;
            int previous = cell;
            int current = next;
            while (neighbors(current).size() == 2) {
                const QVector<int> adjacent = neighbors(current);
                const int following = adjacent[0] == previous ? adjacent[1] : adjacent[0];
                previous = current;
                current = following;
                ++length;
            }
            result.longestCorridor = std::max(result.longestCorridor, length);
        }
    }
    return result;
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
        QChar marker = QLatin1Char(' ');
        if (cell == startCell()) {
            marker = QLatin1Char('S');
        } else if (cell == endCell()) {
            marker = QLatin1Char('E');
        } else if (cell == bossCell()) {
            marker = QLatin1Char('B');
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
                = QLatin1Char(' ');
        }
    }
    return grid;
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

QJsonObject MazeModel::toJson() const {
    QJsonObject object;
    object.insert(QStringLiteral("format"), QStringLiteral("algorithm-maze-v1"));
    object.insert(QStringLiteral("rows"), rows_);
    object.insert(QStringLiteral("columns"), columns_);
    object.insert(QStringLiteral("startCell"), startCell());
    object.insert(QStringLiteral("endCell"), endCell());
    object.insert(QStringLiteral("bossAtCell"), bossCell());

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

MazeModel MazeModel::extractSubArea(int centerCell) const {
    MazeModel sub;
    const int cr = rowOf(centerCell);
    const int cc = columnOf(centerCell);
    sub.rows_ = 3;
    sub.columns_ = 3;
    sub.passages_.fill({false, false, false, false}, 9);
    sub.resources_.fill(0, 9);
    sub.hasBoss_ = false;
    sub.bossCell_ = -1;
    sub.startCell_ = 4;
    sub.endCell_ = 4;

    auto subIndex = [&](int r, int c) { return (r - cr + 1) * 3 + (c - cc + 1); };

    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            const int r = cr + dr;
            const int c = cc + dc;
            if (r < 0 || r >= rows_ || c < 0 || c >= columns_) continue;
            const int origCell = index(r, c);
            const int subCell = subIndex(r, c);
            sub.resources_[subCell] = resources_[origCell];
            for (int dir = 0; dir < 4; ++dir) {
                if (!passages_[origCell][dir]) continue;
                int nr = r, nc = c;
                if (dir == 0) --nr; else if (dir == 1) ++nc;
                else if (dir == 2) ++nr; else --nc;
                if (nr >= cr - 1 && nr <= cr + 1 && nc >= cc - 1 && nc <= cc + 1) {
                    sub.passages_[subCell][dir] = true;
                }
            }
        }
    }
    return sub;
}
