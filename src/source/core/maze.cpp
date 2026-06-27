#include "maze.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
#include <QQueue>
#include <QSet>

#include <algorithm>
#include <functional>
#include <limits>
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

// 四种迷宫生成算法的统一入口。
//
// 约定：
// 1. rows/columns 是逻辑格子数量，不是最终输出字符矩阵的行列数。
// 2. reset() 会先把所有墙封闭，四种算法都只通过 carve() 打通相邻格。
// 3. 四种算法的共同目标都是生成一棵覆盖全部格子的树：
//    - 连通：任意两个格子之间都有路；
//    - 无环：任意两个格子之间只有一条简单路径；
//    - 边数为 V-1，因此是“完美迷宫”。
// 4. 生成拓扑后，chooseDiameterEndpoints() 再把最长路径附近设为起点、终点和 BOSS。
//
// UI 下拉框的索引会直接转换成 MazeAlgorithm，因此验收时选择
// “分治 / Kruskal / DFS / BFS”最终都会走到这里。
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

// 分治法生成迷宫。
//
// 输入参数表示当前递归负责的闭区间矩形：
//   行范围 [top, bottom]，列范围 [left, right]。
//
// 核心思想：
// 1. 如果当前区域只有一个格子，递归结束。
// 2. 否则优先沿较长方向随机切一刀，得到两个子矩形。
// 3. 分别递归生成两个子矩形内部的迷宫。
// 4. 在分割线随机选一个位置，用 carve() 打通左右/上下两个子迷宫。
//
// 正确性直觉：
// 每个子矩形递归结束后都是一棵树；当前层只在两棵树之间添加一条连接边。
// “两棵树 + 一条跨树边”仍然是一棵更大的树，所以不会成环，同时保持连通。
//
// 生成风格：
// 区域边界感较强，容易出现比较长的墙段和少量随机开口，结构比 DFS 更规整。
//
// 复杂度：
// V = rows * columns。平均情况下每个格子参与常数次划分，时间约 O(V)；
// 递归深度平均约 O(log V)，极端随机切分时最坏可到 O(V)。
void MazeModel::generateDivideAndConquer(int top, int bottom, int left, int right) {
    if (top == bottom && left == right) {
        return;
    }

    const int height = bottom - top + 1;
    const int width = right - left + 1;
    if (width >= height && width > 1) {
        // 竖向分割：左子矩形 [left, split]，右子矩形 [split + 1, right]。
        std::uniform_int_distribution<int> splitDistribution(left, right - 1);
        const int split = splitDistribution(random_);
        generateDivideAndConquer(top, bottom, left, split);
        generateDivideAndConquer(top, bottom, split + 1, right);
        // 在分割线随机开一个口，连接左右两个已经生成好的子迷宫。
        std::uniform_int_distribution<int> openingDistribution(top, bottom);
        const int openingRow = openingDistribution(random_);
        carve(index(openingRow, split), index(openingRow, split + 1));
    } else {
        // 横向分割：上子矩形 [top, split]，下子矩形 [split + 1, bottom]。
        std::uniform_int_distribution<int> splitDistribution(top, bottom - 1);
        const int split = splitDistribution(random_);
        generateDivideAndConquer(top, split, left, right);
        generateDivideAndConquer(split + 1, bottom, left, right);
        // 在分割线随机开一个口，连接上下两个已经生成好的子迷宫。
        std::uniform_int_distribution<int> openingDistribution(left, right);
        const int openingColumn = openingDistribution(random_);
        carve(index(split, openingColumn), index(split + 1, openingColumn));
    }
}

// 最小生成树（Kruskal）生成迷宫。
//
// 图模型：
// 每个格子是一个顶点；上下左右相邻格子之间的墙是一条候选边。
// 初始时没有任何通道，相当于 V 个互不连通的点。
//
// 核心思想：
// 1. 枚举所有“向右”和“向下”的相邻边，避免同一堵墙重复加入候选集。
// 2. 随机打乱候选边，相当于给每条边一个随机权重。
// 3. 按随机顺序尝试加入边；并查集判断两个端点是否已连通。
// 4. 如果端点不连通，就打通这堵墙并合并两个连通块；如果已连通就跳过。
//
// 正确性直觉：
// Kruskal 每次只连接两个不同连通块，因此不会产生环；候选边覆盖整个网格，
// 最终所有连通块会合并成一个，得到一棵覆盖全部格子的随机生成树。
//
// 生成风格：
// 随机性较均匀，不像分治那样有明显区域边界，也不像 DFS 那样偏长走廊。
//
// 复杂度：
// 网格候选边 E 约等于 2V。洗牌 O(E)，并查集合并/查询近似 O(E α(V))，
// 因为反阿克曼函数 α(V) 极小，实际可近似看作 O(V)。
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
        // unite() 返回 true 表示这条边连接了两个原本不同的连通块，可以安全打通。
        if (sets.unite(edge.from, edge.to)) {
            carve(edge.from, edge.to);
        }
    }
}

