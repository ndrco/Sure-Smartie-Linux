#include <filesystem>

#include <QApplication>
#include <QMessageBox>

#include "sure_smartie/gui/MainWindow.hpp"

namespace {

std::filesystem::path defaultGuiConfigPath() {
  const std::filesystem::path stdout_example{"configs/stdout-example.json"};
  if (std::filesystem::exists(stdout_example)) {
    return stdout_example;
  }

  const std::filesystem::path sure_example{"configs/sure-example.json"};
  if (std::filesystem::exists(sure_example)) {
    return sure_example;
  }

  return {};
}

}  // namespace

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  app.setApplicationName("sure-smartie-gui");
  app.setOrganizationName("SureSmartieLinux");

  sure_smartie::gui::MainWindow window;

  std::filesystem::path config_path;
  if (argc > 1) {
    config_path = argv[1];
  } else {
    config_path = defaultGuiConfigPath();
  }

  if (!config_path.empty()) {
    QString error_message;
    if (!window.loadConfigFile(QString::fromStdString(config_path.string()), &error_message)) {
      QMessageBox::warning(
          &window,
          "Config load warning",
          QString("Unable to load %1\n\n%2")
              .arg(QString::fromStdString(config_path.string()), error_message));
    }
  }

  window.show();
  return app.exec();
}
