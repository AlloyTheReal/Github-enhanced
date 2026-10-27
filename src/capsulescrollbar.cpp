#include "capsulescrollbar.h"
#include "visuals.h"
#include <QtWidgets>
#include <cmath>
CapsuleScrollBar::CapsuleScrollBar(Qt::Orientation orientation, QWidget *parent)
    : QScrollBar(orientation, parent) {
  setObjectName("capsuleScrollBar");
  setMouseTracking(true);
  setAttribute(Qt::WA_Hover);
  setAutoFillBackground(false);
  expansion.setEasingCurve(QEasingCurve::OutCubic);
  fade.setEasingCurve(QEasingCurve::OutCubic);
  connect(&expansion, &QVariantAnimation::valueChanged, this,
          [this](QVariant v) {
            expanded = v.toReal();
            update();
          });
  connect(&fade, &QVariantAnimation::valueChanged, this, [this](QVariant v) {
    opacity = v.toReal();
    update();
  });
  quiet.setSingleShot(true);
  connect(&quiet, &QTimer::timeout, this, [this] {
    if (!underMouse() && !dragging)
      animate(fade, opacity, .08, 380);
  });
  connect(this, &QScrollBar::valueChanged, this, [this] { reveal(); });
  connect(this, &QScrollBar::rangeChanged, this, [this] { update(); });
  connect(&repeat, &QTimer::timeout, this, [this] {
    repeat.setInterval(65);
    triggerAction(pageAction);
  });
}
QSize CapsuleScrollBar::sizeHint() const {
  return orientation() == Qt::Vertical ? QSize(12, 100) : QSize(100, 12);
}
bool CapsuleScrollBar::reversed() const {
  return invertedAppearance() ^ (orientation() == Qt::Horizontal &&
                                 layoutDirection() == Qt::RightToLeft);
}
qreal CapsuleScrollBar::coordinate(const QPointF &position) const {
  return orientation() == Qt::Vertical ? position.y() : position.x();
}
QRectF CapsuleScrollBar::thumbRect() const {
  const qreal length =
      qMax(0, (orientation() == Qt::Vertical ? height() : width()) - 6);
  const qreal range = qreal(maximum()) - minimum();
  const qreal thumb =
      qMin(length, qMax(qreal(36), length * qMax(1, pageStep()) /
                                       (range + qMax(1, pageStep()))));
  qreal ratio = range > 0 ? (qreal(value()) - minimum()) / range : 0;
  if (reversed())
    ratio = 1 - ratio;
  const qreal start = 3 + ratio * (length - thumb),
              thickness = 4 + expanded * 3;
  return orientation() == Qt::Vertical
             ? QRectF((width() - thickness) / 2, start, thickness, thumb)
             : QRectF(start, (height() - thickness) / 2, thumb, thickness);
}
void CapsuleScrollBar::paintEvent(QPaintEvent *) {
  if (maximum() <= minimum())
    return;
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);
  auto color = palette().color(QPalette::Text);
  color.setAlphaF(opacity);
  painter.setBrush(color);
  auto r = thumbRect();
  const auto radius = qMin(r.width(), r.height()) / 2;
  painter.drawRoundedRect(r, radius, radius);
}
void CapsuleScrollBar::animate(QVariantAnimation &animation, qreal from,
                               qreal to, int duration) {
  animation.stop();
  if (Visuals::reducedMotion()) {
    if (&animation == &fade)
      opacity = to;
    else
      expanded = to;
    update();
    return;
  }
  animation.setStartValue(from);
  animation.setEndValue(to);
  animation.setDuration(duration);
  animation.start();
}
void CapsuleScrollBar::reveal() {
  animate(fade, opacity, underMouse() || dragging ? .85 : .6, 110);
  quiet.start(1000);
}
void CapsuleScrollBar::enterEvent(QEnterEvent *event) {
  animate(expansion, expanded, 1, 160);
  reveal();
  QScrollBar::enterEvent(event);
}
void CapsuleScrollBar::leaveEvent(QEvent *event) {
  if (!dragging)
    animate(expansion, expanded, 0, 200);
  quiet.start(750);
  QScrollBar::leaveEvent(event);
}
void CapsuleScrollBar::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) {
    QScrollBar::mousePressEvent(event);
    return;
  }
  reveal();
  const auto thumb = thumbRect();
  const bool within = orientation() == Qt::Vertical
                          ? event->position().y() >= thumb.top() &&
                                event->position().y() <= thumb.bottom()
                          : event->position().x() >= thumb.left() &&
                                event->position().x() <= thumb.right();
  if (within) {
    dragging = true;
    dragOffset = coordinate(event->position()) -
                 (orientation() == Qt::Vertical ? thumb.top() : thumb.left());
    setSliderDown(true);
  } else {
    const bool after =
        coordinate(event->position()) >
        (orientation() == Qt::Vertical ? thumb.bottom() : thumb.right());
    pageAction = (after != reversed()) ? SliderPageStepAdd : SliderPageStepSub;
    triggerAction(pageAction);
    repeat.start(350);
  }
  event->accept();
}
void CapsuleScrollBar::mouseMoveEvent(QMouseEvent *event) {
  if (!dragging) {
    QScrollBar::mouseMoveEvent(event);
    return;
  }
  const auto thumb = thumbRect();
  const int travel =
      qMax(0, int((orientation() == Qt::Vertical ? height() - thumb.height()
                                                 : width() - thumb.width()) -
                  6));
  const int position = qRound(coordinate(event->position()) - dragOffset - 3);
  setSliderPosition(QStyle::sliderValueFromPosition(
      minimum(), maximum(), position, travel, reversed()));
  reveal();
  event->accept();
}
void CapsuleScrollBar::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) {
    QScrollBar::mouseReleaseEvent(event);
    return;
  }
  repeat.stop();
  if (dragging) {
    dragging = false;
    setSliderDown(false);
  }
  if (!underMouse())
    animate(expansion, expanded, 0, 200);
  quiet.start(750);
  event->accept();
}
void CapsuleScrollBar::keyPressEvent(QKeyEvent *event) {
  if (auto *motion = findChild<QVariantAnimation *>("smoothWheel"))
    motion->stop();
  QScrollBar::keyPressEvent(event);
  reveal();
}
void installCapsuleScrollBars(QAbstractScrollArea *area) {
  if (area->property("capsuleScrollBarsInstalled").toBool())
    return;
  area->setProperty("capsuleScrollBarsInstalled", true);
  area->setVerticalScrollBar(new CapsuleScrollBar(Qt::Vertical, area));
  area->setHorizontalScrollBar(new CapsuleScrollBar(Qt::Horizontal, area));
}
