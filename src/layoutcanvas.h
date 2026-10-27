#pragma once
#include <QWidget>
#include <QDockWidget>
#include <QMainWindow>
#include <QListWidget>

class LayoutCanvas : public QWidget {
    Q_OBJECT
  public:
    struct Splitter {
        QDockWidget *first;
        QDockWidget *second;
        Qt::Orientation orientation;
        QRectF rect;
    };
    LayoutCanvas(QMainWindow *window, const QList<QDockWidget *> &panels, QWidget *parent = nullptr);
    void select(QDockWidget *panel);
    void setPanels(const QList<QDockWidget *> &items);
    QRectF panelRect(const QString &id) const;
    QList<Splitter> splitters() const;
  signals:
    void selectedChanged(QDockWidget *panel);
    void interactionStarted();
    void layoutChanged();

  protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void dragEnterEvent(QDragEnterEvent *) override;
    void dragMoveEvent(QDragMoveEvent *) override;
    void dropEvent(QDropEvent *) override;
    void dragLeaveEvent(QDragLeaveEvent *) override;
    void leaveEvent(QEvent *) override;

  private:
    QMainWindow *window;
    QList<QDockWidget *> panels;
    QDockWidget *selected = nullptr;
    QDockWidget *dragged = nullptr;
    QPointF start;
    QPointF pointer;
    QList<QPair<QDockWidget *, QRectF>> visibleRects() const;
    QRectF frame() const;
    int splitter = -1;
    QList<Splitter> handles;
    int firstSize = 0, secondSize = 0;
    bool moved = false;
    QByteArray before;
    QPair<QDockWidget *, int> targetAt(const QPointF &point) const;
    void place(const QPointF &point);
};

class PanelList : public QListWidget {
  public:
    using QListWidget::QListWidget;

  protected:
    void startDrag(Qt::DropActions supported) override;
};
