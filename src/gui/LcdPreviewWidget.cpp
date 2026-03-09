#include "sure_smartie/gui/LcdPreviewWidget.hpp"

#include <algorithm>
#include <cctype>

#include <QFontDatabase>
#include <QPaintEvent>
#include <QPainter>
#include <QSizePolicy>

namespace sure_smartie::gui {

LcdPreviewWidget::LcdPreviewWidget(QWidget* parent)
    : QWidget(parent),
      geometry_({.cols = 20, .rows = 4}),
      frame_(geometry_.rows, std::string(geometry_.cols, ' ')) {
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void LcdPreviewWidget::setFrame(core::DisplayGeometry geometry, core::Frame frame) {
  geometry_ = geometry;
  frame_ = std::move(frame);
  frame_.resize(geometry_.rows, std::string(geometry_.cols, ' '));
  for (auto& line : frame_) {
    if (line.size() < geometry_.cols) {
      line.resize(geometry_.cols, ' ');
    } else if (line.size() > geometry_.cols) {
      line.resize(geometry_.cols);
    }
  }
  update();
}

const core::Frame& LcdPreviewWidget::frame() const { return frame_; }

QString LcdPreviewWidget::lineText(int row) const {
  if (row < 0 || static_cast<std::size_t>(row) >= frame_.size()) {
    return {};
  }

  return QString::fromStdString(frame_[static_cast<std::size_t>(row)]);
}

QSize LcdPreviewWidget::minimumSizeHint() const { return QSize(760, 300); }

void LcdPreviewWidget::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.fillRect(rect(), QColor("#f4efe2"));

  const QRectF outer_rect = rect().adjusted(8.0, 8.0, -8.0, -8.0);
  painter.setPen(QPen(QColor("#d1c3a3"), 2.0));
  painter.setBrush(QColor("#efe3c6"));
  painter.drawRoundedRect(outer_rect, 18.0, 18.0);

  const QRectF screen_rect = outer_rect.adjusted(20.0, 20.0, -20.0, -20.0);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor("#2f4b36"));
  painter.drawRoundedRect(screen_rect, 12.0, 12.0);

  const QRectF grid_rect = screen_rect.adjusted(14.0, 14.0, -14.0, -14.0);
  const qreal col_spacing = 4.0;
  const qreal row_spacing = 6.0;
  const qreal cell_width =
      std::max<qreal>(8.0,
                      (grid_rect.width() -
                       col_spacing * static_cast<qreal>(geometry_.cols - 1)) /
                          static_cast<qreal>(geometry_.cols));
  const qreal cell_height =
      std::max<qreal>(18.0,
                      (grid_rect.height() -
                       row_spacing * static_cast<qreal>(geometry_.rows - 1)) /
                          static_cast<qreal>(geometry_.rows));

  QFont cell_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  cell_font.setBold(true);
  cell_font.setPointSizeF(std::max<qreal>(9.0, cell_height * 0.54));
  painter.setFont(cell_font);

  for (std::size_t row = 0; row < geometry_.rows; ++row) {
    const auto& line = row < frame_.size() ? frame_[row] : std::string{};
    for (std::size_t col = 0; col < geometry_.cols; ++col) {
      const qreal x = grid_rect.left() +
                      static_cast<qreal>(col) * (cell_width + col_spacing);
      const qreal y = grid_rect.top() +
                      static_cast<qreal>(row) * (cell_height + row_spacing);
      const QRectF cell_rect(x, y, cell_width, cell_height);

      painter.setPen(QPen(QColor(255, 255, 255, 20), 1.0));
      painter.setBrush(QColor(0, 0, 0, 24));
      painter.drawRoundedRect(cell_rect, 4.0, 4.0);

      const char symbol = col < line.size() ? line[col] : ' ';
      paintGlyph(painter, cell_rect.adjusted(2.0, 2.0, -2.0, -2.0), symbol);
    }
  }
}

void LcdPreviewWidget::paintGlyph(QPainter& painter,
                                  const QRectF& cell_rect,
                                  char symbol) const {
  const auto code = static_cast<unsigned char>(symbol);
  if (code >= 1 && code <= 5) {
    const qreal fraction = static_cast<qreal>(code) / 5.0;
    const QRectF fill_rect(
        cell_rect.left(),
        cell_rect.top(),
        std::max<qreal>(3.0, cell_rect.width() * fraction),
        cell_rect.height());
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#d5f08a"));
    painter.drawRoundedRect(fill_rect, 2.0, 2.0);
    return;
  }

  if (symbol == ' ') {
    return;
  }

  const QChar glyph =
      (code >= 0x20 && code <= 0x7E && std::isprint(code)) ? QChar(symbol) : QChar('?');

  painter.setPen(QColor("#d5f08a"));
  painter.drawText(cell_rect, Qt::AlignCenter, QString(glyph));
}

}  // namespace sure_smartie::gui
