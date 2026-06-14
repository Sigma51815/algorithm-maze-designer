#pragma once

#include "maze.h"

#include <QWidget>

class MazeWidget : public QWidget {
    Q_OBJECT

public:
    explicit MazeWidget(QWidget *parent = nullptr);

    void setMaze(const MazeModel &maze);
    void setRevealCount(int count);
    void setSolutionPath(const QVector<int> &path, int visiblePoints);
    void clearSolutionPath();
    void setAiPath(const QVector<int> &path, int visiblePoints);
    void clearAiPath();

protected:
    void paintEvent(QPaintEvent *event) override;
    QSize minimumSizeHint() const override;

private:
    MazeModel maze_;
    bool hasMaze_ = false;
    int revealCount_ = 0;
    QVector<int> solutionPath_;
    int visiblePathPoints_ = 0;
    QVector<int> aiPath_;
    int visibleAiPoints_ = 0;

    [[nodiscard]] bool passageVisible(int first, int second) const;
};
