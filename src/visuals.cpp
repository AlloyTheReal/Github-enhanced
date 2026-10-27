#include "visuals.h"
#include "glassstyle.h"
#include "capsulescrollbar.h"
#include <QtWidgets>
#include <cmath>

namespace {
bool reduceMotion = false, reduceTransparency = false;
int glassStrength = 80;
QColor alpha(QColor color, int opacity) {
    color.setAlpha(opacity);
    return color;
}
QColor text() {
    return qApp->palette().color(QPalette::Text);
}

void animate(QWidget *widget, const char *property, qreal target, int duration) {
    const auto name = QString::fromLatin1(property) + "Animation";
    auto *motion = widget->findChild<QVariantAnimation *>(name, Qt::FindDirectChildrenOnly);
    if (!motion) {
        motion = new QVariantAnimation(widget);
        motion->setObjectName(name);
        const QByteArray key(property);
        QObject::connect(motion, &QVariantAnimation::valueChanged, widget, [widget, key](const QVariant &v) {
            widget->setProperty(key, v);
            widget->update();
        });
    }
    motion->stop();
    if (Visuals::reducedMotion()) {
        widget->setProperty(property, target);
        widget->update();
        return;
    }
    motion->setStartValue(widget->property(property).toReal());
    motion->setEndValue(target);
    motion->setDuration(duration);
    motion->setEasingCurve(QEasingCurve::OutCubic);
    motion->start();
}

class ControlFilter : public QObject {
  public:
    using QObject::QObject;
    bool eventFilter(QObject *object, QEvent *event) override {
        auto *widget = qobject_cast<QWidget *>(object);
        if (!widget)
            return false;
        if(event->type()==QEvent::Polish || event->type()==QEvent::Show || event->type()==QEvent::FontChange) {
            if(auto *check=qobject_cast<QCheckBox *>(widget))
                check->setMinimumHeight(qMax(30,check->fontMetrics().height()+12));
            if(auto *check=qobject_cast<QCheckBox *>(widget);check&&!check->property("glassCheckInstalled").toBool()){
                check->setProperty("glassCheckInstalled",true);check->setProperty("glassChecked",check->isChecked()?1.:0.);
                QObject::connect(check,&QCheckBox::toggled,check,[check](bool on){animate(check,"glassChecked",on?1.:0.,150);});
            }
            if(auto *area=qobject_cast<QAbstractScrollArea *>(widget))installCapsuleScrollBars(area);
            if(auto *view=qobject_cast<QAbstractItemView *>(widget)) {
                view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
                view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
            }
        }
        if(event->type()==QEvent::Wheel && !Visuals::reducedMotion()) {
            auto *wheel=static_cast<QWheelEvent *>(event);
            auto *area=qobject_cast<QAbstractScrollArea *>(widget->parentWidget());
            if(area && area->viewport()==widget && wheel->pixelDelta().isNull() && !(wheel->modifiers()&Qt::ControlModifier)) {
                const bool horizontal=wheel->modifiers()&Qt::ShiftModifier || (wheel->angleDelta().y()==0 && wheel->angleDelta().x()!=0);
                auto *bar=horizontal?area->horizontalScrollBar():area->verticalScrollBar();
                const int delta=horizontal && wheel->angleDelta().x()!=0 ? wheel->angleDelta().x() : wheel->angleDelta().y();
                if(bar->maximum()>bar->minimum() && delta && !bar->isSliderDown()) {
                    auto *motion=bar->findChild<QVariantAnimation *>("smoothWheel",Qt::FindDirectChildrenOnly);
                    if(!motion) {
                        motion=new QVariantAnimation(bar);motion->setObjectName("smoothWheel");
                        QObject::connect(motion,&QVariantAnimation::valueChanged,bar,[bar](const QVariant &value){bar->setValue(value.toInt());});
                        QObject::connect(bar,&QScrollBar::sliderPressed,motion,&QVariantAnimation::stop);
                    }
                    int target=motion->state()==QAbstractAnimation::Running?motion->endValue().toInt():bar->value();
                    target=qBound(bar->minimum(),target-qRound(delta/120.0*bar->singleStep()*QApplication::wheelScrollLines()),bar->maximum());
                    if(target!=bar->value()) {
                        motion->stop();motion->setStartValue(bar->value());motion->setEndValue(target);motion->setDuration(140);motion->setEasingCurve(QEasingCurve::OutCubic);motion->start();wheel->accept();return true;
                    }
                }
            }
        }
        auto *button = qobject_cast<QAbstractButton *>(widget);
        auto *combo = qobject_cast<QComboBox *>(widget);
        auto *spin = qobject_cast<QAbstractSpinBox *>(widget);
        const bool push = qobject_cast<QPushButton *>(widget) || qobject_cast<QToolButton *>(widget);
        if (!push && !combo && !spin)
            return false;
        if (event->type() == QEvent::Enter)
            animate(widget, "glassHover", 1, 180);
        if (event->type() == QEvent::Leave) {
            animate(widget, "glassHover", 0, 200);
            animate(widget, "glassPress", 0, 130);
        }
        if (event->type() == QEvent::MouseButtonPress && widget->isEnabled())
            animate(widget, "glassPress", 1, 90);
        if (event->type() == QEvent::MouseButtonRelease)
            animate(widget, "glassPress", 0, 160);
        if (event->type() != QEvent::Paint)
            return false;
        if (combo && combo->isEditable())
            return false;
        QPainter p(widget);
        p.setRenderHint(QPainter::Antialiasing);
        const auto palette = widget->palette();
        const bool dark = palette.color(QPalette::Base).lightness() < 128;
        const qreal hover = Visuals::reducedMotion() ? (widget->underMouse() ? 1. : 0.)
                                                     : widget->property("glassHover").toReal();
        const qreal press = Visuals::reducedMotion() ? (button && button->isDown() ? 1. : 0.)
                                                     : widget->property("glassPress").toReal();
        QRectF frame = widget->rect().adjusted(1, 1, -1, -1);
        if (widget->width() < 22 || widget->height() < 18)
            return false;
        const qreal radius = qMin(qreal(11), frame.height() / 2);
        if (combo) {
            const bool inToolbar =
                combo->parentWidget() && combo->parentWidget()->objectName() == "toolbarBlock";
            QColor fill = inToolbar ? alpha(palette.color(QPalette::Button), int(28 + hover * 32))
                                    : palette.color(QPalette::Button);
            fill = Visuals::mix(fill, palette.color(QPalette::Mid), press * .12);
            p.setBrush(fill);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(frame, radius, radius);
            const auto color = combo->isEnabled() ? text() : palette.color(QPalette::PlaceholderText);
            p.setPen(color);
            p.setFont(combo->font());
            const QRect content = widget->rect().adjusted(12, 0, -36, 0);
            p.drawText(
                content, Qt::AlignVCenter | Qt::AlignLeft,
                QFontMetrics(combo->font())
                    .elidedText(combo->currentIndex() < 0 ? combo->placeholderText() : combo->currentText(),
                                Qt::ElideRight, content.width()));
            Visuals::chevron(p, QRectF(widget->width() - 27, widget->height() / 2. - 6, 12, 12),
                             Qt::DownArrow, color);
        } else if (spin) {
            p.setBrush(palette.color(QPalette::Button));
            p.setPen(QPen(alpha(text(), 28), 1));
            p.drawRoundedRect(frame, radius, radius);
            if (spin->buttonSymbols() != QAbstractSpinBox::NoButtons) {
                QStyleOptionSpinBox option;
                option.initFrom(spin);
                option.buttonSymbols = spin->buttonSymbols();
                option.frame = true;
                option.stepEnabled = QAbstractSpinBox::StepUpEnabled | QAbstractSpinBox::StepDownEnabled;
                const auto up =
                    spin->style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxUp, spin);
                const auto down =
                    spin->style()->subControlRect(QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxDown, spin);
                const auto color = spin->isEnabled() ? text() : palette.color(QPalette::PlaceholderText);
                Visuals::chevron(p, QRectF(up.center().x() - 6, up.center().y() - 6, 12, 12), Qt::UpArrow,
                                 color);
                Visuals::chevron(p, QRectF(down.center().x() - 6, down.center().y() - 6, 12, 12),
                                 Qt::DownArrow, color);
            }
        } else if (button) {
            const auto *tool = qobject_cast<QToolButton *>(widget);
            const bool flat = tool || (qobject_cast<QPushButton *>(widget) &&
                                       qobject_cast<QPushButton *>(widget)->isFlat());
            const bool primary = widget->objectName() == "primary" || button->isChecked();
            const auto accent = palette.color(QPalette::Highlight);
            QColor fill = primary ? accent : palette.color(QPalette::Button);
            fill =
                Visuals::mix(fill, dark ? QColor("#ffffff") : QColor("#000000"), hover * (dark ? .07 : .035));
            fill =
                Visuals::mix(fill, dark ? QColor("#ffffff") : QColor("#000000"), press * (dark ? .06 : .06));
            if (flat && !primary)
                fill.setAlpha(int(hover * 24 + press * 24 + (widget->property("recentEntry").toBool() && widget->hasFocus() ? 24 : 0)));
            if (!widget->isEnabled()) {
                fill = palette.color(QPalette::Button);
                if (flat)
                    fill.setAlpha(0);
            }
            p.setBrush(fill);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(frame, radius, radius);
            // The fine upper edge gives the material depth without a permanent glow.
            if (press > .02) {
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(alpha(Qt::white, dark ? 18 : 110), .7));
                p.drawRoundedRect(frame.adjusted(.5, .5, -.5, -.5), radius - .5, radius - .5);
            }
            QColor foreground = primary ? Visuals::foreground(accent) : palette.color(QPalette::ButtonText);
            if (!widget->isEnabled())
                foreground = palette.color(QPalette::PlaceholderText);
            const auto swatch = widget->property("swatchColor");
            if (widget->property("recentRemove").toBool()) {
                const auto center = frame.center();
                p.setPen(QPen(foreground, 1.6, Qt::SolidLine, Qt::RoundCap));
                p.drawLine(center + QPointF(-4,-4), center + QPointF(4,4));
                p.drawLine(center + QPointF(-4,4), center + QPointF(4,-4));
            } else if (swatch.isValid()) {
                p.setBrush(swatch.value<QColor>());
                p.setPen(QPen(alpha(text(), 45), 1));
                p.drawEllipse(frame.center(), 10, 10);
            } else {
                const int inset = widget->width() < 44 ? 4 : 12;
                QRect content = widget->rect().adjusted(inset, 0, -inset, 0);
                const bool menu = tool && tool->menu();
                if (menu)
                    content.adjust(0, 0, -16, 0);
                const bool iconOnly = tool && tool->toolButtonStyle() == Qt::ToolButtonIconOnly;
                const bool textOnly = tool && tool->toolButtonStyle() == Qt::ToolButtonTextOnly;
                QString title = button->text();
                title.remove('&');
                QFont font = button->font();
                if (primary)
                    font.setWeight(QFont::Medium);
                p.setFont(font);
                int iconSize = button->icon().isNull() || textOnly ? 0 : 16;
                int labelSize = iconOnly ? 0 : QFontMetrics(font).horizontalAdvance(title);
                const int total = iconSize + (iconSize && labelSize ? 7 : 0) + labelSize;
                int x = content.left() + qMax(0, (content.width() - total) / 2);
                if (menu)
                    x = content.left();
                if (iconSize) {
                    button->icon().paint(&p, QRect(x, widget->height() / 2 - 8, 16, 16), Qt::AlignCenter,
                                         widget->isEnabled() ? QIcon::Normal : QIcon::Disabled);
                    x += iconSize + (labelSize ? 7 : 0);
                }
                p.setPen(foreground);
                if (!iconOnly)
                    p.drawText(QRect(x, 0, qMax(0, content.right() - x + 1), widget->height()),
                               Qt::AlignVCenter | Qt::AlignLeft,
                               QFontMetrics(font).elidedText(title, Qt::ElideRight,
                                                             qMax(0, content.right() - x + 1)));
                if (menu)
                    Visuals::chevron(p, QRectF(widget->width() - 23, widget->height() / 2. - 6, 12, 12),
                                     Qt::DownArrow, foreground);
                else if (tool && tool->arrowType() != Qt::NoArrow)
                    Visuals::chevron(p, QRectF(widget->width() / 2. - 6, widget->height() / 2. - 6, 12, 12),
                                     tool->arrowType(), foreground);
            }
        }
        if (widget->hasFocus() && widget->isEnabled() && !widget->property("recentEntry").toBool()) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(alpha(palette.color(QPalette::Highlight), 150), 2));
            p.drawRoundedRect(frame.adjusted(1, 1, -1, -1), radius, radius);
        }
        return true;
    }
};

