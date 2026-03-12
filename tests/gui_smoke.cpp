#include <algorithm>
#include <cassert>
#include <filesystem>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRectF>
#include <QTemporaryDir>

#include "sure_smartie/core/Config.hpp"
#include "sure_smartie/core/ConfigValidator.hpp"
#include "sure_smartie/gui/LcdPreviewWidget.hpp"
#include "sure_smartie/gui/MainWindow.hpp"

namespace {

bool hasErrors(const std::vector<sure_smartie::core::Diagnostic>& diagnostics) {
  for (const auto& diagnostic : diagnostics) {
    if (diagnostic.severity == sure_smartie::core::DiagnosticSeverity::error) {
      return true;
    }
  }

  return false;
}

}  // namespace

namespace sure_smartie::gui {

struct LcdPreviewWidgetTestAccess {
  static QRectF contentRectForCell(const QRectF& cell_rect) {
    return LcdPreviewWidget::contentRectForCell(cell_rect);
  }

  static qreal fittedGlyphPointSize(const QRectF& cell_rect) {
    return LcdPreviewWidget::fittedGlyphPointSize(cell_rect);
  }
};

}  // namespace sure_smartie::gui

int main(int argc, char** argv) {
  QApplication app(argc, argv);

  sure_smartie::gui::MainWindow window;
  window.show();
  app.processEvents();
  QString error_message;

  const auto config_path =
      std::filesystem::current_path() / "configs" / "sure-example.json";
  assert(window.loadConfigFile(QString::fromStdString(config_path.string()), &error_message));
  assert(!window.isDirty());
  app.processEvents();

  window.setScreenLine(0, 0, "CPU {bar:cpu.load,6} gui");
  assert(window.isDirty());

  window.refreshPreviewNow();
  app.processEvents();
  const QString preview_line = window.previewLine(0);
  assert(preview_line.size() == 20);

  auto* preview_combo = window.findChild<QComboBox*>("previewScreenCombo");
  auto* rotation_check = window.findChild<QCheckBox*>("rotationPreviewCheck");
  auto* validation_list = window.findChild<QListWidget*>("validationList");
  auto* preview_status = window.findChild<QLabel*>("previewStatusLabel");
  auto* save_button = window.findChild<QPushButton*>("footerSaveButton");
  auto* apply_button = window.findChild<QPushButton*>("footerApplyButton");
  auto* cancel_button = window.findChild<QPushButton*>("footerCancelButton");
  assert(preview_combo != nullptr);
  assert(rotation_check != nullptr);
  assert(validation_list != nullptr);
  assert(preview_status != nullptr);
  assert(save_button != nullptr);
  assert(apply_button != nullptr);
  assert(cancel_button != nullptr);
  assert(preview_combo->isEnabled());
  assert(validation_list->count() >= 1);
  assert(!preview_status->text().isEmpty());
  assert(save_button->isEnabled());
  assert(apply_button->isEnabled());
  assert(cancel_button->isEnabled());

  rotation_check->setChecked(true);
  app.processEvents();
  assert(!preview_combo->isEnabled());
  assert(!preview_status->text().isEmpty());

  rotation_check->setChecked(false);
  app.processEvents();
  assert(preview_combo->isEnabled());

  const QRectF cell_rect(0.0, 0.0, 26.0, 42.0);
  const QRectF content_rect =
      sure_smartie::gui::LcdPreviewWidgetTestAccess::contentRectForCell(cell_rect);
  const qreal point_size =
      sure_smartie::gui::LcdPreviewWidgetTestAccess::fittedGlyphPointSize(cell_rect);
  assert(point_size >= 4.0);

  QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  font.setBold(true);
  font.setPointSizeF(point_size);
  const QFontMetricsF metrics(font);
  qreal max_glyph_width = 0.0;
  for (const QChar glyph : QStringLiteral("8%W-0")) {
    max_glyph_width =
        std::max(max_glyph_width, metrics.tightBoundingRect(QString(glyph)).width());
  }
  assert(max_glyph_width <= content_rect.width() + 0.5);
  assert(metrics.tightBoundingRect(QStringLiteral("W")).height() <= content_rect.height() + 0.5);

  cancel_button->click();
  app.processEvents();
  assert(!window.isDirty());
  assert(!cancel_button->isEnabled());

  QTemporaryDir temporary_dir;
  assert(temporary_dir.isValid());
  const QString save_path = temporary_dir.filePath("gui-config.json");
  assert(window.saveConfigFile(save_path, &error_message));
  assert(!window.isDirty());

  const auto saved_config =
      sure_smartie::core::ConfigLoader::loadFromFile(save_path.toStdString());
  const auto diagnostics = sure_smartie::core::ConfigValidator::validate(saved_config);
  assert(!hasErrors(diagnostics));

  return 0;
}
