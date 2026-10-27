#include "glassstyle.h"
#include "visuals.h"
#include <QtWidgets>
GlassStyle::GlassStyle() : QProxyStyle(QStyleFactory::create("Fusion")) {}
int GlassStyle::pixelMetric(PixelMetric m, const QStyleOption *o,
                            const QWidget *w) const {
  if (m == PM_IndicatorWidth || m == PM_IndicatorHeight ||
      m == PM_ExclusiveIndicatorWidth || m == PM_ExclusiveIndicatorHeight)
    return 18;
  if (m == PM_DefaultFrameWidth)
    return 0;
  if (m == PM_SplitterWidth)
    return 6;
  if (m == PM_ScrollBarExtent)
    return 12;
  return QProxyStyle::pixelMetric(m, o, w);
}
int GlassStyle::styleHint(StyleHint h, const QStyleOption *o, const QWidget *w,
                          QStyleHintReturn *r) const {
  if (h == SH_ComboBox_Popup)
    return false;
  if (h == SH_Widget_Animate)
    return !Visuals::reducedMotion();
  if (h == SH_UnderlineShortcut)
    return o && (o->state & State_KeyboardFocusChange);
  return QProxyStyle::styleHint(h, o, w, r);
}
void GlassStyle::drawPrimitive(PrimitiveElement e, const QStyleOption *o,
                               QPainter *p, const QWidget *w) const {
  if (e == PE_IndicatorCheckBox || e == PE_IndicatorItemViewItemCheck ||
      e == PE_IndicatorRadioButton) {
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    QRectF r = o->rect;
    r.setSize(QSizeF(17, 17));
    r.moveCenter(o->rect.center());
    r.adjust(.5, .5, -.5, -.5);
    const bool checked = o->state & (State_On | State_NoChange),
               enabled = o->state & State_Enabled;
    auto accent = o->palette.color(QPalette::Highlight);
    if (!enabled)
      accent.setAlpha(90);
    auto neutral = o->palette.color(QPalette::Text);
    neutral.setAlpha(enabled ? 85 : 38);
    const qreal transition = w && w->property("glassChecked").isValid()
                                 ? w->property("glassChecked").toReal()
                                 : (checked ? 1. : 0.);
    auto outline = neutral;
    outline.setAlphaF(outline.alphaF() * (1 - transition));
    p->setPen(QPen(outline, 1.3));
    p->setBrush(Visuals::mix(QColor(128, 128, 128, 22), accent, transition));
    if (e == PE_IndicatorRadioButton)
      p->drawEllipse(r);
    else
      p->drawRoundedRect(r, 5, 5);
    p->setPen(QPen(Visuals::foreground(accent), 1.8, Qt::SolidLine,
                   Qt::RoundCap, Qt::RoundJoin));
    if (o->state & State_NoChange)
      p->drawLine(r.center() + QPointF(-4, 0), r.center() + QPointF(4, 0));
    else if (checked || transition > .01) {
      p->setOpacity(transition);
      if (e == PE_IndicatorRadioButton) {
        p->setPen(Qt::NoPen);
        p->setBrush(Visuals::foreground(accent));
        p->drawEllipse(r.center(), 3, 3);
      } else {
        QPainterPath path;
        path.moveTo(r.left() + 3.5, r.center().y());
        path.lineTo(r.left() + 6.5, r.bottom() - 4);
        path.lineTo(r.right() - 3, r.top() + 4);
        p->drawPath(path);
      }
    }
    p->setOpacity(1);
    if (o->state & State_HasFocus) {
      p->setBrush(Qt::NoBrush);
      p->setPen(QPen(accent, 2));
      p->drawRoundedRect(r.adjusted(-2, -2, 2, 2), 6, 6);
    }
    p->restore();
    return;
  }
  if (e == PE_IndicatorBranch) {
    if (o->state & State_Children)
      Visuals::chevron(*p, QRectF(o->rect),
                       o->state & State_Open ? Qt::DownArrow : Qt::RightArrow,
                       o->palette.color(QPalette::PlaceholderText));
    return;
  }
  if (e == PE_FrameFocusRect) {
    if (o->state & State_KeyboardFocusChange) {
      p->save();
      p->setBrush(Qt::NoBrush);
      p->setPen(QPen(o->palette.color(QPalette::Highlight), 1.5));
      p->drawRoundedRect(o->rect.adjusted(1, 1, -1, -1), 6, 6);
      p->restore();
    }
    return;
  }
  if (e == PE_PanelItemViewItem) {
    if (o->state & (State_Selected | State_MouseOver)) {
      p->save();
      p->setRenderHint(QPainter::Antialiasing);
      auto c = o->palette.color(QPalette::Text);
      c.setAlpha(o->state & State_Selected ? 24 : 12);
      p->setPen(Qt::NoPen);
      p->setBrush(c);
      p->drawRoundedRect(o->rect.adjusted(3, 1, -3, -1), 7, 7);
      p->restore();
    }
    return;
  }
  if (e == PE_IndicatorArrowDown || e == PE_IndicatorArrowUp ||
      e == PE_IndicatorArrowLeft || e == PE_IndicatorArrowRight) {
    Visuals::chevron(*p, QRectF(o->rect),
                     e == PE_IndicatorArrowDown   ? Qt::DownArrow
                     : e == PE_IndicatorArrowUp   ? Qt::UpArrow
                     : e == PE_IndicatorArrowLeft ? Qt::LeftArrow
                                                  : Qt::RightArrow,
                     o->palette.color(QPalette::Text));
    return;
  }
  QProxyStyle::drawPrimitive(e, o, p, w);
}
void GlassStyle::drawControl(ControlElement e, const QStyleOption *o,
                             QPainter *p, const QWidget *w) const {
  if (e == CE_ItemViewItem) {
    auto item = qstyleoption_cast<const QStyleOptionViewItem *>(o);
    if (item) {
      auto copy = *item;
      copy.palette.setColor(QPalette::HighlightedText,
                            o->palette.color(QPalette::Text));
      QProxyStyle::drawControl(e, &copy, p, w);
      return;
    }
  }
  QProxyStyle::drawControl(e, o, p, w);
}