QImage blur(const QPixmap &source) {
    if (source.isNull())
        return {};
    QImage small = source.toImage().scaled(source.width() / 2, source.height() / 2, Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation);
    small.setDevicePixelRatio(1);
    QGraphicsScene scene;
    auto *item = scene.addPixmap(QPixmap::fromImage(small));
    auto *effect = new QGraphicsBlurEffect;
    effect->setBlurRadius(18);
    effect->setBlurHints(QGraphicsBlurEffect::QualityHint);
    item->setGraphicsEffect(effect);
    QImage result(small.size(), QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    scene.render(&painter, QRectF(QPointF(), result.size()), QRectF(QPointF(), small.size()));
    return result;
}

class Backdrop : public QWidget {
  public:
    QImage image;
    qreal opacity = 0;
    explicit Backdrop(QWidget *parent) : QWidget(parent) {
        setObjectName("dialogBackdrop");
        setAttribute(Qt::WA_NoSystemBackground);
    }

  protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setOpacity(Visuals::reducedMotion() ? 1 : opacity);
        if (!Visuals::reducedTransparency() && !image.isNull()) {
            p.setRenderHint(QPainter::SmoothPixmapTransform);
            p.drawImage(rect(), image);
        }
        p.fillRect(rect(), QColor(0, 0, 0, Visuals::reducedTransparency() ? 100 : 65));
    }
};