// 回溯法（DFS）生成迷宫。
//
// 数据结构：
// visited 记录格子是否已经加入生成树；stack 保存当前深搜路径。
//
// 核心思想：
// 1. 从 startCell_ 出发，把起点标记为已访问并压栈。
// 2. 查看栈顶格子的未访问邻居。
// 3. 如果存在未访问邻居，随机选择一个，打通当前格子到该邻居的墙，然后入栈。
// 4. 如果没有未访问邻居，说明这条路已经走到尽头，弹栈回退到上一个分叉。
// 5. 栈空时，所有可达格子都已访问；在规则网格中也就是全部格子。
//
// 正确性直觉：
// 一个格子只会在“第一次访问”时被父格子连接，因此每个非起点格子只有一条父边。
// 这种父子边集合天然不会成环；DFS 又会遍历完整网格，所以最终连通。
//
// 生成风格：
// DFS 会倾向于沿着一个方向深入很久，再慢慢回退补分支，因此常出现长走廊、
// 深死胡同和比较强的蜿蜒感。
//
// 复杂度：
// 每个格子入栈/出栈一次，相邻边检查常数次，时间 O(V + E)，网格中 E=O(V)。
// visited 和 stack 都是 O(V) 空间。
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
            // 当前格子四周都访问过了，回退到上一个仍可能有分支的格子。
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

