#pragma once
#include <QDialog>
#include <QCheckBox>
#include <QStackedWidget>
#include <QVariantAnimation>
#include <QImage>
#include <QColor>
#include <QJsonObject>
class QPainter;

namespace Visuals {
void install();
void configure(bool reduceMotion, bool reduceTransparency, int glassStrength = 80);
bool reducedMotion();
bool reducedTransparency();
int strength();
QColor mix(const QColor &from, const QColor &to, qreal progress);
QColor foreground(const QColor &background);
void chevron(QPainter &painter, const QRectF &rect, Qt::ArrowType direction, const QColor &color);
} // namespace Visuals

class GlassSwitch : public QCheckBox {
    Q_OBJECT
  public:
    explicit GlassSwitch(QWidget *parent = nullptr);
    QSize sizeHint() const override {
        return {48, 28};
    }
    QSize minimumSizeHint() const override {
        return sizeHint();
    }

  protected:
    void paintEvent(QPaintEvent *) override;
    bool hitButton(const QPoint &position) const override {
        return rect().contains(position);
    }

  private:
    QVariantAnimation motion;
    qreal position = 0;
};

class TransitionStack : public QStackedWidget {
    Q_OBJECT
  public:
    explicit TransitionStack(QWidget *parent = nullptr);
    void transitionTo(int index);

  private:
    QVariantAnimation motion;
};

// Blurs only a cached image of our own parent window, never other applications.
class GlassDialog : public QDialog {
    Q_OBJECT
  public:
    explicit GlassDialog(QWidget *parent = nullptr);
    ~GlassDialog() override;

  protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

  private:
    QWidget *backdrop = nullptr;
    QImage backdropImage;
    QVariantAnimation reveal;
    QPoint dragOffset;
    bool dragging = false;
};

// Returns an invalid color when cancelled.
QColor pickGlassColor(const QColor &initial, QWidget *parent);

// Public AppKit APIs; implemented as no-ops on Linux and Windows.
bool installNativeGlass(QWidget *widget);
bool systemReducesMotion();
bool systemReducesTransparency();
