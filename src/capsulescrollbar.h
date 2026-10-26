#pragma once
#include <QScrollBar>
#include <QTimer>
#include <QVariantAnimation>
class QAbstractScrollArea;
class CapsuleScrollBar : public QScrollBar {
  Q_OBJECT
public:
  explicit CapsuleScrollBar(Qt::Orientation orientation,
                            QWidget *parent = nullptr);
  QSize sizeHint() const override;
  QRectF thumbRect() const;

protected:
  void paintEvent(QPaintEvent *) override;
  void enterEvent(QEnterEvent *) override;
  void leaveEvent(QEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;
  void keyPressEvent(QKeyEvent *) override;

private:
  QVariantAnimation expansion, fade;
  QTimer quiet, repeat;
  qreal expanded = 0, opacity = .25, dragOffset = 0;
  bool dragging = false;
  SliderAction pageAction = SliderNoAction;
  void reveal();
  void animate(QVariantAnimation &animation, qreal from, qreal to,
               int duration);
  qreal coordinate(const QPointF &position) const;
  bool reversed() const;
};
void installCapsuleScrollBars(QAbstractScrollArea *area);