class SaturationPlane : public QWidget {
  public:
    QColor color;
    std::function<void(const QColor &)> changed;
    explicit SaturationPlane(const QColor &c, QWidget *parent) : QWidget(parent), color(c) {
        setMinimumSize(260, 180);
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::CrossCursor);
        setAccessibleName("Saturation et luminosité, ajustables avec les flèches");
    }

  protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = rect().adjusted(2, 2, -2, -2);
        QPainterPath path;
        path.addRoundedRect(r, 12, 12);
        p.setClipPath(path);
        p.fillRect(r, QColor::fromHsvF(qMax(0.f, color.hsvHueF()), 1, 1));
        QLinearGradient white(r.topLeft(), r.topRight());
        white.setColorAt(0, Qt::white);
        white.setColorAt(1, Qt::transparent);
        p.fillRect(r, white);
        QLinearGradient black(r.topLeft(), r.bottomLeft());
        black.setColorAt(0, Qt::transparent);
        black.setColorAt(1, Qt::black);
        p.fillRect(r, black);
        p.setClipping(false);
        const QPointF point(r.left() + color.hsvSaturationF() * r.width(),
                            r.bottom() - color.valueF() * r.height());
        p.setPen(QPen(QColor(0, 0, 0, 90), 4));
        p.setBrush(color);
        p.drawEllipse(point, 7, 7);
        p.setPen(QPen(Qt::white, 2));
        p.drawEllipse(point, 7, 7);
        if (hasFocus()) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(palette().color(QPalette::Highlight), 2));
            p.drawRoundedRect(r, 12, 12);
        }
    }
    void mousePressEvent(QMouseEvent *e) override {
        setFocus();
        select(e->position());
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        if (e->buttons() & Qt::LeftButton)
            select(e->position());
    }
    void keyPressEvent(QKeyEvent *e) override {
        qreal s = color.hsvSaturationF(), v = color.valueF();
        const qreal step = e->modifiers() & Qt::ShiftModifier ? .1 : .01;
        if (e->key() == Qt::Key_Left)
            s -= step;
        else if (e->key() == Qt::Key_Right)
            s += step;
        else if (e->key() == Qt::Key_Up)
            v += step;
        else if (e->key() == Qt::Key_Down)
            v -= step;
        else {
            QWidget::keyPressEvent(e);
            return;
        }
        color = QColor::fromHsvF(qMax(0.f, color.hsvHueF()), qBound(0., s, 1.), qBound(0., v, 1.));
        update();
        if (changed)
            changed(color);
    }

  private:
    void select(const QPointF &point) {
        color = QColor::fromHsvF(qMax(0.f, color.hsvHueF()), qBound(0., (point.x() - 2) / (width() - 4), 1.),
                                 qBound(0., 1 - (point.y() - 2) / (height() - 4), 1.));
        update();
        if (changed)
            changed(color);
    }
};
} // namespace

