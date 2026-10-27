#pragma once
#include <QProxyStyle>
class GlassStyle : public QProxyStyle {
public:
  GlassStyle();
  void drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                     QPainter *painter,
                     const QWidget *widget = nullptr) const override;
  void drawControl(ControlElement element, const QStyleOption *option,
                   QPainter *painter,
                   const QWidget *widget = nullptr) const override;
  int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                  const QWidget *widget = nullptr) const override;
  int styleHint(StyleHint hint, const QStyleOption *option = nullptr,
                const QWidget *widget = nullptr,
                QStyleHintReturn *data = nullptr) const override;
};
