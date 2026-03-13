#include <cstdlib>
#include <filesystem>

#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#include <QSize>

#include "sure_smartie/gui/MainWindow.hpp"

namespace {

std::filesystem::path defaultGuiConfigPath() {
  if (const char* config_env = std::getenv("SURE_SMARTIE_CONFIG");
      config_env != nullptr && *config_env != '\0') {
    return config_env;
  }

#ifdef SURE_SMARTIE_DEFAULT_CONFIG_PATH
  const std::filesystem::path installed_config{SURE_SMARTIE_DEFAULT_CONFIG_PATH};
  if (std::filesystem::exists(installed_config)) {
    return installed_config;
  }

  const std::filesystem::path installed_example =
      installed_config.string() + ".example";
  if (std::filesystem::exists(installed_example)) {
    return installed_example;
  }
#endif

  const std::filesystem::path sure_example{"configs/sure-example.json"};
  if (std::filesystem::exists(sure_example)) {
    return sure_example;
  }

  const std::filesystem::path stdout_example{"configs/stdout-example.json"};
  if (std::filesystem::exists(stdout_example)) {
    return stdout_example;
  }

  return {};
}

}  // namespace

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  app.setApplicationName("sure-smartie-gui");
  app.setOrganizationName("SureSmartieLinux");

  QIcon app_icon;
  app_icon.addFile(":/icons/app-icon-16.png", QSize(16, 16));
  app_icon.addFile(":/icons/app-icon-24.png", QSize(24, 24));
  app_icon.addFile(":/icons/app-icon-32.png", QSize(32, 32));
  app_icon.addFile(":/icons/app-icon-48.png", QSize(48, 48));
  app_icon.addFile(":/icons/app-icon-64.png", QSize(64, 64));
  app_icon.addFile(":/icons/app-icon-128.png", QSize(128, 128));
  app_icon.addFile(":/icons/app-icon-256.png", QSize(256, 256));
  app_icon.addFile(":/icons/app-icon-512.png", QSize(512, 512));
  app.setWindowIcon(app_icon);

  sure_smartie::gui::MainWindow window;
  window.setWindowIcon(app_icon);

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