void Visuals::install() {
    if (qApp->property("glassControlsInstalled").toBool())
        return;
    qApp->setProperty("glassControlsInstalled", true);
    qApp->setStyle(new GlassStyle);
    qApp->installEventFilter(new ControlFilter(qApp));
}
void Visuals::configure(bool motion, bool transparency, int strength) {
    reduceMotion = motion;
    reduceTransparency = transparency;
    glassStrength = qBound(40, strength, 100);
    if (Visuals::reducedMotion())
        for (auto *widget : QApplication::allWidgets()) {
            if (qobject_cast<TransitionStack *>(widget))
                if (auto *effect = qobject_cast<QGraphicsOpacityEffect *>(widget->graphicsEffect()))
                    effect->setOpacity(1);
        }
    for (auto *widget : QApplication::topLevelWidgets())
        widget->update();
}
bool Visuals::reducedMotion() {
    return reduceMotion || systemReducesMotion();
}
bool Visuals::reducedTransparency() {
    return reduceTransparency || systemReducesTransparency();
}
int Visuals::strength() {
    return glassStrength;
}
QColor Visuals::mix(const QColor &a, const QColor &b, qreal t) {
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t,
                            a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}
QColor Visuals::foreground(const QColor &c) {
    return .2126 * c.redF() + .7152 * c.greenF() + .0722 * c.blueF() > .6 ? QColor("#19181b")
                                                                          : QColor("#ffffff");
}
void Visuals::chevron(QPainter &p, const QRectF &r, Qt::ArrowType direction, const QColor &color) {
    p.save();
    p.translate(r.center());
    if (direction == Qt::UpArrow)
        p.rotate(180);
    else if (direction == Qt::LeftArrow)
        p.rotate(90);
    else if (direction == Qt::RightArrow)
        p.rotate(-90);
    p.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    QPainterPath path;
    path.moveTo(-4, -1.5);
    path.lineTo(0, 2.5);
    path.lineTo(4, -1.5);
    p.drawPath(path);
    p.restore();
}

