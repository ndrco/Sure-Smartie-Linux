#include <cassert>
#include <filesystem>

#include <QApplication>
#include <QTemporaryDir>

#include "sure_smartie/core/Config.hpp"
#include "sure_smartie/core/ConfigValidator.hpp"
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

int main(int argc, char** argv) {
  QApplication app(argc, argv);

  sure_smartie::gui::MainWindow window;
  QString error_message;

  const auto config_path =
      std::filesystem::current_path() / "configs" / "sure-example.json";
  assert(window.loadConfigFile(QString::fromStdString(config_path.string()), &error_message));
  assert(!window.isDirty());

  window.setScreenLine(0, 0, "CPU {bar:cpu.load,6} gui");
  assert(window.isDirty());

  window.refreshPreviewNow();
  const QString preview_line = window.previewLine(0);
  assert(preview_line.size() == 20);

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
