#pragma once

#include <QRectF>
#include <QString>
#include <QWidget>

#include "sure_smartie/core/Types.hpp"

class QPainter;

namespace sure_smartie::gui {

struct LcdPreviewWidgetTestAccess;

class LcdPreviewWidget : public QWidget {
 public:
  explicit LcdPreviewWidget(QWidget* parent = nullptr);

  void setFrame(core::DisplayGeometry geometry, core::Frame frame);
  const core::Frame& frame() const;
  QString lineText(int row) const;

  QSize minimumSizeHint() const override;

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  friend struct LcdPreviewWidgetTestAccess;

  static QRectF contentRectForCell(const QRectF& cell_rect);
  static qreal fittedGlyphPointSize(const QRectF& cell_rect);
  void paintGlyph(QPainter& painter, const QRectF& cell_rect, char symbol) const;

  core::DisplayGeometry geometry_;
  core::Frame frame_;
};

}  // namespace sure_smartie::gui