GlassSwitch::GlassSwitch(QWidget *parent) : QCheckBox(parent) {
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    motion.setDuration(180);
    motion.setEasingCurve(QEasingCurve::OutCubic);
    connect(&motion, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        position = v.toReal();
        update();
    });
    connect(this, &QCheckBox::toggled, this, [this](bool checked) {
        motion.stop();
        if (Visuals::reducedMotion() || !isVisible()) {
            position = checked ? 1 : 0;
            update();
            return;
        }
        motion.setStartValue(position);
        motion.setEndValue(checked ? 1. : 0.);
        motion.start();
    });
}
void GlassSwitch::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto accent = palette().color(QPalette::Highlight);
    const qreal progress = Visuals::reducedMotion() ? (isChecked() ? 1. : 0.) : position;
    QColor fill = Visuals::mix(palette().color(QPalette::Mid), accent, progress);
    if (!isEnabled())
        fill.setAlpha(90);
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawRoundedRect(QRectF(1, 2, 46, 24), 12, 12);
    p.setBrush(Visuals::mix(QColor("#f6f5f7"), Visuals::foreground(accent), progress));
    p.drawEllipse(QRectF(4 + 22 * progress, 5, 18, 18));
    if (hasFocus()) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(accent, 1.5));
        p.drawRoundedRect(QRectF(0, 1, 47, 26), 13, 13);
    }
}

