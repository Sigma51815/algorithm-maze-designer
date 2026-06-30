#pragma once

#include "bosssolver.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include <algorithm>
#include <array>
#include <random>

enum class MazeAlgorithm {
    DivideAndConquer,   // 分治法：递归切分矩形区域，每次只开一个口连接两个子迷宫
    KruskalMst,         // Kruskal：把相邻格看成候选边，用并查集选出一棵随机生成树
    DepthFirstSearch,   // DFS 回溯：随机向未访问邻居深入，走不动再退栈继续分支
    BreadthFirstSearch  // 分支限界 BFS：优先队列维护边界，按下界估计选择扩展边
};

struct MazeEdge {
    int from = -1;
    int to = -1;
};

struct ResourceBranchDecision {
    int attachCell = -1;
    int rootCell = -1;
    int gain = 0;
    bool selected = false;
    QVector<int> cells;
};

// 任务② DP 的输出包：同时保存最大资源数、实际路径、首次计分格子和分支取舍记录。
struct ResourcePlan {
    int maxValue = 0;
    QVector<int> walk;
    QVector<int> collectedCells;
    QVector<int> backboneCells;
    QVector<ResourceBranchDecision> branchDecisions;
    QVector<int> cumulativeValues;
};

struct MazeStatistics {
    int solutionLength = 0;
    int deadEnds = 0;
    int junctions = 0;
    int longestCorridor = 0;
};

struct CellTopology {
    int cell = -1;
    int depth = 0;
    int branchDepth = 0;
    bool onMainPath = false;
    bool isDeadEnd = false;
    bool isJunction = false;
    int corridorLength = 0;
};

// Adaptive coin / trap counts derived from maze cell count.
// Density: ~12 % coins, ~8 % traps, with lower/upper bounds.
inline int autoCoinCount(int cellCount) {
    return std::clamp(static_cast<int>(cellCount * 0.12), 3, 50);
}
inline int autoTrapCount(int cellCount) {
    return std::clamp(static_cast<int>(cellCount * 0.08), 2, 35);
}

class MazeModel {
public:
    void generate(int rows, int columns, MazeAlgorithm algorithm, quint32 seed);
    void placeResources(int coinCount, int trapCount, quint32 seed,
                        int coinValue = 50, int trapValue = -30);

    [[nodiscard]] int rows() const { return rows_; }
    [[nodiscard]] int columns() const { return columns_; }
    [[nodiscard]] int cellCount() const { return rows_ * columns_; }
    [[nodiscard]] int startCell() const { return startCell_; }
    [[nodiscard]] int endCell() const { return endCell_; }
    [[nodiscard]] int bossCell() const { return bossCell_; }
    [[nodiscard]] bool hasBoss() const { return hasBoss_; }
    void setBossCell(int cell);
    bool setSpecialCells(int startCell, int endCell, int bossCell, bool hasBoss);
    [[nodiscard]] int resourceAt(int cell) const;
    void consumeResource(int cell);
    [[nodiscard]] bool isOpen(int first, int second) const;
    [[nodiscard]] QVector<int> neighbors(int cell) const;
    [[nodiscard]] const QVector<MazeEdge> &generationSteps() const { return generationSteps_; }

    [[nodiscard]] bool validatePerfect(QString *reason = nullptr) const;
    [[nodiscard]] MazeStatistics statistics() const;
    [[nodiscard]] QVector<CellTopology> analyzeTopology() const;
    [[nodiscard]] QVector<int> deadEndCells() const;
    [[nodiscard]] QVector<int> branchCells() const;
    [[nodiscard]] QVector<int> mainPathCells() const;
    [[nodiscard]] ResourcePlan optimalResourceWalk() const;
    [[nodiscard]] ResourcePlan optimalStartToEndResourceWalk() const;
    [[nodiscard]] QStringList expandedGrid() const;
    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] QStringList compactGrid() const;
    [[nodiscard]] QJsonObject toCrossTestJson(const QVector<int> &bossHealth,
                                              const QVector<BossSkill> &skills,
                                              int minRounds,
                                              int coinConsumption) const;
    [[nodiscard]] MazeModel extractSubArea(int centerCell) const;

    [[nodiscard]] QVector<MazeEdge> allEdges() const;
    void setFromEdges(int rows, int columns, const QVector<MazeEdge> &edges, quint32 seed);
    void setResources(const QVector<int> &resources);

    [[nodiscard]] static bool fromExpandedGrid(const QJsonArray &matrix,
                                                MazeModel &out,
                                                QString *error = nullptr);

private:
    static constexpr int Up = 0;
    static constexpr int Right = 1;
    static constexpr int Down = 2;
    static constexpr int Left = 3;

    int rows_ = 0;
    int columns_ = 0;
    int startCell_ = 0;
    int endCell_ = 0;
    int bossCell_ = 0;
    bool hasBoss_ = false;
    QVector<std::array<bool, 4>> passages_;
    QVector<int> resources_;
    QVector<MazeEdge> generationSteps_;
    std::mt19937 random_;

    [[nodiscard]] int index(int row, int column) const { return row * columns_ + column; }
    [[nodiscard]] int rowOf(int cell) const { return cell / columns_; }
    [[nodiscard]] int columnOf(int cell) const { return cell % columns_; }
    [[nodiscard]] QVector<int> gridNeighbors(int cell) const;
    [[nodiscard]] QVector<int> bfsParent() const;
    void reset(int rows, int columns, quint32 seed);
    void carve(int first, int second);
    void chooseDiameterEndpoints();

    void generateDivideAndConquer(int top, int bottom, int left, int right);
    void generateKruskal();
    void generateDepthFirst();
    void generateBreadthFirst();
};