// 分支限界法（BFS/优先队列边界扩展）生成迷宫。
//
// 这里的名字叫 BreadthFirstSearch，但实现不是普通 FIFO 队列 BFS，
// 而是“分层 BFS + 分支限界”的优先队列扩展。
//
// 数据结构：
// frontier 保存所有从已访问区域指向未访问格子的候选边。
// 每条候选边是一个 FrontierBranch，包含：
//   from/to       : 候选边两端；
//   depth         : 从随机根节点扩展到该边时的层数；
//   sourceDegree  : from 当前已经连接了多少条通道；
//   randomWeight  : 随机扰动，避免结果过于固定；
//   lowerBound    : 优先级，下界越小越先扩展。
//
// lowerBound 设计：
//   lowerBound = depth * 10000 + degree[from] * 100 + randomWeight
// depth 权重最大，使整体保持 BFS 分层扩展；degree 次之，影响分叉形态；
// randomWeight 最小，只负责同层同类候选的随机化。
//
// 分支限界含义：
// 每次只扩展当前 lowerBound 最小的候选边。如果 to 已经访问过，说明它已经
// 通过更早或更优的边加入生成树，这条边会造成环，直接剪掉。若 from 的度数
// 已经变化，则旧 lowerBound 失效，重新计算后再入队。
//
// 多次尝试：
// 外层最多生成 12 个候选迷宫，用 solutionLength * 10 + deadEnds 打分，
// 保留主路径更长、死胡同更多的结果，使迷宫更有挑战和区分度。
//
// 复杂度：
// 单次尝试中，每条候选边会进行优先队列操作，约 O(E log E)；
// 12 次是固定常数倍，网格中可写作 O(V log V)。
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
            // 把当前格子通向“未访问邻居”的所有边加入边界集合。
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
                // 目标格子已经被接入生成树，再打通会形成环，剪枝跳过。
                continue;
            }
            if (branch.sourceDegree != degree[branch.from]) {
                // from 的分叉度已变化，旧下界不再准确，更新后重新参与排序。
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
        // 用较长主路径和更多死胡同作为候选质量指标，保留当前最好的一次尝试。
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

void MazeModel::placeResources(int coinCount, int trapCount, quint32 seed,
                               int coinValue, int trapValue) {
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
        resources_[mainPathCells[i]] = trapValue;
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
        resources_[remaining.takeLast()] = trapValue;
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
    // Shuffle so coins don't deterministically favor branch cells over main-path cells.
    std::shuffle(coinCandidates.begin(), coinCandidates.end(), std::mt19937{static_cast<unsigned>(seed + 3)});
    for (int i = 0; i < coinCount; ++i) {
        resources_[coinCandidates[i]] = coinValue;
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

QVector<CellTopology> MazeModel::analyzeTopology() const {
    QVector<CellTopology> result(cellCount());
    if (cellCount() == 0) return result;

    QVector<int> parent = bfsParent();

    QVector<int> depth(cellCount(), -1);
    QQueue<int> queue;
    depth[startCell()] = 0;
    queue.enqueue(startCell());
    while (!queue.isEmpty()) {
        int cur = queue.dequeue();
        for (int next : neighbors(cur)) {
            if (depth[next] < 0) {
                depth[next] = depth[cur] + 1;
                queue.enqueue(next);
            }
        }
    }

    QVector<bool> onMainPath(cellCount(), false);
    if (parent[endCell()] < 0) return result;
    for (int cell = endCell();; cell = parent[cell]) {
        onMainPath[cell] = true;
        if (cell == startCell()) break;
    }

    for (int cell = 0; cell < cellCount(); ++cell) {
        result[cell].cell = cell;
        result[cell].depth = depth[cell];
        result[cell].onMainPath = onMainPath[cell];
        int degree = neighbors(cell).size();
        result[cell].isDeadEnd = (degree == 1 && cell != startCell() && cell != endCell());
        result[cell].isJunction = (degree >= 3);
    }

    for (int cell = 0; cell < cellCount(); ++cell) {
        if (!result[cell].isDeadEnd) continue;
        int d = 0;
        int cur = cell;
        int prev = -1;
        while (cur >= 0) {
            int degree = neighbors(cur).size();
            if (degree >= 3 || cur == startCell() || cur == endCell()) break;
            ++d;
            int next = -1;
            for (int n : neighbors(cur)) {
                if (n != prev) { next = n; break; }
            }
            prev = cur;
            cur = next;
        }
        cur = cell;
        prev = -1;
        int remaining = d;
        while (remaining > 0 && cur >= 0) {
            result[cur].branchDepth = remaining;
            result[cur].corridorLength = d;
            int next = -1;
            for (int n : neighbors(cur)) {
                if (n != prev) { next = n; break; }
            }
            prev = cur;
            cur = next;
            --remaining;
        }
    }

    return result;
}

QVector<int> MazeModel::deadEndCells() const {
    QVector<int> result;
    for (int cell = 0; cell < cellCount(); ++cell) {
        if (cell == startCell() || cell == endCell()) continue;
        if (neighbors(cell).size() == 1) result.append(cell);
    }
    return result;
}

QVector<int> MazeModel::bfsParent() const {
    QVector<int> parent(cellCount(), -1);
    if (cellCount() == 0) return parent;
    QQueue<int> queue;
    parent[startCell()] = startCell();
    queue.enqueue(startCell());
    while (!queue.isEmpty() && parent[endCell()] < 0) {
        int cur = queue.dequeue();
        for (int next : neighbors(cur)) {
            if (parent[next] < 0) {
                parent[next] = cur;
                queue.enqueue(next);
            }
        }
    }
    return parent;
}

QVector<int> MazeModel::branchCells() const {
    QVector<int> parent = bfsParent();
    QVector<bool> onMain(cellCount(), false);
    if (parent[endCell()] < 0) return {};
    for (int cell = endCell();; cell = parent[cell]) {
        onMain[cell] = true;
        if (cell == startCell()) break;
    }
    QVector<int> result;
    for (int cell = 0; cell < cellCount(); ++cell) {
        if (cell != startCell() && cell != endCell() && !onMain[cell])
            result.append(cell);
    }
    return result;
}

QVector<int> MazeModel::mainPathCells() const {
    QVector<int> parent = bfsParent();
    if (parent[endCell()] < 0) return {};
    QVector<int> result;
    for (int cell = endCell();; cell = parent[cell]) {
        result.append(cell);
        if (cell == startCell()) break;
    }
    return result;
}

ResourcePlan MazeModel::optimalResourceWalk() const {
    ResourcePlan result;
    if (cellCount() == 0) {
        return result;
    }

    // 完美迷宫是一棵树，因此“从某条边进入一个子树能获得多少净收益”
    // 可以用记忆化递归计算。负收益子树会被舍弃，正收益子树才值得绕路收集。
    QVector<QHash<int, int>> directedGain(cellCount());
    std::function<int(int, int)> calculateGain = [&](int cell, int from) {
        const auto cached = directedGain[cell].constFind(from);
        if (cached != directedGain[cell].constEnd()) {
            return *cached;
        }
        int value = resourceAt(cell);
        for (int next : neighbors(cell)) {
            if (next == from) {
                continue;
            }
            value += std::max(0, calculateGain(next, cell));
        }
        directedGain[cell].insert(from, value);
        return value;
    };

    int bestRoot = 0;
    int bestValue = std::numeric_limits<int>::min();
    // 尝试每个格子作为收集路径根节点，找到全局最优资源收益。
    // 这个结果在优化评估中作为“理想玩家”的上限分数。
    for (int cell = 0; cell < cellCount(); ++cell) {
        const int value = calculateGain(cell, -1);
        if (value > bestValue) {
            bestValue = value;
            bestRoot = cell;
        }
    }

    QSet<int> selected;
    std::function<void(int, int)> selectPositiveSubtree = [&](int cell, int from) {
        selected.insert(cell);
        for (int next : neighbors(cell)) {
            if (next == from) {
                continue;
            }
            if (calculateGain(next, cell) > 0) {
                selectPositiveSubtree(next, cell);
            }
        }
    };
    if (bestValue > 0 || resourceAt(bestRoot) >= 0) {
        selectPositiveSubtree(bestRoot, -1);
    } else {
        selected.insert(bestRoot);
    }

    result.backboneCells = selected.values();
    std::sort(result.backboneCells.begin(), result.backboneCells.end());

    auto collectSelectedCells = [&](int root, int from) {
        QVector<int> cells;
        std::function<void(int, int)> visit = [&](int cell, int parentCell) {
            if (!selected.contains(cell)) {
                return;
            }
            cells.append(cell);
            for (int next : neighbors(cell)) {
                if (next == parentCell) {
                    continue;
                }
                visit(next, cell);
            }
        };
        visit(root, from);
        std::sort(cells.begin(), cells.end());
        return cells;
    };

    for (int next : neighbors(bestRoot)) {
        if (!selected.contains(next)) {
            continue;
        }
        ResourceBranchDecision decision;
        decision.attachCell = bestRoot;
        decision.rootCell = next;
        decision.gain = calculateGain(next, bestRoot);
        decision.selected = true;
        decision.cells = collectSelectedCells(next, bestRoot);
        result.branchDecisions.append(decision);
    }

    for (int cell = 0; cell < cellCount(); ++cell) {
        if (!selected.contains(cell)) {
            continue;
        }
        for (int next : neighbors(cell)) {
            if (selected.contains(next) || cell > next) {
                continue;
            }
            ResourceBranchDecision decision;
            decision.attachCell = cell;
            decision.rootCell = next;
            decision.gain = calculateGain(next, cell);
            decision.selected = decision.gain > 0;
            decision.cells = collectSelectedCells(next, cell);
            result.branchDecisions.append(decision);
        }
    }

    result.walk.append(bestRoot);
    std::function<void(int, int)> appendSelectedWalk = [&](int cell, int from) {
        for (int next : neighbors(cell)) {
            if (next == from || !selected.contains(next)) {
                continue;
            }
            result.walk.append(next);
            appendSelectedWalk(next, cell);
            result.walk.append(cell);
        }
    };
    appendSelectedWalk(bestRoot, -1);

    int cumulative = 0;
    for (int cell : result.walk) {
        if (!result.collectedCells.contains(cell)) {
            result.collectedCells.append(cell);
            cumulative += resourceAt(cell);
        }
        result.cumulativeValues.append(cumulative);
    }
    std::sort(result.collectedCells.begin(), result.collectedCells.end());
    for (int cell : result.collectedCells) {
        result.maxValue += resourceAt(cell);
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
        } else if (hasBoss_ && cell == bossCell_) {
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

bool MazeModel::setSpecialCells(int startCell, int endCell, int bossCell, bool hasBoss) {
    if (startCell < 0 || endCell < 0 || startCell >= cellCount()
        || endCell >= cellCount() || startCell == endCell) {
        return false;
    }
    if (hasBoss
        && (bossCell < 0 || bossCell >= cellCount()
            || bossCell == startCell || bossCell == endCell)) {
        return false;
    }

    startCell_ = startCell;
    endCell_ = endCell;
    hasBoss_ = hasBoss;
    bossCell_ = hasBoss ? bossCell : -1;
    return true;
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
    object.insert(QStringLiteral("hasBoss"), hasBoss());
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

QVector<MazeEdge> MazeModel::allEdges() const {
    QSet<quint64> seen;
    QVector<MazeEdge> edges;
    for (int cell = 0; cell < cellCount(); ++cell) {
        for (int next : neighbors(cell)) {
            int lo = std::min(cell, next);
            int hi = std::max(cell, next);
            quint64 key = (static_cast<quint64>(lo) << 32) | static_cast<quint32>(hi);
            if (!seen.contains(key)) {
                seen.insert(key);
                edges.append({lo, hi});
            }
        }
    }
    return edges;
}

void MazeModel::setFromEdges(int rows, int columns, const QVector<MazeEdge> &edges,
                              quint32 seed) {
    reset(rows, columns, seed);
    for (const MazeEdge &edge : edges) {
        if (edge.from < 0 || edge.to < 0 || edge.from >= cellCount()
            || edge.to >= cellCount() || !gridNeighbors(edge.from).contains(edge.to)) {
            continue;
        }
        carve(edge.from, edge.to);
    }
    chooseDiameterEndpoints();
    hasBoss_ = true;
}

void MazeModel::setResources(const QVector<int> &resources) {
    resources_ = resources;
}

bool MazeModel::fromExpandedGrid(const QJsonArray &matrix, MazeModel &out,
                                  QString *error) {
    if (matrix.isEmpty()) {
        if (error) *error = QStringLiteral("迷宫矩阵为空");
        return false;
    }

    QStringList grid;
    for (const QJsonValue &rowVal : matrix) {
        if (rowVal.isString()) {
            grid.append(rowVal.toString());
        } else if (rowVal.isArray()) {
            const QJsonArray row = rowVal.toArray();
            QString line;
            for (const QJsonValue &cellVal : row) line += cellVal.toString();
            grid.append(line);
        } else {
            if (error) *error = QStringLiteral("无效的行格式");
            return false;
        }
    }

    const int gridRows = grid.size();
    const int gridCols = grid.isEmpty() ? 0 : grid.first().size();
    for (const QString &line : grid) {
        if (line.size() != gridCols) {
            if (error) *error = QStringLiteral("maze rows must have the same width");
            return false;
        }
    }
    if (gridRows < 3 || gridCols < 3 || gridRows % 2 == 0 || gridCols % 2 == 0) {
        if (error) *error = QStringLiteral("迷宫尺寸无效（需奇数×奇数且≥3）");
        return false;
    }

    const int logicalRows = (gridRows - 1) / 2;
    const int logicalCols = (gridCols - 1) / 2;
    const int totalCells = logicalRows * logicalCols;

    out.reset(logicalRows, logicalCols, 42);

    int startCell = -1;
    int endCell = -1;
    int bossCell = -1;
    int startCount = 0;
    int endCount = 0;
    int bossCount = 0;
    QVector<int> resources(totalCells, 0);
    QVector<MazeEdge> edges;

    for (int r = 0; r < logicalRows; ++r) {
        for (int c = 0; c < logicalCols; ++c) {
            const int gr = r * 2 + 1;
            const int gc = c * 2 + 1;
            const int cell = r * logicalCols + c;
            const QChar ch = grid[gr][gc];

            if (ch == QLatin1Char('S')) {
                startCell = cell;
                ++startCount;
            } else if (ch == QLatin1Char('E')) {
                endCell = cell;
                ++endCount;
            } else if (ch == QLatin1Char('B')) {
                bossCell = cell;
                ++bossCount;
            }
            else if (ch == QLatin1Char('G')) resources[cell] = 50;
            else if (ch == QLatin1Char('T')) resources[cell] = -30;

            if (c + 1 < logicalCols) {
                const int wallCol = gc + 1;
                if (wallCol < gridCols && grid[gr][wallCol] != QLatin1Char('#')) {
                    edges.append({cell, cell + 1});
                }
            }
            if (r + 1 < logicalRows) {
                const int wallRow = gr + 1;
                if (wallRow < gridRows && grid[wallRow][gc] != QLatin1Char('#')) {
                    edges.append({cell, cell + logicalCols});
                }
            }
        }
    }

    if (startCount != 1 || endCount != 1 || bossCount > 1) {
        if (error) *error = QStringLiteral("未找到起点(S)或终点(E)");
        return false;
    }

    out.reset(logicalRows, logicalCols, 42);
    for (const MazeEdge &edge : edges) {
        out.carve(edge.from, edge.to);
    }
    out.resources_ = resources;
    if (!out.setSpecialCells(startCell, endCell, bossCell, bossCell >= 0)) {
        if (error) *error = QStringLiteral("invalid start, end, or boss marker");
        return false;
    }
    QString reason;
    if (!out.validatePerfect(&reason)) {
        if (error) *error = QStringLiteral("imported maze is not perfect: %1").arg(reason);
        return false;
    }

    return true;
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
