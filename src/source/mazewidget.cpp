#include "mazewidget.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>

MazeWidget::MazeWidget(QWidget *parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAutoFillBackground(true);
}

void MazeWidget::setMaze(const MazeModel &maze) {
    maze_ = maze;
    hasMaze_ = maze_.cellCount() > 0;
    revealCount_ = maze_.generationSteps().size();
    clearSolutionPath();
    update();
}

void MazeWidget::setRevealCount(int count) {
    revealCount_ = std::clamp(count, 0,
                              static_cast<int>(maze_.generationSteps().size()));
    update();
}

void MazeWidget::setSolutionPath(const QVector<int> &path, int visiblePoints) {
    solutionPath_ = path;
    visiblePathPoints_ = std::clamp(visiblePoints, 0,
                                    static_cast<int>(solutionPath_.size()));
    update();
}

void MazeWidget::clearSolutionPath() {
    solutionPath_.clear();
    visiblePathPoints_ = 0;
    update();
}

QSize MazeWidget::minimumSizeHint() const {
    return {560, 560};
}

bool MazeWidget::passageVisible(int first, int second) const {
    if (!maze_.isOpen(first, second)) {
        return false;
    }
    if (revealCount_ >= maze_.generationSteps().size()) {
        return true;
    }
    for (int i = 0; i < revealCount_; ++i) {
        const MazeEdge &edge = maze_.generationSteps()[i];
        if ((edge.from == first && edge.to == second)
            || (edge.from == second && edge.to == first)) {
            return true;
        }
    }
    return false;
}

void MazeWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor("#f4f7f8"));

    if (!hasMaze_) {
        painter.setPen(QColor("#607080"));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("请生成迷宫"));
        return;
    }

    const qreal margin = 24.0;
    const qreal cellSize = std::min((width() - margin * 2) / maze_.columns(),
                                    (height() - margin * 2) / maze_.rows());
    const qreal boardWidth = cellSize * maze_.columns();
    const qreal boardHeight = cellSize * maze_.rows();
    const qreal originX = (width() - boardWidth) / 2.0;
    const qreal originY = (height() - boardHeight) / 2.0;

    auto centerOf = [&](int cell) {
        const int row = cell / maze_.columns();
        const int column = cell % maze_.columns();
        return QPointF(originX + (column + 0.5) * cellSize,
                       originY + (row + 0.5) * cellSize);
    };
    auto rectOf = [&](int cell) {
        const int row = cell / maze_.columns();
        const int column = cell % maze_.columns();
        return QRectF(originX + column * cellSize, originY + row * cellSize,
                      cellSize, cellSize);
    };

    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawRoundedRect(QRectF(originX, originY, boardWidth, boardHeight), 4, 4);
    painter.setBrush(QColor("#dff4e8"));
    painter.drawRect(rectOf(maze_.startCell()));
    painter.setBrush(QColor("#eee3ff"));
    painter.drawRect(rectOf(maze_.bossCell()));
    painter.setBrush(QColor("#fde7e7"));
    painter.drawRect(rectOf(maze_.endCell()));

    if (visiblePathPoints_ > 1) {
        QPainterPath path(centerOf(solutionPath_[0]));
        for (int i = 1; i < visiblePathPoints_; ++i) {
            path.lineTo(centerOf(solutionPath_[i]));
        }
        QPen pathPen(QColor(34, 122, 190, 180), std::max(2.0, cellSize * 0.22),
                     Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pathPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    }

    painter.setPen(Qt::NoPen);
    for (int cell = 0; cell < maze_.cellCount(); ++cell) {
        const QPointF center = centerOf(cell);
        const qreal radius = std::max(2.5, cellSize * 0.22);
        if (maze_.resourceAt(cell) > 0) {
            painter.setBrush(QColor("#f5b82e"));
            painter.drawEllipse(center, radius, radius);
            painter.setPen(QPen(QColor("#8a5b00"), 1));
            painter.drawText(QRectF(center.x() - radius, center.y() - radius,
                                    radius * 2, radius * 2),
                             Qt::AlignCenter, QStringLiteral("+") );
            painter.setPen(Qt::NoPen);
        } else if (maze_.resourceAt(cell) < 0) {
            QPolygonF triangle;
            triangle << QPointF(center.x(), center.y() - radius)
                     << QPointF(center.x() + radius, center.y() + radius)
                     << QPointF(center.x() - radius, center.y() + radius);
            painter.setBrush(QColor("#e36a3d"));
            painter.drawPolygon(triangle);
        }
    }

    painter.setPen(QPen(QColor("#17242d"), std::max(1.2, cellSize * 0.08),
                        Qt::SolidLine, Qt::SquareCap));
    for (int row = 0; row < maze_.rows(); ++row) {
        for (int column = 0; column < maze_.columns(); ++column) {
            const int cell = row * maze_.columns() + column;
            const qreal left = originX + column * cellSize;
            const qreal top = originY + row * cellSize;
            const qreal right = left + cellSize;
            const qreal bottom = top + cellSize;
            if (row == 0 || !passageVisible(cell, cell - maze_.columns())) {
                painter.drawLine(QPointF(left, top), QPointF(right, top));
            }
            if (column == 0 || !passageVisible(cell, cell - 1)) {
                painter.drawLine(QPointF(left, top), QPointF(left, bottom));
            }
            if (row == maze_.rows() - 1
                || !passageVisible(cell, cell + maze_.columns())) {
                painter.drawLine(QPointF(left, bottom), QPointF(right, bottom));
            }
            if (column == maze_.columns() - 1 || !passageVisible(cell, cell + 1)) {
                painter.drawLine(QPointF(right, top), QPointF(right, bottom));
            }
        }
    }

    QFont labelFont = painter.font();
    labelFont.setBold(true);
    labelFont.setPixelSize(std::max(8, static_cast<int>(cellSize * 0.38)));
    painter.setFont(labelFont);
    painter.setPen(QColor("#17613a"));
    painter.drawText(rectOf(maze_.startCell()), Qt::AlignCenter, QStringLiteral("S"));
    painter.setPen(QColor("#6d43a8"));
    painter.drawText(rectOf(maze_.bossCell()), Qt::AlignCenter, QStringLiteral("B"));
    painter.setPen(QColor("#9a2630"));
    painter.drawText(rectOf(maze_.endCell()), Qt::AlignCenter, QStringLiteral("E"));
}
