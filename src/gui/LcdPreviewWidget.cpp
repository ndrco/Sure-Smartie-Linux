#include "sure_smartie/gui/LcdPreviewWidget.hpp"

#include <algorithm>
#include <cctype>

#include <QFontDatabase>
#include <QFontMetricsF>
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

QRectF LcdPreviewWidget::contentRectForCell(const QRectF& cell_rect) {
  const qreal horizontal_padding = std::max<qreal>(2.0, cell_rect.width() * 0.12);
  const qreal vertical_padding = std::max<qreal>(2.0, cell_rect.height() * 0.14);
  QRectF content_rect = cell_rect.adjusted(horizontal_padding,
                                           vertical_padding,
                                           -horizontal_padding,
                                           -vertical_padding);
  if (content_rect.width() < 4.0 || content_rect.height() < 4.0) {
    content_rect = cell_rect.adjusted(1.0, 1.0, -1.0, -1.0);
  }
  return content_rect;
}

qreal LcdPreviewWidget::fittedGlyphPointSize(const QRectF& cell_rect) {
  const QRectF content_rect = contentRectForCell(cell_rect);

  QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  font.setBold(true);

  const QString glyph_candidates = QStringLiteral("WMQ@#%08-");
  qreal low = 4.0;
  qreal high = std::max<qreal>(8.0, content_rect.height() * 1.35);
  qreal best = low;

  for (int iteration = 0; iteration < 20; ++iteration) {
    const qreal mid = (low + high) / 2.0;
    font.setPointSizeF(mid);
    const QFontMetricsF metrics(font);

    qreal max_width = 0.0;
    for (const QChar glyph : glyph_candidates) {
      max_width = std::max(max_width, metrics.tightBoundingRect(QString(glyph)).width());
    }

    const bool fits_width = max_width <= content_rect.width();
    const bool fits_height = metrics.tightBoundingRect(QStringLiteral("W")).height() <=
                             content_rect.height();
    if (fits_width && fits_height) {
      best = mid;
      low = mid;
    } else {
      high = mid;
    }
  }

  return best;
}

void LcdPreviewWidget::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.fillRect(rect(), QColor("#f4efe2"));

  const QRectF outer_rect = rect().adjusted(8.0, 8.0, -8.0, -8.0);
  painter.setPen(QPen(QColor("#c4b595"), 1.5));
  painter.setBrush(QColor("#efe3c6"));
  painter.drawRoundedRect(outer_rect, 18.0, 18.0);

  const QRectF screen_rect = outer_rect.adjusted(20.0, 20.0, -20.0, -20.0);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor("#294734"));
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
  cell_font.setPointSizeF(fittedGlyphPointSize(QRectF(0.0, 0.0, cell_width, cell_height)));
  painter.setFont(cell_font);

  for (std::size_t row = 0; row < geometry_.rows; ++row) {
    const auto& line = row < frame_.size() ? frame_[row] : std::string{};
    for (std::size_t col = 0; col < geometry_.cols; ++col) {
      const qreal x = grid_rect.left() +
                      static_cast<qreal>(col) * (cell_width + col_spacing);
      const qreal y = grid_rect.top() +
                      static_cast<qreal>(row) * (cell_height + row_spacing);
      const QRectF cell_rect(x, y, cell_width, cell_height);

      painter.setPen(QPen(QColor(255, 255, 255, 14), 0.8));
      painter.setBrush(QColor(255, 255, 255, 10));
      painter.drawRoundedRect(cell_rect, 4.0, 4.0);

      const char symbol = col < line.size() ? line[col] : ' ';
      paintGlyph(painter, contentRectForCell(cell_rect), symbol);
    }
  }
}

void LcdPreviewWidget::paintGlyph(QPainter& painter,
                                  const QRectF& cell_rect,
                                  char symbol) const {
  const auto code = static_cast<unsigned char>(symbol);
  if (code >= 1 && code <= 5) {
    const qreal fraction = static_cast<qreal>(code) / 5.0;
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(213, 240, 138, 42));
    painter.drawRoundedRect(cell_rect, 2.5, 2.5);

    const QRectF fill_rect(
        cell_rect.left(),
        cell_rect.top(),
        std::max<qreal>(2.0, cell_rect.width() * fraction),
        cell_rect.height());
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