TransitionStack::TransitionStack(QWidget *parent) : QStackedWidget(parent) {
    auto *effect = new QGraphicsOpacityEffect(this);
    effect->setOpacity(1);
    setGraphicsEffect(effect);
    motion.setDuration(160);
    motion.setEasingCurve(QEasingCurve::OutCubic);
    connect(&motion, &QVariantAnimation::valueChanged, effect,
            [effect](const QVariant &v) { effect->setOpacity(Visuals::reducedMotion() ? 1 : v.toReal()); });
}
void TransitionStack::transitionTo(int index) {
    if (index == currentIndex())
        return;
    motion.stop();
    setCurrentIndex(index);
    if (Visuals::reducedMotion() || !isVisible()) {
        static_cast<QGraphicsOpacityEffect *>(graphicsEffect())->setOpacity(1);
        return;
    }
    motion.setStartValue(.15);
    motion.setEndValue(1.);
    motion.start();
}

GlassDialog::GlassDialog(QWidget *parent) : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint) {
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);
    reveal.setDuration(180);
    reveal.setEasingCurve(QEasingCurve::OutCubic);
    connect(&reveal, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        if (auto *overlay = dynamic_cast<Backdrop *>(backdrop)) {
            overlay->opacity = v.toReal();
            overlay->update();
        }
    });
}
GlassDialog::~GlassDialog() {
    delete backdrop;
}
void GlassDialog::showEvent(QShowEvent *event) {
    const auto availableSize = screen()->availableGeometry().size() - QSize(16, 16);
    resize(qMin(width(), availableSize.width()), qMin(height(), availableSize.height()));
    if (parentWidget() && isModal()) {
        delete backdrop;
        backdrop = nullptr;
        backdropImage = Visuals::reducedTransparency() ? QImage{} : blur(parentWidget()->grab());
        auto *overlay = new Backdrop(parentWidget());
        overlay->image = backdropImage;
        overlay->setGeometry(parentWidget()->rect());
        overlay->show();
        overlay->raise();
        backdrop = overlay;
        const auto center = parentWidget()->mapToGlobal(parentWidget()->rect().center());
        QRect available = screen()->availableGeometry();
        QPoint position = center - QPoint(width() / 2, height() / 2);
        position.setX(qBound(available.left() + 8, position.x(),
                             qMax(available.left() + 8, available.right() - width() - 8)));
        position.setY(qBound(available.top() + 8, position.y(),
                             qMax(available.top() + 8, available.bottom() - height() - 8)));
        move(position);
        if (Visuals::reducedMotion())
            overlay->opacity = 1;
        else {
            reveal.setStartValue(0.);
            reveal.setEndValue(1.);
            reveal.start();
        }
    }
    QDialog::showEvent(event);
    if (!Visuals::reducedTransparency() && QSettings().value("native", true).toBool())
        installNativeGlass(this);
}
void GlassDialog::hideEvent(QHideEvent *event) {
    reveal.stop();
    if (backdrop)
        backdrop->hide();
    QDialog::hideEvent(event);
}
void GlassDialog::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = rect().adjusted(1, 1, -1, -1);
    QPainterPath shape;
    shape.addRoundedRect(r, 24, 24);
    p.setClipPath(shape);
    const auto surface = palette().color(QPalette::Base);
    if (!Visuals::reducedTransparency() && !backdropImage.isNull() && parentWidget()) {
        const QPoint origin = parentWidget()->mapFromGlobal(mapToGlobal(QPoint()));
        const double sx = double(backdropImage.width()) / parentWidget()->width(),
                     sy = double(backdropImage.height()) / parentWidget()->height();
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawImage(r, backdropImage, QRectF(origin.x() * sx, origin.y() * sy, width() * sx, height() * sy));
    } else
        p.fillRect(r, surface);
    QLinearGradient gradient(r.topLeft(), r.bottomRight());
    const int opacity = Visuals::reducedTransparency() ? 255 : 155 + Visuals::strength();
    gradient.setColorAt(0, alpha(Visuals::mix(surface, QColor("#9a8798"), .13), opacity));
    gradient.setColorAt(1, alpha(surface, opacity + 10 > 255 ? 255 : opacity + 10));
    p.fillPath(shape, gradient);
    p.setClipping(false);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(alpha(palette().color(QPalette::Text), 25), 1));
    p.drawPath(shape);
}
void GlassDialog::mousePressEvent(QMouseEvent *event) {
    dragging = event->button() == Qt::LeftButton && event->position().y() < 60;
    if (dragging) {
        dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        if (windowHandle())
            windowHandle()->startSystemMove();
    }
    QDialog::mousePressEvent(event);
}
void GlassDialog::mouseMoveEvent(QMouseEvent *event) {
    if (dragging && event->buttons() & Qt::LeftButton)
        move(event->globalPosition().toPoint() - dragOffset);
    QDialog::mouseMoveEvent(event);
}
void GlassDialog::mouseReleaseEvent(QMouseEvent *event) {
    dragging = false;
    QDialog::mouseReleaseEvent(event);
}
void GlassDialog::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape)
        reject();
    else
        QDialog::keyPressEvent(event);
}

