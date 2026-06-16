#pragma once

#include "bosssolver.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include <array>
#include <random>

enum class MazeAlgorithm {
    DivideAndConquer,
    KruskalMst,
    DepthFirstSearch,
    BreadthFirstSearch
};

struct MazeEdge {
    int from = -1;
    int to = -1;
};

struct ResourcePlan {
    int maxValue = 0;
    QVector<int> walk;
    QVector<int> collectedCells;
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

class MazeModel {
public:
    void generate(int rows, int columns, MazeAlgorithm algorithm, quint32 seed);
    void placeResources(int coinCount, int trapCount, quint32 seed);

    [[nodiscard]] int rows() const { return rows_; }
    [[nodiscard]] int columns() const { return columns_; }
    [[nodiscard]] int cellCount() const { return rows_ * columns_; }
    [[nodiscard]] int startCell() const { return startCell_; }
    [[nodiscard]] int endCell() const { return endCell_; }
    [[nodiscard]] int bossCell() const { return bossCell_; }
    [[nodiscard]] bool hasBoss() const { return hasBoss_; }
    void setBossCell(int cell);
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
