#pragma once

#include "core/bosssolver.h"

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

class MazeModel {
public:
    void generate(int rows, int columns, MazeAlgorithm algorithm, quint32 seed);
    void placeResources(int coinCount, int trapCount, quint32 seed);

    [[nodiscard]] int rows() const { return rows_; }
    [[nodiscard]] int columns() const { return columns_; }
    [[nodiscard]] int cellCount() const { return rows_ * columns_; }
    [[nodiscard]] int startCell() const { return 0; }
    [[nodiscard]] int endCell() const { return cellCount() - 1; }
    [[nodiscard]] int bossCell() const { return bossCell_; }
    [[nodiscard]] bool hasBoss() const { return hasBoss_; }
    void setBossCell(int cell);
    [[nodiscard]] int resourceAt(int cell) const;
    void consumeResource(int cell);
    [[nodiscard]] bool isOpen(int first, int second) const;
    [[nodiscard]] QVector<int> neighbors(int cell) const;
    [[nodiscard]] const QVector<MazeEdge> &generationSteps() const { return generationSteps_; }

    [[nodiscard]] bool validatePerfect(QString *reason = nullptr) const;
    [[nodiscard]] ResourcePlan optimalResourceWalk() const;
    [[nodiscard]] QStringList expandedGrid() const;
    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] QStringList compactGrid() const;
    [[nodiscard]] QJsonObject toCrossTestJson(const QVector<int> &bossHealth,
                                              const QVector<BossSkill> &skills,
                                              int minRounds,
                                              int coinConsumption) const;

private:
    static constexpr int Up = 0;
    static constexpr int Right = 1;
    static constexpr int Down = 2;
    static constexpr int Left = 3;

    int rows_ = 0;
    int columns_ = 0;
    int bossCell_ = -1;
    bool hasBoss_ = false;
    QVector<std::array<bool, 4>> passages_;
    QVector<int> resources_;
    QVector<MazeEdge> generationSteps_;
    std::mt19937 random_;

    [[nodiscard]] int index(int row, int column) const { return row * columns_ + column; }
    [[nodiscard]] int rowOf(int cell) const { return cell / columns_; }
    [[nodiscard]] int columnOf(int cell) const { return cell % columns_; }
    [[nodiscard]] QVector<int> gridNeighbors(int cell) const;
    void reset(int rows, int columns, quint32 seed);
    void carve(int first, int second);

    void generateDivideAndConquer(int top, int bottom, int left, int right);
    void generateKruskal();
    void generateDepthFirst();
    void generateBreadthFirst();
};