QColor pickGlassColor(const QColor &initial, QWidget *parent) {
    GlassDialog dialog(parent);
    dialog.setObjectName("colorPicker");
    dialog.setWindowTitle("Choisir une couleur");
    dialog.resize(380, 430);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    auto *title = new QLabel("Choisir une couleur");
    title->setObjectName("pageTitle");
    layout->addWidget(title);
    auto *plane = new SaturationPlane(initial, &dialog);
    plane->setObjectName("colorPlane");
    layout->addWidget(plane, 1);
    auto *hue = new QSlider(Qt::Horizontal);
    hue->setObjectName("hueSlider");
    hue->setRange(0, 359);
    hue->setValue(qMax(0, initial.hsvHue()));
    hue->setAccessibleName("Teinte");
    layout->addWidget(hue);
    auto *row = new QHBoxLayout;
    auto *swatch = new QPushButton;
    swatch->setProperty("swatchColor", initial);
    swatch->setFixedSize(42, 36);
    swatch->setEnabled(false);
    row->addWidget(swatch);
    auto *hex = new QLineEdit(initial.name().toUpper());
    hex->setObjectName("colorHex");
    hex->setAccessibleName("Couleur hexadécimale");
    row->addWidget(hex);
    layout->addLayout(row);
    auto *chips = new QHBoxLayout;
    for (const auto &value : {"#d4d0d5", "#8ea5db", "#ad9bd8", "#cf9cac", "#c8ad85", "#8db9a3"}) {
        auto *chip = new QPushButton;
        chip->setProperty("swatchColor", QColor(value));
        chip->setFixedSize(40, 32);
        chip->setToolTip(value);
        chip->setAccessibleName(value);
        chips->addWidget(chip);
        QObject::connect(chip, &QPushButton::clicked, &dialog, [=] {
            const QColor color(value);
            plane->color = color;
            plane->update();
            hue->setValue(color.hsvHue());
            hex->setText(color.name().toUpper());
        });
    }
    layout->addLayout(chips);
    QColor selected = initial;
    plane->changed = [&](const QColor &color) {
        selected = color;
        const QSignalBlocker block(hex);
        hex->setText(color.name().toUpper());
        swatch->setProperty("swatchColor", color);
        swatch->update();
    };
    QObject::connect(hue, &QSlider::valueChanged, &dialog, [=, &selected](int value) {
        selected = QColor::fromHsv(value, plane->color.hsvSaturation(), plane->color.value());
        plane->color = selected;
        plane->update();
        hex->setText(selected.name().toUpper());
    });
    auto *box = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
    layout->addWidget(box);
    box->button(QDialogButtonBox::Ok)->setText("Choisir");
    box->button(QDialogButtonBox::Ok)->setObjectName("primary");
    box->button(QDialogButtonBox::Ok)->setIcon({});
    box->button(QDialogButtonBox::Cancel)->setText("Annuler");
    box->button(QDialogButtonBox::Cancel)->setIcon({});
    QObject::connect(hex, &QLineEdit::textChanged, &dialog, [=, &selected](const QString &value) {
        const bool valid = QRegularExpression("^#[0-9a-fA-F]{6}$").match(value).hasMatch();
        box->button(QDialogButtonBox::Ok)->setEnabled(valid);
        if (!valid)
            return;
        selected = QColor(value);
        plane->color = selected;
        plane->update();
        swatch->setProperty("swatchColor", selected);
        swatch->update();
        const QSignalBlocker block(hue);
        hue->setValue(qMax(0, selected.hsvHue()));
    });
    QObject::connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    return dialog.exec() == QDialog::Accepted ? selected : QColor{};
}

#ifndef Q_OS_MACOS
bool installNativeGlass(QWidget *) {
    return false;
}
bool systemReducesMotion() {
    return false;
}
bool systemReducesTransparency() {
    return false;
}
#endif

GlassComboBox::GlassComboBox(QWidget *parent) : QComboBox(parent) {
    auto *list = new QListView;
    list->setObjectName("glassComboList");
    list->setFrameShape(QFrame::NoFrame);
    list->setUniformItemSizes(true);
    list->setTextElideMode(Qt::ElideRight);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setView(list);
    setMaxVisibleItems(8);
    setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    setMinimumContentsLength(12);
    installCapsuleScrollBars(list);
}
void GlassComboBox::showPopup() {
    QComboBox::showPopup();
    auto *popup = view()->window();
    if (!popup || popup == window() || !popup->isVisible()) return;
    if (popup->objectName() != "glassComboPopup") {
        popup->setObjectName("glassComboPopup"); popup->style()->unpolish(popup); popup->style()->polish(popup);
    }
    QPainterPath shape;
    shape.addRoundedRect(QRectF(popup->rect()), 12, 12);
    popup->setMask(QRegion(shape.toFillPolygon().toPolygon()));
    if (popupAnimation) { popupAnimation->stop(); popupAnimation->deleteLater(); }
    auto *fade = qobject_cast<QGraphicsOpacityEffect *>(view()->graphicsEffect());
    if (!fade) { fade = new QGraphicsOpacityEffect(view()); view()->setGraphicsEffect(fade); }
    if (Visuals::reducedMotion()) { fade->setOpacity(1); return; }
    popupAnimation = new QPropertyAnimation(fade, "opacity", this);
    popupAnimation->setDuration(120);
    popupAnimation->setStartValue(.8); popupAnimation->setEndValue(1.);
    popupAnimation->setEasingCurve(QEasingCurve::OutCubic);
    popupAnimation->start();
}
void GlassComboBox::hidePopup() {
    if (popupAnimation) popupAnimation->stop();
    if (auto *fade = qobject_cast<QGraphicsOpacityEffect *>(view()->graphicsEffect())) fade->setOpacity(1);
    QComboBox::hidePopup();
}

QString glassInputText(QWidget *parent,const QString &title,const QString &label,QLineEdit::EchoMode mode,const QString &text,bool *ok){
    GlassDialog dialog(parent);dialog.setWindowTitle(title);dialog.resize(500,230);auto *root=new QVBoxLayout(&dialog);root->setContentsMargins(26,26,26,22);root->setSpacing(14);auto *heading=new QLabel(title);heading->setObjectName("pageTitle");root->addWidget(heading);auto *caption=new QLabel(label);caption->setWordWrap(true);root->addWidget(caption);auto *field=new QLineEdit(text);field->setObjectName("inputValue");field->setEchoMode(mode);root->addWidget(field);auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Ok)->setText("Valider");buttons->button(QDialogButtonBox::Cancel)->setText("Annuler");root->addWidget(buttons);QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);field->selectAll();field->setFocus();const bool accepted=dialog.exec()==QDialog::Accepted;if(ok)*ok=accepted;return accepted?field->text():QString();
}
