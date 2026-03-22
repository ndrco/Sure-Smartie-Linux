#include "sure_smartie/gui/MainWindow.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <utility>

#include <QAction>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLayout>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStyle>
#include <QSplitter>
#include <QStatusBar>
#include <QStringList>
#include <QTemporaryFile>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "sure_smartie/core/Config.hpp"
#include "sure_smartie/core/ConfigSerializer.hpp"
#include "sure_smartie/core/ConfigValidator.hpp"
#include "sure_smartie/core/MetricsSnapshotService.hpp"
#include "sure_smartie/engine/ScreenManager.hpp"
#include "sure_smartie/engine/TemplateEngine.hpp"
#include "sure_smartie/gui/LcdPreviewWidget.hpp"
#include "sure_smartie/providers/BuiltinProviderFactory.hpp"

namespace sure_smartie::gui {
namespace {

QString toQString(const std::filesystem::path& path) {
  return QString::fromStdString(path.string());
}

std::filesystem::path toPath(const QString& path) {
  return std::filesystem::path(path.toStdString());
}

std::filesystem::path normalizePath(const std::filesystem::path& path) {
  std::error_code error;
  const auto absolute = std::filesystem::absolute(path, error);
  return (error ? path : absolute).lexically_normal();
}

#ifdef SURE_SMARTIE_DEFAULT_CONFIG_PATH
std::filesystem::path systemConfigPath() {
  return normalizePath(std::filesystem::path{SURE_SMARTIE_DEFAULT_CONFIG_PATH});
}

std::filesystem::path systemConfigExamplePath() {
  return normalizePath(std::filesystem::path{
      std::string{SURE_SMARTIE_DEFAULT_CONFIG_PATH} + ".example"});
}

bool isSystemConfigPath(const std::filesystem::path& path) {
  return normalizePath(path) == systemConfigPath();
}

bool isSystemConfigExamplePath(const std::filesystem::path& path) {
  return normalizePath(path) == systemConfigExamplePath();
}
#else
bool isSystemConfigPath(const std::filesystem::path& path) {
  Q_UNUSED(path);
  return false;
}

bool isSystemConfigExamplePath(const std::filesystem::path& path) {
  Q_UNUSED(path);
  return false;
}
#endif

bool isPermissionLikeSaveError(const QString& message) {
  const QString lowered = message.toLower();
  return lowered.contains("unable to open config for writing") ||
         lowered.contains("permission denied") ||
         lowered.contains("operation not permitted") ||
         lowered.contains("read-only");
}

QString suggestedSavePath(const std::filesystem::path& current_path) {
#ifdef SURE_SMARTIE_DEFAULT_CONFIG_PATH
  if (current_path.empty()) {
    return QString::fromStdString(systemConfigPath().string());
  }
  if (isSystemConfigExamplePath(current_path)) {
    return QString::fromStdString(systemConfigPath().string());
  }
#endif
  return current_path.empty() ? QString{} : toQString(current_path);
}

QString findPrivilegedSaveHelperPath() {
  const QString application_dir = QCoreApplication::applicationDirPath();
  const QString local_helper =
      QFileInfo(application_dir + "/sure-smartie-privileged-save").absoluteFilePath();
  if (QFileInfo::exists(local_helper) && QFileInfo(local_helper).isExecutable()) {
    return local_helper;
  }

#ifdef SURE_SMARTIE_PRIVILEGED_SAVE_HELPER_PATH
  const QString installed_helper =
      QStringLiteral(SURE_SMARTIE_PRIVILEGED_SAVE_HELPER_PATH);
  if (QFileInfo::exists(installed_helper) && QFileInfo(installed_helper).isExecutable()) {
    return installed_helper;
  }
#endif

  return {};
}

QString screenLabel(const core::ScreenDefinition& screen, int index) {
  if (!screen.name.empty()) {
    return QString::fromStdString(screen.name);
  }

  return QString("screen_%1").arg(index + 1);
}

QString severityText(core::DiagnosticSeverity severity) {
  switch (severity) {
    case core::DiagnosticSeverity::info:
      return "info";
    case core::DiagnosticSeverity::warning:
      return "warning";
    case core::DiagnosticSeverity::error:
      return "error";
  }

  return "unknown";
}

QColor severityColor(core::DiagnosticSeverity severity) {
  switch (severity) {
    case core::DiagnosticSeverity::info:
      return QColor("#475467");
    case core::DiagnosticSeverity::warning:
      return QColor("#b54708");
    case core::DiagnosticSeverity::error:
      return QColor("#b42318");
  }

  return QColor("#475467");
}

bool diagnosticsHaveErrors(const std::vector<core::Diagnostic>& diagnostics) {
  for (const auto& diagnostic : diagnostics) {
    if (diagnostic.severity == core::DiagnosticSeverity::error) {
      return true;
    }
  }

  return false;
}

QString diagnosticText(const core::Diagnostic& diagnostic) {
  return QString("[%1] %2: %3")
      .arg(severityText(diagnostic.severity),
           QString::fromStdString(diagnostic.field_path),
           QString::fromStdString(diagnostic.message));
}

std::optional<std::pair<int, QString>> parseIndexedField(const QString& field_path,
                                                         const QString& prefix) {
  const QString marker = prefix + "[";
  if (!field_path.startsWith(marker)) {
    return std::nullopt;
  }

  const int close_index = field_path.indexOf(']', marker.size());
  if (close_index < 0) {
    return std::nullopt;
  }

  bool ok = false;
  const int index =
      field_path.mid(marker.size(), close_index - marker.size()).toInt(&ok);
  if (!ok) {
    return std::nullopt;
  }

  return std::make_pair(index, field_path.mid(close_index + 1));
}

core::AppConfig makeDefaultConfig() {
  core::AppConfig config;
  config.device = "/dev/null";
  config.display.type = "stdout";
  config.display.cols = 20;
  config.display.rows = 4;
  config.display.backlight = true;
  config.display.contrast = 128;
  config.display.brightness = 192;
  config.providers = {"cpu", "gpu", "ram", "system"};
  config.screens = {
      core::ScreenDefinition{
          .name = "overview",
          .interval = std::chrono::milliseconds{2000},
          .lines = {
              "CPU {bar:cpu.load,6} {cpu.load}%",
              "GPU {bar:gpu.load,6} {gpu.load}%",
              "RAM {ram.percent}% {ram.used_gb}",
              "{system.time} {system.hostname}",
          },
      },
  };
  return config;
}

void clearLayout(QLayout* layout) {
  while (QLayoutItem* item = layout->takeAt(0)) {
    if (QWidget* widget = item->widget()) {
      delete widget;
    }
    if (QLayout* child_layout = item->layout()) {
      clearLayout(child_layout);
      delete child_layout;
    }
    delete item;
  }
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), config_(makeDefaultConfig()) {
  buildUi();
  applyStyle();
  syncUiFromConfig();
  setDirty(false);
  refreshPreview(true);
}

MainWindow::~MainWindow() { discardPendingMetricsCollection(); }

bool MainWindow::loadConfigFile(const QString& path, QString* error_message) {
  try {
    discardPendingMetricsCollection();
    config_ = core::ConfigLoader::loadFromFile(toPath(path));
    current_config_path_ = toPath(path);
    metrics_service_dirty_ = true;
    ++metrics_epoch_;
    last_metrics_.clear();
    preview_diagnostics_.clear();
    rotation_manager_.reset();
    syncUiFromConfig();
    setDirty(false);
    refreshPreview(true);
    statusBar()->showMessage(QString("Loaded %1").arg(path), 2500);
    return true;
  } catch (const std::exception& error) {
    if (error_message != nullptr) {
      *error_message = error.what();
    }
    return false;
  }
}

bool MainWindow::saveConfigFile(const QString& path, QString* error_message) {
  try {
    const auto target_path = toPath(path);
    if (!target_path.parent_path().empty()) {
      std::filesystem::create_directories(target_path.parent_path());
    }
    core::ConfigSerializer::saveToFile(config_, target_path);
    current_config_path_ = target_path;
    setDirty(false);
    statusBar()->showMessage(QString("Saved %1").arg(path), 2500);
    return true;
  } catch (const std::exception& error) {
    const QString save_error = error.what();
    const auto target_path = toPath(path);
    if (isSystemConfigPath(target_path) && isPermissionLikeSaveError(save_error)) {
      return saveConfigFilePrivileged(path, false, error_message);
    }

    if (error_message != nullptr) {
      *error_message = save_error;
    }
    return false;
  }
}

bool MainWindow::isDirty() const { return dirty_; }

QString MainWindow::currentConfigPath() const { return toQString(current_config_path_); }

void MainWindow::setScreenLine(int screen_index, int line_index, const QString& text) {
  if (screen_index < 0 ||
      static_cast<std::size_t>(screen_index) >= config_.screens.size() ||
      line_index < 0) {
    return;
  }

  setSelectedScreen(screen_index);
  if (static_cast<std::size_t>(line_index) >= line_edits_.size()) {
    auto& screen = config_.screens[static_cast<std::size_t>(screen_index)];
    screen.lines.resize(static_cast<std::size_t>(line_index + 1));
    syncScreenEditorFromSelection();
  }

  if (static_cast<std::size_t>(line_index) < line_edits_.size()) {
    line_edits_[static_cast<std::size_t>(line_index)]->setText(text);
  }
}

QString MainWindow::previewLine(int row) const {
  return preview_widget_ != nullptr ? preview_widget_->lineText(row) : QString{};
}

void MainWindow::refreshPreviewNow() { refreshPreview(true); }

void MainWindow::closeEvent(QCloseEvent* event) {
  if (maybeDiscardChanges()) {
    event->accept();
    return;
  }

  event->ignore();
}

void MainWindow::buildUi() {
  resize(1480, 900);
  setMinimumSize(1200, 760);

  auto* file_menu = menuBar()->addMenu("&File");
  auto* open_action = file_menu->addAction("Open...");
  auto* save_action = file_menu->addAction("Save");
  auto* save_as_action = file_menu->addAction("Save As...");
  file_menu->addSeparator();
  auto* quit_action = file_menu->addAction("Quit");

  connect(open_action, &QAction::triggered, this, [this]() {
    if (!maybeDiscardChanges()) {
      return;
    }

    const QString path = QFileDialog::getOpenFileName(
        this, "Open config", currentConfigPath(), "JSON files (*.json);;All files (*)");
    if (path.isEmpty()) {
      return;
    }

    QString error_message;
    if (!loadConfigFile(path, &error_message)) {
      QMessageBox::critical(this, "Open failed", error_message);
    }
  });

  connect(save_action, &QAction::triggered, this, [this]() {
    QString error_message;
    if (!saveCurrentConfig(false, &error_message)) {
      if (!error_message.isEmpty()) {
        QMessageBox::critical(this, "Save failed", error_message);
      }
    }
  });

  connect(save_as_action, &QAction::triggered, this, [this]() {
    QString error_message;
    if (!saveCurrentConfig(true, &error_message)) {
      if (!error_message.isEmpty()) {
        QMessageBox::critical(this, "Save failed", error_message);
      }
    }
  });

  connect(quit_action, &QAction::triggered, this, [this]() { close(); });

  auto* central = new QWidget(this);
  central->setObjectName("mainCanvas");
  auto* root_layout = new QVBoxLayout(central);
  root_layout->setContentsMargins(12, 12, 12, 12);
  root_layout->setSpacing(12);

  auto* splitter = new QSplitter(Qt::Horizontal, central);
  splitter->setObjectName("mainSplitter");
  root_layout->addWidget(splitter);
  setCentralWidget(central);

  auto* left_scroll = new QScrollArea(splitter);
  left_scroll->setWidgetResizable(true);
  left_scroll->setFrameShape(QFrame::NoFrame);
  left_scroll->viewport()->setObjectName("leftScrollViewport");

  auto* editor_root = new QWidget(left_scroll);
  editor_root->setObjectName("editorRoot");
  auto* editor_layout = new QVBoxLayout(editor_root);
  editor_layout->setContentsMargins(0, 0, 12, 0);
  editor_layout->setSpacing(12);
  left_scroll->setWidget(editor_root);

  auto* project_group = new QGroupBox("Project", editor_root);
  auto* project_layout = new QVBoxLayout(project_group);
  project_layout->setContentsMargins(16, 18, 16, 14);
  project_layout->setSpacing(6);
  project_path_label_ = new QLabel(project_group);
  project_path_label_->setObjectName("projectSummaryLabel");
  project_path_label_->setWordWrap(true);
  dirty_state_label_ = new QLabel(project_group);
  dirty_state_label_->setObjectName("dirtyStateLabel");
  dirty_state_label_->setWordWrap(true);
  project_layout->addWidget(project_path_label_);
  project_layout->addWidget(dirty_state_label_);
  editor_layout->addWidget(project_group);

  auto* display_group = new QGroupBox("Display Settings", editor_root);
  auto* display_layout = new QFormLayout(display_group);
  display_layout->setContentsMargins(16, 18, 16, 16);
  display_layout->setHorizontalSpacing(14);
  display_layout->setVerticalSpacing(10);
  display_layout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  display_layout->setFormAlignment(Qt::AlignTop);
  device_edit_ = new QLineEdit(display_group);
  device_edit_->setPlaceholderText("/dev/ttyUSB0");
  refresh_interval_spin_ = new QSpinBox(display_group);
  refresh_interval_spin_->setRange(-1000000, 600000);
  baudrate_spin_ = new QSpinBox(display_group);
  baudrate_spin_->setRange(-1000000, 4000000);
  display_type_combo_ = new QComboBox(display_group);
  display_type_combo_->setEditable(true);
  display_type_combo_->addItems({"sure", "stdout"});
  cols_spin_ = new QSpinBox(display_group);
  cols_spin_->setRange(-128, 512);
  rows_spin_ = new QSpinBox(display_group);
  rows_spin_->setRange(-128, 128);
  backlight_check_ = new QCheckBox("Enabled", display_group);
  contrast_spin_ = new QSpinBox(display_group);
  contrast_spin_->setRange(-1024, 1024);
  brightness_spin_ = new QSpinBox(display_group);
  brightness_spin_->setRange(-1024, 1024);
  display_layout->addRow("Device", device_edit_);
  display_layout->addRow("Refresh ms", refresh_interval_spin_);
  display_layout->addRow("Baudrate", baudrate_spin_);
  display_layout->addRow("Display type", display_type_combo_);
  display_layout->addRow("Columns", cols_spin_);
  display_layout->addRow("Rows", rows_spin_);
  display_layout->addRow("Backlight", backlight_check_);
  display_layout->addRow("Contrast", contrast_spin_);
  display_layout->addRow("Brightness", brightness_spin_);
  editor_layout->addWidget(display_group);

  auto* cpu_fan_group = new QGroupBox("CPU Fan Sensor", editor_root);
  auto* cpu_fan_layout = new QFormLayout(cpu_fan_group);
  cpu_fan_layout->setContentsMargins(16, 18, 16, 16);
  cpu_fan_layout->setHorizontalSpacing(14);
  cpu_fan_layout->setVerticalSpacing(10);
  cpu_fan_layout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  cpu_fan_layout->setFormAlignment(Qt::AlignTop);
  cpu_fan_rpm_path_edit_ = new QLineEdit(cpu_fan_group);
  cpu_fan_rpm_path_edit_->setObjectName("cpuFanRpmPathEdit");
  cpu_fan_rpm_path_edit_->setPlaceholderText("/sys/class/hwmon/hwmon9/fan3_input");
  cpu_fan_max_rpm_spin_ = new QSpinBox(cpu_fan_group);
  cpu_fan_max_rpm_spin_->setObjectName("cpuFanMaxRpmSpin");
  cpu_fan_max_rpm_spin_->setRange(0, 50000);
  cpu_fan_max_rpm_spin_->setSpecialValueText("not set");
  auto* cpu_fan_hint = new QLabel(
      "cpu.fan_rpm reads the exact sysfs file. cpu.fan_percent uses Max RPM when it is set.",
      cpu_fan_group);
  cpu_fan_hint->setObjectName("sectionHintLabel");
  cpu_fan_hint->setWordWrap(true);
  cpu_fan_layout->addRow("RPM path", cpu_fan_rpm_path_edit_);
  cpu_fan_layout->addRow("Max RPM", cpu_fan_max_rpm_spin_);
  cpu_fan_layout->addRow(QString(), cpu_fan_hint);
  editor_layout->addWidget(cpu_fan_group);

  auto* providers_group = new QGroupBox("Providers & Plugins", editor_root);
  auto* providers_layout = new QVBoxLayout(providers_group);
  providers_layout->setContentsMargins(16, 18, 16, 16);
  providers_layout->setSpacing(10);
  auto* builtin_label = new QLabel("Builtin providers", providers_group);
  builtin_label->setObjectName("sectionCaptionLabel");
  providers_layout->addWidget(builtin_label);
  auto* provider_grid = new QGridLayout();
  provider_grid->setHorizontalSpacing(20);
  provider_grid->setVerticalSpacing(8);
  int provider_column = 0;
  int provider_row = 0;
  for (const auto& provider_name : providers::builtinProviderNames()) {
    auto* check = new QCheckBox(QString::fromStdString(provider_name), providers_group);
    provider_checks_.push_back({provider_name, check});
    provider_grid->addWidget(check, provider_row, provider_column);
    ++provider_column;
    if (provider_column == 2) {
      provider_column = 0;
      ++provider_row;
    }
  }
  providers_layout->addLayout(provider_grid);
  extra_providers_label_ = new QLabel(providers_group);
  extra_providers_label_->setObjectName("sectionHintLabel");
  extra_providers_label_->setWordWrap(true);
  providers_layout->addWidget(extra_providers_label_);
  auto* plugin_paths_label = new QLabel("Plugin paths", providers_group);
  plugin_paths_label->setObjectName("sectionCaptionLabel");
  providers_layout->addWidget(plugin_paths_label);
  plugin_paths_list_ = new QListWidget(providers_group);
  plugin_paths_list_->setObjectName("pluginPathsList");
  plugin_paths_list_->setSelectionMode(QAbstractItemView::SingleSelection);
  plugin_paths_list_->setMinimumHeight(110);
  plugin_paths_list_->setMaximumHeight(160);
  plugin_paths_list_->setAlternatingRowColors(true);
  plugin_paths_list_->setUniformItemSizes(true);
  providers_layout->addWidget(plugin_paths_list_);
  auto* plugin_buttons = new QHBoxLayout();
  plugin_buttons->setSpacing(8);
  auto* add_plugin_button = new QPushButton("Add plugin", providers_group);
  auto* remove_plugin_button = new QPushButton("Remove plugin", providers_group);
  plugin_buttons->addWidget(add_plugin_button);
  plugin_buttons->addWidget(remove_plugin_button);
  plugin_buttons->addStretch(1);
  providers_layout->addLayout(plugin_buttons);
  editor_layout->addWidget(providers_group);

  auto* glyphs_group = new QGroupBox("Custom Glyphs", editor_root);
  auto* glyphs_layout = new QHBoxLayout(glyphs_group);
  glyphs_layout->setContentsMargins(16, 18, 16, 16);
  glyphs_layout->setSpacing(14);

  auto* glyphs_list_column = new QVBoxLayout();
  auto* glyphs_list_label = new QLabel("Available glyphs", glyphs_group);
  glyphs_list_label->setObjectName("sectionCaptionLabel");
  glyphs_list_column->addWidget(glyphs_list_label);
  custom_glyphs_list_ = new QListWidget(glyphs_group);
  custom_glyphs_list_->setObjectName("customGlyphList");
  custom_glyphs_list_->setSelectionMode(QAbstractItemView::SingleSelection);
  custom_glyphs_list_->setMinimumHeight(140);
  custom_glyphs_list_->setAlternatingRowColors(true);
  custom_glyphs_list_->setUniformItemSizes(true);
  glyphs_list_column->addWidget(custom_glyphs_list_, 1);
  auto* glyph_buttons = new QHBoxLayout();
  glyph_buttons->setSpacing(8);
  auto* add_glyph_button = new QPushButton("Add glyph", glyphs_group);
  add_glyph_button->setObjectName("addCustomGlyphButton");
  auto* remove_glyph_button = new QPushButton("Remove glyph", glyphs_group);
  remove_glyph_button->setObjectName("removeCustomGlyphButton");
  glyph_buttons->addWidget(add_glyph_button);
  glyph_buttons->addWidget(remove_glyph_button);
  glyph_buttons->addStretch(1);
  glyphs_list_column->addLayout(glyph_buttons);
  glyphs_layout->addLayout(glyphs_list_column, 1);

  auto* glyph_editor_column = new QVBoxLayout();
  auto* glyph_form = new QFormLayout();
  glyph_form->setHorizontalSpacing(14);
  glyph_form->setVerticalSpacing(10);
  glyph_form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  custom_glyph_name_edit_ = new QLineEdit(glyphs_group);
  custom_glyph_name_edit_->setObjectName("customGlyphNameEdit");
  custom_glyph_name_edit_->setPlaceholderText("heart");
  glyph_form->addRow("Name", custom_glyph_name_edit_);
  glyph_editor_column->addLayout(glyph_form);
  custom_glyph_token_label_ = new QLabel(glyphs_group);
  custom_glyph_token_label_->setObjectName("sectionHintLabel");
  custom_glyph_token_label_->setWordWrap(true);
  glyph_editor_column->addWidget(custom_glyph_token_label_);
  auto* glyph_matrix = new QGridLayout();
  glyph_matrix->setHorizontalSpacing(6);
  glyph_matrix->setVerticalSpacing(6);
  for (int row = 0; row < static_cast<int>(core::kGlyphHeight); ++row) {
    auto* row_label = new QLabel(QString::number(row + 1), glyphs_group);
    row_label->setObjectName("sectionHintLabel");
    glyph_matrix->addWidget(row_label, row, 0, Qt::AlignCenter);
    for (int column = 0; column < static_cast<int>(core::kGlyphWidth); ++column) {
      auto* pixel_button = new QPushButton(glyphs_group);
      pixel_button->setCheckable(true);
      pixel_button->setProperty("glyphPixelButton", true);
      pixel_button->setFixedSize(22, 22);
      pixel_button->setFocusPolicy(Qt::StrongFocus);
      custom_glyph_buttons_[static_cast<std::size_t>(row)]
                           [static_cast<std::size_t>(column)] = pixel_button;
      glyph_matrix->addWidget(pixel_button, row, column + 1);
    }
  }
  glyph_editor_column->addLayout(glyph_matrix);
  auto* glyph_hint = new QLabel("Rows are top-to-bottom. Columns are left-to-right.", glyphs_group);
  glyph_hint->setObjectName("sectionHintLabel");
  glyph_hint->setWordWrap(true);
  glyph_editor_column->addWidget(glyph_hint);
  glyph_editor_column->addStretch(1);
  glyphs_layout->addLayout(glyph_editor_column, 1);
  editor_layout->addWidget(glyphs_group);

  auto* screens_group = new QGroupBox("Screens Editor", editor_root);
  auto* screens_layout = new QHBoxLayout(screens_group);
  auto* screens_list_column = new QVBoxLayout();
  screens_list_ = new QListWidget(screens_group);
  screens_list_->setSelectionMode(QAbstractItemView::SingleSelection);
  screens_list_column->addWidget(screens_list_);
  auto* screen_button_row = new QGridLayout();
  auto* add_screen_button = new QPushButton("Add", screens_group);
  auto* remove_screen_button = new QPushButton("Remove", screens_group);
  auto* duplicate_screen_button = new QPushButton("Duplicate", screens_group);
  auto* move_up_button = new QPushButton("Move Up", screens_group);
  auto* move_down_button = new QPushButton("Move Down", screens_group);
  screen_button_row->addWidget(add_screen_button, 0, 0);
  screen_button_row->addWidget(remove_screen_button, 0, 1);
  screen_button_row->addWidget(duplicate_screen_button, 1, 0);
  screen_button_row->addWidget(move_up_button, 1, 1);
  screen_button_row->addWidget(move_down_button, 2, 0, 1, 2);
  screens_list_column->addLayout(screen_button_row);
  screens_layout->addLayout(screens_list_column, 1);

  auto* screen_editor_column = new QVBoxLayout();
  auto* screen_form = new QFormLayout();
  screen_name_edit_ = new QLineEdit(screens_group);
  screen_interval_spin_ = new QSpinBox(screens_group);
  screen_interval_spin_->setRange(-1000000, 600000);
  runtime_rotation_check_ = new QCheckBox("Enabled", screens_group);
  runtime_rotation_check_->setObjectName("runtimeRotationCheck");
  screen_form->addRow("Name", screen_name_edit_);
  screen_form->addRow("Interval ms", screen_interval_spin_);
  screen_form->addRow("Auto rotation", runtime_rotation_check_);
  screen_editor_column->addLayout(screen_form);
  auto* lines_group = new QGroupBox("Lines", screens_group);
  lines_layout_ = new QVBoxLayout(lines_group);
  screen_editor_column->addWidget(lines_group);
  screens_layout->addLayout(screen_editor_column, 2);
  editor_layout->addWidget(screens_group);
  editor_layout->addStretch(1);

  auto* right_panel = new QWidget(splitter);
  right_panel->setObjectName("rightPanel");
  auto* right_layout = new QVBoxLayout(right_panel);
  right_layout->setContentsMargins(8, 0, 0, 0);
  right_layout->setSpacing(12);

  auto* preview_group = new QGroupBox("Preview", right_panel);
  auto* preview_layout = new QVBoxLayout(preview_group);
  preview_layout->setContentsMargins(16, 18, 16, 16);
  preview_layout->setSpacing(10);
  auto* preview_row_top = new QHBoxLayout();
  preview_row_top->setSpacing(10);
  auto* preview_row_bottom = new QHBoxLayout();
  preview_row_bottom->setSpacing(10);
  preview_screen_combo_ = new QComboBox(preview_group);
  preview_screen_combo_->setObjectName("previewScreenCombo");
  rotation_preview_check_ = new QCheckBox("Rotation preview", preview_group);
  rotation_preview_check_->setObjectName("rotationPreviewCheck");
  preview_refresh_spin_ = new QSpinBox(preview_group);
  preview_refresh_spin_->setObjectName("previewRefreshSpin");
  preview_refresh_spin_->setRange(200, 10000);
  preview_refresh_spin_->setSingleStep(100);
  preview_refresh_spin_->setValue(1000);
  auto* refresh_button = new QPushButton("Refresh now", preview_group);
  auto* screen_label = new QLabel("Screen", preview_group);
  screen_label->setObjectName("sectionCaptionLabel");
  auto* refresh_label = new QLabel("Refresh ms", preview_group);
  refresh_label->setObjectName("sectionCaptionLabel");
  preview_row_top->addWidget(screen_label);
  preview_row_top->addWidget(preview_screen_combo_, 1);
  preview_row_top->addWidget(refresh_button);
  preview_row_bottom->addWidget(rotation_preview_check_);
  preview_row_bottom->addStretch(1);
  preview_row_bottom->addWidget(refresh_label);
  preview_row_bottom->addWidget(preview_refresh_spin_);
  preview_layout->addLayout(preview_row_top);
  preview_layout->addLayout(preview_row_bottom);
  preview_status_label_ = new QLabel(preview_group);
  preview_status_label_->setObjectName("previewStatusLabel");
  preview_status_label_->setWordWrap(true);
  preview_layout->addWidget(preview_status_label_);
  preview_widget_ = new LcdPreviewWidget(preview_group);
  preview_layout->addWidget(preview_widget_, 1);
  right_layout->addWidget(preview_group, 2);

  auto* validation_group = new QGroupBox("Validation", right_panel);
  auto* validation_layout = new QVBoxLayout(validation_group);
  validation_layout->setContentsMargins(16, 18, 16, 16);
  validation_list_ = new QListWidget(validation_group);
  validation_list_->setObjectName("validationList");
  validation_list_->setMinimumHeight(120);
  validation_list_->setMaximumHeight(190);
  validation_list_->setAlternatingRowColors(true);
  validation_list_->setUniformItemSizes(true);
  validation_layout->addWidget(validation_list_);
  right_layout->addWidget(validation_group, 0);

  footer_actions_ = new QWidget(central);
  footer_actions_->setObjectName("footerActions");
  footer_actions_->setMaximumHeight(44);
  auto* footer_layout = new QHBoxLayout(footer_actions_);
  footer_layout->setContentsMargins(0, 4, 0, 0);
  footer_layout->setSpacing(8);
  footer_layout->addStretch(1);

  auto* cancel_button = new QPushButton("Cancel", footer_actions_);
  cancel_button->setObjectName("footerCancelButton");
  auto* apply_button = new QPushButton("Apply", footer_actions_);
  apply_button->setObjectName("footerApplyButton");
  auto* save_button = new QPushButton("Save", footer_actions_);
  save_button->setObjectName("footerSaveButton");
  footer_layout->addWidget(cancel_button);
  footer_layout->addWidget(apply_button);
  footer_layout->addWidget(save_button);
  root_layout->addWidget(footer_actions_);

  splitter->addWidget(left_scroll);
  splitter->addWidget(right_panel);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 2);

  preview_timer_ = new QTimer(this);
  preview_timer_->setInterval(preview_refresh_spin_->value());
  preview_timer_->start();

  connect(device_edit_, &QLineEdit::textChanged, this, [this](const QString& text) {
    if (updating_ui_) {
      return;
    }
    config_.device = text.toStdString();
    markConfigEdited(false, false, false);
  });

  connect(baudrate_spin_,
          static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this,
          [this](int value) {
            if (updating_ui_) {
              return;
            }
            config_.baudrate = value;
            markConfigEdited(false, false, false);
          });

  connect(refresh_interval_spin_,
          static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this,
          [this](int value) {
            if (updating_ui_) {
              return;
            }
            config_.refresh_interval = std::chrono::milliseconds(value);
            markConfigEdited(false, false, false);
          });

  connect(display_type_combo_,
          &QComboBox::currentTextChanged,
          this,
          [this](const QString& text) {
            if (updating_ui_) {
              return;
            }
            config_.display.type = text.toStdString();
            markConfigEdited(false, false, false);
          });

  connect(cols_spin_,
          static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this,
          [this](int value) {
            if (updating_ui_) {
              return;
            }
            config_.display.cols = value;
            updateLineLabels();
            markConfigEdited(false, false, false);
          });

  connect(rows_spin_,
          static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this,
          [this](int value) {
            if (updating_ui_) {
              return;
            }
            config_.display.rows = value;
            syncScreenEditorFromSelection();
            markConfigEdited(false, false, false);
          });

  connect(backlight_check_, &QCheckBox::toggled, this, [this](bool checked) {
    if (updating_ui_) {
      return;
    }
    config_.display.backlight = checked;
    markConfigEdited(false, false, false);
  });

  connect(contrast_spin_,
          static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this,
          [this](int value) {
            if (updating_ui_) {
              return;
            }
            config_.display.contrast = value;
            markConfigEdited(false, false, false);
          });

  connect(brightness_spin_,
          static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this,
          [this](int value) {
            if (updating_ui_) {
              return;
            }
            config_.display.brightness = value;
            markConfigEdited(false, false, false);
          });

  connect(cpu_fan_rpm_path_edit_, &QLineEdit::textChanged, this, [this](const QString& text) {
    if (updating_ui_) {
      return;
    }
    config_.cpu_fan.rpm_path = text.toStdString();
    markConfigEdited(true, true, false);
  });

  connect(cpu_fan_max_rpm_spin_,
          static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this,
          [this](int value) {
            if (updating_ui_) {
              return;
            }
            config_.cpu_fan.max_rpm = value;
            markConfigEdited(true, true, false);
          });

  for (const auto& [provider_name, check_box] : provider_checks_) {
    Q_UNUSED(provider_name);
    connect(check_box, &QCheckBox::toggled, this, [this](bool checked) {
      Q_UNUSED(checked);
      if (updating_ui_) {
        return;
      }

      config_.providers.clear();
      for (const auto& [builtin_name, builtin_check] : provider_checks_) {
        if (builtin_check->isChecked()) {
          config_.providers.push_back(builtin_name);
        }
      }
      config_.providers.insert(config_.providers.end(),
                               extra_provider_names_.begin(),
                               extra_provider_names_.end());
      markConfigEdited(true, true, false);
    });
  }

  connect(add_plugin_button, &QPushButton::clicked, this, [this]() { addPluginPaths(); });
  connect(remove_plugin_button, &QPushButton::clicked, this, [this]() {
    removeSelectedPluginPath();
  });

  connect(custom_glyphs_list_,
          &QListWidget::currentRowChanged,
          this,
          [this](int row) {
            Q_UNUSED(row);
            if (updating_ui_) {
              return;
            }
            syncCustomGlyphEditorFromSelection();
          });

  connect(custom_glyph_name_edit_,
          &QLineEdit::textChanged,
          this,
          [this](const QString& text) {
            if (updating_ui_) {
              return;
            }
            const int index = selectedCustomGlyphIndex();
            if (index < 0 ||
                static_cast<std::size_t>(index) >= config_.custom_glyphs.size()) {
              return;
            }
            config_.custom_glyphs[static_cast<std::size_t>(index)].name = text.toStdString();
            if (QListWidgetItem* item = custom_glyphs_list_->item(index)) {
              item->setText(text.isEmpty() ? QString("glyph_%1").arg(index + 1) : text);
            }
            updateCustomGlyphTokenHint();
            markConfigEdited(false, false, false);
          });

  for (int row = 0; row < static_cast<int>(core::kGlyphHeight); ++row) {
    for (int column = 0; column < static_cast<int>(core::kGlyphWidth); ++column) {
      auto* pixel_button =
          custom_glyph_buttons_[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)];
      connect(pixel_button, &QPushButton::toggled, this, [this, row, column](bool checked) {
        if (updating_ui_) {
          return;
        }
        const int index = selectedCustomGlyphIndex();
        if (index < 0 ||
            static_cast<std::size_t>(index) >= config_.custom_glyphs.size()) {
          return;
        }

        auto& glyph = config_.custom_glyphs[static_cast<std::size_t>(index)];
        const int mask = 1 << (static_cast<int>(core::kGlyphWidth) - column - 1);
        if (checked) {
          glyph.pattern[static_cast<std::size_t>(row)] |= mask;
        } else {
          glyph.pattern[static_cast<std::size_t>(row)] &= ~mask;
        }
        markConfigEdited(false, false, false);
      });
    }
  }

  connect(add_glyph_button, &QPushButton::clicked, this, [this]() { addCustomGlyph(); });
  connect(remove_glyph_button, &QPushButton::clicked, this, [this]() {
    removeSelectedCustomGlyph();
  });

  connect(screens_list_,
          &QListWidget::currentRowChanged,
          this,
          [this](int row) {
            if (updating_ui_) {
              return;
            }
            syncScreenEditorFromSelection();
            if (!rotation_preview_check_->isChecked() && row >= 0) {
              preview_screen_combo_->setCurrentIndex(row);
            }
            refreshPreview(false);
          });

  connect(screen_name_edit_, &QLineEdit::textChanged, this, [this](const QString& text) {
    if (updating_ui_) {
      return;
    }
    const int index = selectedScreenIndex();
    if (index < 0 || static_cast<std::size_t>(index) >= config_.screens.size()) {
      return;
    }
    config_.screens[static_cast<std::size_t>(index)].name = text.toStdString();
    if (QListWidgetItem* item = screens_list_->item(index)) {
      item->setText(screenLabel(config_.screens[static_cast<std::size_t>(index)], index));
    }
    updatePreviewScreenChoices();
    markConfigEdited(false, false, true);
  });

  connect(screen_interval_spin_,
          static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this,
          [this](int value) {
            if (updating_ui_) {
              return;
            }
            const int index = selectedScreenIndex();
            if (index < 0 || static_cast<std::size_t>(index) >= config_.screens.size()) {
              return;
            }
            config_.screens[static_cast<std::size_t>(index)].interval =
                std::chrono::milliseconds(value);
            markConfigEdited(false, false, true);
          });

  connect(runtime_rotation_check_, &QCheckBox::toggled, this, [this](bool checked) {
    if (updating_ui_) {
      return;
    }
    config_.auto_screen_rotation = checked;
    markConfigEdited(false, false, false);
  });

  connect(add_screen_button, &QPushButton::clicked, this, [this]() { addScreen(); });
  connect(remove_screen_button, &QPushButton::clicked, this, [this]() {
    removeSelectedScreen();
  });
  connect(duplicate_screen_button, &QPushButton::clicked, this, [this]() {
    duplicateSelectedScreen();
  });
  connect(move_up_button, &QPushButton::clicked, this, [this]() { moveSelectedScreen(-1); });
  connect(move_down_button, &QPushButton::clicked, this, [this]() { moveSelectedScreen(1); });

  connect(preview_screen_combo_,
          static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this,
          [this](int index) {
            if (updating_ui_ || rotation_preview_check_->isChecked()) {
              return;
            }
            if (index >= 0) {
              screens_list_->setCurrentRow(index);
            }
            refreshPreview(false);
          });

  connect(rotation_preview_check_, &QCheckBox::toggled, this, [this](bool checked) {
    Q_UNUSED(checked);
    preview_screen_combo_->setEnabled(!checked);
    resetRotationPreview();
    refreshPreview(false);
  });

  connect(preview_refresh_spin_,
          static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this,
          [this](int value) { preview_timer_->setInterval(value); });

  connect(refresh_button, &QPushButton::clicked, this, [this]() { refreshPreview(true); });
  connect(preview_timer_, &QTimer::timeout, this, [this]() { requestAsyncPreviewRefresh(); });

  connect(validation_list_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
    focusFieldPath(item->data(Qt::UserRole).toString());
  });

  connect(save_button, &QPushButton::clicked, this, [this]() {
    QString error_message;
    if (!saveCurrentConfig(false, &error_message)) {
      if (!error_message.isEmpty()) {
        QMessageBox::critical(this, "Save failed", error_message);
      }
      return;
    }
  });
  connect(apply_button, &QPushButton::clicked, this, [this]() {
    QString error_message;
    if (!applyCurrentConfig(&error_message)) {
      if (!error_message.isEmpty()) {
        QMessageBox::critical(this, "Apply failed", error_message);
      }
      return;
    }
  });
  connect(cancel_button, &QPushButton::clicked, this, [this]() { revertCurrentConfig(); });

  statusBar()->showMessage("Ready", 1500);
  updateFooterActions();
}

void MainWindow::applyStyle() {
  setStyleSheet(
      "QMainWindow { background: #ece5d8; color: #2f2418; }"
      "QWidget#mainCanvas, QWidget#editorRoot, QWidget#rightPanel, QWidget#leftScrollViewport {"
      "  background: #ece5d8;"
      "}"
      "QSplitter#mainSplitter { background: #ece5d8; }"
      "QGroupBox {"
      "  font-weight: 600;"
      "  color: #4d3b2c;"
      "  border: 1px solid #dccfbe;"
      "  border-radius: 12px;"
      "  margin-top: 14px;"
      "  padding-top: 12px;"
      "  background: #faf6ee;"
      "}"
      "QGroupBox::title {"
      "  subcontrol-origin: margin;"
      "  left: 14px;"
      "  padding: 0 8px;"
      "  background: #ece5d8;"
      "  color: #5b4630;"
      "}"
      "QLabel { color: #403124; }"
      "QLabel#sectionCaptionLabel { color: #6b533c; font-weight: 600; }"
      "QLabel#sectionHintLabel { color: #6a5a49; }"
      "QLabel#projectSummaryLabel { color: #4f3e2f; font-weight: 600; }"
      "QLabel#dirtyStateLabel[dirtyState=\"false\"] { color: #2f6b47; }"
      "QLabel#dirtyStateLabel[dirtyState=\"true\"] { color: #9f2d20; font-weight: 600; }"
      "QLabel#previewStatusLabel { color: #355748; font-weight: 600; }"
      "QLineEdit, QSpinBox, QComboBox, QListWidget {"
      "  color: #2c2319;"
      "  background: #fffdf9;"
      "  border: 1px solid #d6cab8;"
      "  border-radius: 8px;"
      "  padding: 6px 8px;"
      "  selection-background-color: #cfe3dc;"
      "  selection-color: #21342f;"
      "}"
      "QComboBox { padding-right: 18px; }"
      "QSpinBox { padding-right: 20px; }"
      "QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QListWidget:focus {"
      "  border: 1px solid #83a89a;"
      "  background: #fffefb;"
      "}"
      "QLineEdit:disabled, QSpinBox:disabled, QComboBox:disabled, QListWidget:disabled {"
      "  color: #7b6a58;"
      "  background: #efe6d8;"
      "  border: 1px solid #d2c4b0;"
      "}"
      "QComboBox QAbstractItemView {"
      "  color: #2f2418;"
      "  background: #fffdf9;"
      "  border: 1px solid #d8cebf;"
      "  selection-background-color: #d8e8e2;"
      "  selection-color: #223630;"
      "  outline: 0;"
      "}"
      "QCheckBox { color: #3d3025; spacing: 8px; }"
      "QCheckBox:disabled { color: #8b7866; }"
      "QCheckBox::indicator { width: 16px; height: 16px; }"
      "QCheckBox::indicator:unchecked {"
      "  border: 1px solid #9f8b71;"
      "  border-radius: 4px;"
      "  background: #fffdf9;"
      "}"
      "QCheckBox::indicator:checked {"
      "  border: 1px solid #2b5549;"
      "  border-radius: 4px;"
      "  background: #2f6456;"
      "}"
      "QPushButton {"
      "  background: #274c45;"
      "  color: #f7f4ea;"
      "  border: none;"
      "  border-radius: 8px;"
      "  padding: 8px 12px;"
      "  font-weight: 600;"
      "}"
      "QPushButton[glyphPixelButton=\"true\"] {"
      "  background: #fffdf9;"
      "  color: transparent;"
      "  border: 1px solid #ccbfae;"
      "  border-radius: 5px;"
      "  padding: 0;"
      "}"
      "QPushButton[glyphPixelButton=\"true\"]:checked {"
      "  background: #294734;"
      "  border: 1px solid #21382b;"
      "}"
      "QPushButton[glyphPixelButton=\"true\"]:hover {"
      "  background: #e7f0d2;"
      "}"
      "QPushButton[glyphPixelButton=\"true\"]:checked:hover {"
      "  background: #315541;"
      "}"
      "QPushButton:hover { background: #1f3d38; }"
      "QPushButton:pressed { background: #17302d; }"
      "QPushButton:disabled { background: #b5b0a6; color: #f0ece4; }"
      "QListWidget { alternate-background-color: #f7f2ea; }"
      "QScrollArea { border: none; background: transparent; }"
      "QScrollBar:vertical {"
      "  background: #ece5d8;"
      "  width: 12px;"
      "  margin: 0;"
      "}"
      "QScrollBar::handle:vertical {"
      "  background: #c9bba6;"
      "  border-radius: 6px;"
      "  min-height: 36px;"
      "}"
      "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
      "  background: #ece5d8;"
      "}"
      "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
      "  height: 0px;"
      "  background: transparent;"
      "}"
      "QScrollBar:horizontal {"
      "  background: #ece5d8;"
      "  height: 12px;"
      "  margin: 0;"
      "}"
      "QScrollBar::handle:horizontal {"
      "  background: #c9bba6;"
      "  border-radius: 6px;"
      "  min-width: 36px;"
      "}"
      "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
      "  background: #ece5d8;"
      "}"
      "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
      "  width: 0px;"
      "  background: transparent;"
      "}"
      "QSplitter::handle { background: #dfd4c6; width: 6px; }"
      "QWidget#footerActions {"
      "  border-top: 1px solid #dfd4c6;"
      "  background: #efe8dc;"
      "}"
      "QWidget#footerActions QPushButton {"
      "  padding: 6px 10px;"
      "  min-height: 26px;"
      "}"
      "QStatusBar { background: #efe7da; color: #4d3c2d; }");
}

void MainWindow::updateWindowTitle() {
  const QString name = current_config_path_.empty() ? "untitled"
                                                    : toQString(current_config_path_.filename());
  const QString dirty_marker = dirty_ ? " *" : "";
  setWindowTitle(QString("sure-smartie-gui | %1%2").arg(name, dirty_marker));
}

void MainWindow::updateProjectSummary() {
  const QString path_text = current_config_path_.empty()
                                ? "Config source: unsaved buffer"
                                : QString("Config source: %1").arg(currentConfigPath());
  project_path_label_->setText(path_text);
  dirty_state_label_->setText(dirty_ ? "Buffer state: modified" : "Buffer state: clean");
  dirty_state_label_->setProperty("dirtyState", dirty_);
  dirty_state_label_->style()->unpolish(dirty_state_label_);
  dirty_state_label_->style()->polish(dirty_state_label_);
}

void MainWindow::setDirty(bool dirty) {
  dirty_ = dirty;
  updateProjectSummary();
  updateWindowTitle();
  updateFooterActions();
}

bool MainWindow::maybeDiscardChanges() {
  if (!dirty_) {
    return true;
  }

  QMessageBox dialog(this);
  dialog.setWindowTitle("Unsaved changes");
  dialog.setText("The current config has unsaved changes.");
  dialog.setInformativeText("Save before continuing?");
  dialog.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
  dialog.setDefaultButton(QMessageBox::Save);

  const int result = dialog.exec();
  if (result == QMessageBox::Discard) {
    return true;
  }
  if (result == QMessageBox::Cancel) {
    return false;
  }

  QString error_message;
  if (!saveCurrentConfig(false, &error_message)) {
    if (!error_message.isEmpty()) {
      QMessageBox::critical(this, "Save failed", error_message);
    }
    return false;
  }

  return true;
}

void MainWindow::updateFooterActions() {
  if (footer_actions_ == nullptr) {
    return;
  }

  if (auto* save_button = footer_actions_->findChild<QPushButton*>("footerSaveButton")) {
    save_button->setEnabled(dirty_ || current_config_path_.empty());
  }
  if (auto* apply_button = footer_actions_->findChild<QPushButton*>("footerApplyButton")) {
    apply_button->setEnabled(dirty_ || current_config_path_.empty());
  }
  if (auto* cancel_button = footer_actions_->findChild<QPushButton*>("footerCancelButton")) {
    cancel_button->setEnabled(dirty_);
  }
}

bool MainWindow::saveCurrentConfig(bool force_prompt,
                                   QString* error_message,
                                   bool restart_service) {
  QString target_path = suggestedSavePath(current_config_path_);
  if (force_prompt || target_path.isEmpty()) {
    target_path = QFileDialog::getSaveFileName(
        this,
        force_prompt ? "Save config as" : "Save config",
        target_path.isEmpty() ? "sure-smartie.json" : target_path,
        "JSON files (*.json)");
    if (target_path.isEmpty()) {
      if (error_message != nullptr) {
        error_message->clear();
      }
      return false;
    }
  }

  if (restart_service && isSystemConfigPath(toPath(target_path))) {
    return saveConfigFilePrivileged(target_path, true, error_message);
  }

  return saveConfigFile(target_path, error_message);
}

bool MainWindow::saveConfigFilePrivileged(const QString& path,
                                          bool restart_service,
                                          QString* error_message) {
  QMessageBox dialog(this);
  dialog.setWindowTitle("Administrator access required");
  dialog.setIcon(QMessageBox::Question);
  dialog.setText(restart_service
                     ? "Applying the system config requires administrator privileges."
                     : "Saving the system config requires administrator privileges.");
  dialog.setInformativeText(
      QString("Authenticate to %1 %2 using the system password dialog.")
          .arg(restart_service ? "save and apply" : "update")
          .arg(path));
  dialog.setStandardButtons(QMessageBox::Save | QMessageBox::Cancel);
  dialog.setDefaultButton(QMessageBox::Save);
  dialog.button(QMessageBox::Save)
      ->setText(restart_service ? "Authenticate, Save, and Apply"
                                : "Authenticate and Save");

  if (dialog.exec() != QMessageBox::Save) {
    if (error_message != nullptr) {
      error_message->clear();
    }
    return false;
  }

  const QString helper_path = findPrivilegedSaveHelperPath();
  if (helper_path.isEmpty()) {
    if (error_message != nullptr) {
      *error_message =
          "The privileged save helper is not installed. Reinstall sure-smartie-linux.";
    }
    return false;
  }

  const QString pkexec_path = QStandardPaths::findExecutable("pkexec");
  if (pkexec_path.isEmpty()) {
    if (error_message != nullptr) {
      *error_message =
          "pkexec is not available on this system, so administrator save cannot start.";
    }
    return false;
  }

  QTemporaryFile temporary_file(QDir::tempPath() + "/sure-smartie-config-XXXXXX.json");
  temporary_file.setAutoRemove(true);
  if (!temporary_file.open()) {
    if (error_message != nullptr) {
      *error_message = "Unable to create a temporary config file for privileged save.";
    }
    return false;
  }

  const std::string serialized = core::ConfigSerializer::serialize(config_);
  if (temporary_file.write(serialized.data(),
                           static_cast<qint64>(serialized.size())) !=
      static_cast<qint64>(serialized.size())) {
    if (error_message != nullptr) {
      *error_message = "Unable to stage the temporary config for privileged save.";
    }
    return false;
  }
  temporary_file.flush();
  temporary_file.close();

  QProcess process(this);
  process.setProcessChannelMode(QProcess::MergedChannels);
  QStringList arguments;
  arguments << helper_path;
  if (restart_service) {
    arguments << "--restart-service";
  }
  arguments << temporary_file.fileName() << path;
  process.start(pkexec_path, arguments);
  if (!process.waitForStarted()) {
    if (error_message != nullptr) {
      *error_message = "Unable to start the administrator authentication helper.";
    }
    return false;
  }

  process.waitForFinished(-1);
  const QString output =
      QString::fromUtf8(process.readAllStandardOutput()).trimmed();

  if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    if (error_message != nullptr) {
      if (process.exitCode() == 126) {
        *error_message = "Administrator authentication was cancelled.";
      } else if (process.exitCode() == 127) {
        *error_message = output.isEmpty()
                             ? "Administrator authentication failed."
                             : output;
      } else {
        *error_message = output.isEmpty()
                             ? "Privileged save failed."
                             : output;
      }
    }
    return false;
  }

  current_config_path_ = toPath(path);
  setDirty(false);
  statusBar()->showMessage(
      QString("Saved %1 with administrator privileges").arg(path), 3000);
  return true;
}

bool MainWindow::applyCurrentConfig(QString* error_message) {
  const QString target_path = suggestedSavePath(current_config_path_);
  const bool should_restart_service =
      !target_path.isEmpty() && isSystemConfigPath(toPath(target_path));
  if (!saveCurrentConfig(false, error_message, should_restart_service)) {
    return false;
  }

  discardPendingMetricsCollection();
  metrics_service_dirty_ = true;
  ++metrics_epoch_;
  last_metrics_.clear();
  preview_diagnostics_.clear();
  resetRotationPreview();
  refreshPreview(true);
  statusBar()->showMessage(
      should_restart_service ? "Config saved, service restarted, and preview refreshed"
                             : "Config saved and preview refreshed",
      3000);
  return true;
}

void MainWindow::revertCurrentConfig() {
  if (!dirty_) {
    return;
  }

  if (current_config_path_.empty()) {
    discardPendingMetricsCollection();
    config_ = makeDefaultConfig();
    metrics_service_dirty_ = true;
    ++metrics_epoch_;
    last_metrics_.clear();
    preview_diagnostics_.clear();
    rotation_manager_.reset();
    syncUiFromConfig();
    setDirty(false);
    refreshPreview(true);
    statusBar()->showMessage("Reverted to default buffer", 2500);
    return;
  }

  QString error_message;
  if (!loadConfigFile(currentConfigPath(), &error_message)) {
    QMessageBox::critical(this, "Cancel failed", error_message);
    return;
  }

  statusBar()->showMessage("Unsaved changes discarded", 2500);
}

void MainWindow::syncUiFromConfig() {
  const bool previous_updating = updating_ui_;
  updating_ui_ = true;

  syncDisplaySectionFromConfig();
  syncCpuFanSectionFromConfig();
  syncProvidersSectionFromConfig();
  syncPluginSectionFromConfig();
  syncCustomGlyphSectionFromConfig();
  syncScreensSectionFromConfig();

  updating_ui_ = previous_updating;
  updateValidationList();
}

void MainWindow::syncDisplaySectionFromConfig() {
  device_edit_->setText(QString::fromStdString(config_.device));
  refresh_interval_spin_->setValue(static_cast<int>(config_.refresh_interval.count()));
  baudrate_spin_->setValue(config_.baudrate);
  display_type_combo_->setCurrentText(QString::fromStdString(config_.display.type));
  cols_spin_->setValue(config_.display.cols);
  rows_spin_->setValue(config_.display.rows);
  backlight_check_->setChecked(config_.display.backlight);
  contrast_spin_->setValue(config_.display.contrast);
  brightness_spin_->setValue(config_.display.brightness);
}

void MainWindow::syncCpuFanSectionFromConfig() {
  cpu_fan_rpm_path_edit_->setText(QString::fromStdString(config_.cpu_fan.rpm_path));
  cpu_fan_max_rpm_spin_->setValue(config_.cpu_fan.max_rpm);
}

void MainWindow::syncProvidersSectionFromConfig() {
  extra_provider_names_.clear();
  for (const auto& [provider_name, check_box] : provider_checks_) {
    const bool enabled = std::find(config_.providers.begin(),
                                   config_.providers.end(),
                                   provider_name) != config_.providers.end();
    check_box->setChecked(enabled);
  }

  for (const auto& provider_name : config_.providers) {
    if (!providers::isBuiltinProviderName(provider_name)) {
      extra_provider_names_.push_back(provider_name);
    }
  }

  if (extra_provider_names_.empty()) {
    extra_providers_label_->setText("Extra providers: none");
  } else {
    QStringList extra_names;
    for (const auto& provider_name : extra_provider_names_) {
      extra_names << QString::fromStdString(provider_name);
    }
    extra_providers_label_->setText(
        QString("Extra providers preserved on save: %1").arg(extra_names.join(", ")));
  }
}

void MainWindow::syncPluginSectionFromConfig() {
  plugin_paths_list_->clear();
  for (const auto& plugin_path : config_.plugin_paths) {
    plugin_paths_list_->addItem(QString::fromStdString(plugin_path));
  }
}

void MainWindow::syncCustomGlyphSectionFromConfig() {
  int selected_index = selectedCustomGlyphIndex();
  if (selected_index < 0 && !config_.custom_glyphs.empty()) {
    selected_index = 0;
  }
  if (selected_index >= static_cast<int>(config_.custom_glyphs.size())) {
    selected_index = static_cast<int>(config_.custom_glyphs.size()) - 1;
  }

  custom_glyphs_list_->clear();
  for (std::size_t index = 0; index < config_.custom_glyphs.size(); ++index) {
    const auto& glyph = config_.custom_glyphs[index];
    const QString label =
        glyph.name.empty() ? QString("glyph_%1").arg(index + 1)
                           : QString::fromStdString(glyph.name);
    custom_glyphs_list_->addItem(label);
  }

  if (!config_.custom_glyphs.empty() && selected_index >= 0) {
    custom_glyphs_list_->setCurrentRow(selected_index);
  }

  syncCustomGlyphEditorFromSelection();
}

void MainWindow::syncScreensSectionFromConfig() {
  runtime_rotation_check_->setChecked(config_.auto_screen_rotation);

  int selected_index = selectedScreenIndex();
  if (selected_index < 0 && !config_.screens.empty()) {
    selected_index = 0;
  }
  if (selected_index >= static_cast<int>(config_.screens.size())) {
    selected_index = static_cast<int>(config_.screens.size()) - 1;
  }

  screens_list_->clear();
  for (std::size_t index = 0; index < config_.screens.size(); ++index) {
    screens_list_->addItem(screenLabel(config_.screens[index], static_cast<int>(index)));
  }

  updatePreviewScreenChoices();

  if (!config_.screens.empty() && selected_index >= 0) {
    screens_list_->setCurrentRow(selected_index);
  }

  syncScreenEditorFromSelection();
}

void MainWindow::syncScreenEditorFromSelection() {
  const bool previous_updating = updating_ui_;
  updating_ui_ = true;

  const int index = selectedScreenIndex();
  const bool has_selection =
      index >= 0 && static_cast<std::size_t>(index) < config_.screens.size();

  screen_name_edit_->setEnabled(has_selection);
  screen_interval_spin_->setEnabled(has_selection);

  if (has_selection) {
    const auto& screen = config_.screens[static_cast<std::size_t>(index)];
    screen_name_edit_->setText(QString::fromStdString(screen.name));
    screen_interval_spin_->setValue(static_cast<int>(screen.interval.count()));
  } else {
    screen_name_edit_->clear();
    screen_interval_spin_->setValue(0);
  }

  rebuildLineEditors();
  if (has_selection) {
    const auto& screen = config_.screens[static_cast<std::size_t>(index)];
    for (std::size_t line_index = 0; line_index < line_edits_.size(); ++line_index) {
      const QString line_text =
          line_index < screen.lines.size()
              ? QString::fromStdString(screen.lines[line_index])
              : QString{};
      line_edits_[line_index]->setText(line_text);
      line_edits_[line_index]->setEnabled(true);
    }
  } else {
    for (auto* line_edit : line_edits_) {
      line_edit->clear();
      line_edit->setEnabled(false);
    }
  }

  updating_ui_ = previous_updating;
  updateLineLabels();
}

void MainWindow::syncCustomGlyphEditorFromSelection() {
  const bool previous_updating = updating_ui_;
  updating_ui_ = true;

  const int index = selectedCustomGlyphIndex();
  const bool has_selection =
      index >= 0 && static_cast<std::size_t>(index) < config_.custom_glyphs.size();

  custom_glyph_name_edit_->setEnabled(has_selection);
  if (has_selection) {
    const auto& glyph = config_.custom_glyphs[static_cast<std::size_t>(index)];
    custom_glyph_name_edit_->setText(QString::fromStdString(glyph.name));
    for (std::size_t row = 0; row < core::kGlyphHeight; ++row) {
      const int row_bits = glyph.pattern[row];
      for (std::size_t column = 0; column < core::kGlyphWidth; ++column) {
        const int mask = 1 << static_cast<int>(core::kGlyphWidth - column - 1);
        custom_glyph_buttons_[row][column]->setEnabled(true);
        custom_glyph_buttons_[row][column]->setChecked((row_bits & mask) != 0);
      }
    }
  } else {
    custom_glyph_name_edit_->clear();
    for (auto& row_buttons : custom_glyph_buttons_) {
      for (auto* button : row_buttons) {
        button->setEnabled(false);
        button->setChecked(false);
      }
    }
  }

  updating_ui_ = previous_updating;
  updateCustomGlyphTokenHint();
}

void MainWindow::rebuildLineEditors() {
  clearLayout(lines_layout_);
  line_edits_.clear();
  line_length_labels_.clear();

  int editor_row_count = std::max(4, std::max(config_.display.rows, 0));
  const int index = selectedScreenIndex();
  if (index >= 0 && static_cast<std::size_t>(index) < config_.screens.size()) {
    editor_row_count =
        std::max(editor_row_count,
                 static_cast<int>(config_.screens[static_cast<std::size_t>(index)].lines.size()));
  }

  for (int row = 0; row < editor_row_count; ++row) {
    auto* row_widget = new QWidget(this);
    auto* row_layout = new QHBoxLayout(row_widget);
    row_layout->setContentsMargins(0, 0, 0, 0);
    row_layout->setSpacing(8);

    auto* row_label = new QLabel(QString("L%1").arg(row + 1), row_widget);
    row_label->setFixedWidth(28);
    auto* line_edit = new QLineEdit(row_widget);
    line_edit->setPlaceholderText("Display template line");
    auto* length_label = new QLabel(row_widget);
    length_label->setMinimumWidth(70);

    row_layout->addWidget(row_label);
    row_layout->addWidget(line_edit, 1);
    row_layout->addWidget(length_label);
    lines_layout_->addWidget(row_widget);

    line_edits_.push_back(line_edit);
    line_length_labels_.push_back(length_label);

    connect(line_edit, &QLineEdit::textChanged, this, [this, row](const QString& text) {
      if (updating_ui_) {
        return;
      }

      const int screen_index = selectedScreenIndex();
      if (screen_index < 0 ||
          static_cast<std::size_t>(screen_index) >= config_.screens.size()) {
        return;
      }

      auto& lines = config_.screens[static_cast<std::size_t>(screen_index)].lines;
      if (static_cast<std::size_t>(row) >= lines.size()) {
        lines.resize(static_cast<std::size_t>(row + 1));
      }
      lines[static_cast<std::size_t>(row)] = text.toStdString();
      updateLineLabels();
      markConfigEdited(false, false, false);
    });
  }

  lines_layout_->addStretch(1);
}

void MainWindow::updateLineLabels() {
  for (std::size_t index = 0; index < line_edits_.size(); ++index) {
    const auto estimate = engine::TemplateEngine::estimateRenderedWidth(
        line_edits_[index]->text().toStdString());
    const int length = estimate.has_value()
                           ? static_cast<int>(*estimate)
                           : line_edits_[index]->text().size();
    if (config_.display.cols > 0) {
      line_length_labels_[index]->setText(
          QString("est %1/%2").arg(length).arg(config_.display.cols));
      const bool overflow = estimate.has_value() && length > config_.display.cols;
      line_length_labels_[index]->setStyleSheet(
          overflow ? "color: #b42318;" : "color: #667085;");
    } else {
      line_length_labels_[index]->setText(QString::number(length));
      line_length_labels_[index]->setStyleSheet("color: #667085;");
    }
  }
}

void MainWindow::updatePreviewScreenChoices() {
  const bool previous_updating = updating_ui_;
  updating_ui_ = true;

  int current_index = preview_screen_combo_->currentIndex();
  if (current_index < 0) {
    current_index = selectedScreenIndex();
  }
  if (current_index >= static_cast<int>(config_.screens.size())) {
    current_index = static_cast<int>(config_.screens.size()) - 1;
  }

  preview_screen_combo_->clear();
  for (std::size_t index = 0; index < config_.screens.size(); ++index) {
    preview_screen_combo_->addItem(screenLabel(config_.screens[index], static_cast<int>(index)));
  }
  if (!config_.screens.empty() && current_index >= 0) {
    preview_screen_combo_->setCurrentIndex(current_index);
  }

  updating_ui_ = previous_updating;
}

void MainWindow::updateCustomGlyphTokenHint() {
  if (custom_glyph_token_label_ == nullptr) {
    return;
  }

  const int index = selectedCustomGlyphIndex();
  if (index < 0 || static_cast<std::size_t>(index) >= config_.custom_glyphs.size()) {
    custom_glyph_token_label_->setText("Select or create a glyph to use it as {glyph:name}.");
    return;
  }

  const QString name = custom_glyph_name_edit_->text().trimmed();
  custom_glyph_token_label_->setText(
      name.isEmpty()
          ? "Give the glyph a name to reference it from a template."
          : QString("Use in templates: {glyph:%1}").arg(name));
}

void MainWindow::updateValidationList() {
  validation_list_->clear();

  auto diagnostics = core::ConfigValidator::validate(config_);
  diagnostics.insert(diagnostics.end(),
                     preview_diagnostics_.begin(),
                     preview_diagnostics_.end());

  if (diagnostics.empty()) {
    auto* item = new QListWidgetItem("[info] config is valid");
    item->setForeground(severityColor(core::DiagnosticSeverity::info));
    validation_list_->addItem(item);
    return;
  }

  for (const auto& diagnostic : diagnostics) {
    auto* item = new QListWidgetItem(diagnosticText(diagnostic));
    item->setForeground(severityColor(diagnostic.severity));
    item->setData(Qt::UserRole, QString::fromStdString(diagnostic.field_path));
    validation_list_->addItem(item);
  }
}

void MainWindow::rebuildMetricsService() {
  metrics_service_ = std::make_unique<core::MetricsSnapshotService>(config_);
  preview_diagnostics_ = metrics_service_->lastDiagnostics();
  metrics_service_dirty_ = false;
}

void MainWindow::startAsyncMetricsCollection() {
  if (metrics_collection_in_flight_) {
    return;
  }

  if (metrics_service_dirty_) {
    rebuildMetricsService();
  }
  if (metrics_service_ == nullptr) {
    return;
  }

  auto* service = metrics_service_.get();
  const auto epoch = metrics_epoch_;
  metrics_collection_in_flight_ = true;
  pending_metrics_future_ = std::async(
      std::launch::async,
      [service, epoch]() -> std::tuple<std::uint64_t,
                                       core::MetricMap,
                                       std::vector<core::Diagnostic>> {
        auto metrics = service->collect();
        auto diagnostics = service->lastDiagnostics();
        return {epoch, std::move(metrics), std::move(diagnostics)};
      });
}

bool MainWindow::applyPendingMetricsCollectionIfReady(bool block) {
  if (!metrics_collection_in_flight_ || !pending_metrics_future_.valid()) {
    return false;
  }

  if (!block &&
      pending_metrics_future_.wait_for(std::chrono::milliseconds{0}) !=
          std::future_status::ready) {
    return false;
  }

  auto [epoch, metrics, diagnostics] = pending_metrics_future_.get();
  metrics_collection_in_flight_ = false;
  if (epoch != metrics_epoch_ || metrics_service_dirty_) {
    return false;
  }

  last_metrics_ = std::move(metrics);
  preview_diagnostics_ = std::move(diagnostics);
  return true;
}

void MainWindow::discardPendingMetricsCollection() {
  if (!metrics_collection_in_flight_) {
    return;
  }
  if (pending_metrics_future_.valid()) {
    pending_metrics_future_.wait();
    pending_metrics_future_.get();
  }
  metrics_collection_in_flight_ = false;
}

void MainWindow::requestAsyncPreviewRefresh() {
  applyPendingMetricsCollectionIfReady(false);
  if (!metrics_collection_in_flight_) {
    startAsyncMetricsCollection();
  }
  refreshPreview(false);
}

void MainWindow::resetRotationPreview() {
  rotation_manager_.reset();
  if (config_.screens.empty()) {
    return;
  }

  try {
    rotation_manager_ = std::make_unique<engine::ScreenManager>(config_.screens);
  } catch (const std::exception& error) {
    preview_diagnostics_.push_back(core::Diagnostic{
        .severity = core::DiagnosticSeverity::warning,
        .field_path = "screens",
        .message = "rotation preview unavailable: " + std::string(error.what()),
    });
  }
}

void MainWindow::refreshPreview(bool recollect_metrics) {
  if (recollect_metrics) {
    discardPendingMetricsCollection();
  } else {
    applyPendingMetricsCollectionIfReady(false);
  }

  if (metrics_service_dirty_ && (recollect_metrics || !metrics_collection_in_flight_)) {
    rebuildMetricsService();
  }

  if (recollect_metrics || (last_metrics_.empty() && !metrics_collection_in_flight_)) {
    if (metrics_service_ != nullptr) {
      last_metrics_ = metrics_service_->collect();
      preview_diagnostics_ = metrics_service_->lastDiagnostics();
    } else {
      last_metrics_.clear();
      preview_diagnostics_.clear();
    }
  }

  const auto geometry = core::normalizedGeometry(config_.display);
  core::RenderedFrame rendered;
  rendered.frame = blankFrame();
  QString status = "Preview ready";

  try {
    if (config_.screens.empty()) {
      status = "No screens configured";
    } else if (rotation_preview_check_->isChecked()) {
      if (rotation_manager_ == nullptr) {
        resetRotationPreview();
      }
      if (rotation_manager_ != nullptr) {
        const auto& screen = rotation_manager_->current(std::chrono::steady_clock::now());
        rendered =
            preview_renderer_.renderDetailed(screen, geometry, last_metrics_, config_.custom_glyphs);
        status = QString("Rotation preview: %1").arg(QString::fromStdString(screen.name));
      }
    } else {
      int preview_index = preview_screen_combo_->currentIndex();
      if (preview_index < 0) {
        preview_index = selectedScreenIndex();
      }
      if (preview_index >= 0 &&
          static_cast<std::size_t>(preview_index) < config_.screens.size()) {
        rendered = preview_renderer_.renderScreenDetailed(config_, last_metrics_, preview_index);
        status = QString("Previewing: %1").arg(
            screenLabel(config_.screens[static_cast<std::size_t>(preview_index)],
                        preview_index));
      }
    }
  } catch (const std::exception& error) {
    preview_diagnostics_.push_back(core::Diagnostic{
        .severity = core::DiagnosticSeverity::warning,
        .field_path = "preview",
        .message = "preview render failed: " + std::string(error.what()),
    });
    rendered.frame = blankFrame();
    rendered.glyphs = {};
    status = "Preview unavailable";
  }

  if (rotation_preview_check_->isChecked() && config_.screens.size() > 1) {
    status += QString(" | %1 screens").arg(config_.screens.size());
  }
  if (metrics_collection_in_flight_) {
    status += " | refreshing metrics";
  }
  if (diagnosticsHaveErrors(core::ConfigValidator::validate(config_))) {
    status += " | invalid config";
  }

  preview_status_label_->setText(status);
  preview_widget_->setFrame(geometry, std::move(rendered.frame), std::move(rendered.glyphs));
  updateValidationList();
}

core::Frame MainWindow::blankFrame() const {
  const auto geometry = core::normalizedGeometry(config_.display);
  return core::Frame(geometry.rows, std::string(geometry.cols, ' '));
}

int MainWindow::selectedScreenIndex() const {
  return screens_list_ != nullptr ? screens_list_->currentRow() : -1;
}

int MainWindow::selectedCustomGlyphIndex() const {
  return custom_glyphs_list_ != nullptr ? custom_glyphs_list_->currentRow() : -1;
}

void MainWindow::setSelectedScreen(int index) {
  if (screens_list_ == nullptr) {
    return;
  }

  const bool previous_updating = updating_ui_;
  updating_ui_ = true;
  screens_list_->setCurrentRow(index);
  updating_ui_ = previous_updating;
  syncScreenEditorFromSelection();
}

void MainWindow::setSelectedCustomGlyph(int index) {
  if (custom_glyphs_list_ == nullptr) {
    return;
  }

  const bool previous_updating = updating_ui_;
  updating_ui_ = true;
  custom_glyphs_list_->setCurrentRow(index);
  updating_ui_ = previous_updating;
  syncCustomGlyphEditorFromSelection();
}

void MainWindow::addScreen() {
  core::ScreenDefinition screen;
  screen.name = "screen_" + std::to_string(config_.screens.size() + 1);
  screen.interval = std::chrono::milliseconds{2000};
  screen.lines.resize(static_cast<std::size_t>(std::max(config_.display.rows, 4)));

  config_.screens.push_back(std::move(screen));
  syncScreensSectionFromConfig();
  setSelectedScreen(static_cast<int>(config_.screens.size() - 1));
  markConfigEdited(false, false, true);
}

void MainWindow::removeSelectedScreen() {
  const int index = selectedScreenIndex();
  if (index < 0 || static_cast<std::size_t>(index) >= config_.screens.size()) {
    return;
  }

  config_.screens.erase(config_.screens.begin() + index);
  syncScreensSectionFromConfig();
  if (!config_.screens.empty()) {
    setSelectedScreen(std::min(index, static_cast<int>(config_.screens.size() - 1)));
  }
  markConfigEdited(false, false, true);
}

void MainWindow::duplicateSelectedScreen() {
  const int index = selectedScreenIndex();
  if (index < 0 || static_cast<std::size_t>(index) >= config_.screens.size()) {
    return;
  }

  auto copy = config_.screens[static_cast<std::size_t>(index)];
  copy.name += "_copy";
  config_.screens.insert(config_.screens.begin() + index + 1, std::move(copy));
  syncScreensSectionFromConfig();
  setSelectedScreen(index + 1);
  markConfigEdited(false, false, true);
}

void MainWindow::moveSelectedScreen(int delta) {
  const int index = selectedScreenIndex();
  const int target = index + delta;
  if (index < 0 || target < 0 ||
      static_cast<std::size_t>(index) >= config_.screens.size() ||
      static_cast<std::size_t>(target) >= config_.screens.size()) {
    return;
  }

  std::swap(config_.screens[static_cast<std::size_t>(index)],
            config_.screens[static_cast<std::size_t>(target)]);
  syncScreensSectionFromConfig();
  setSelectedScreen(target);
  markConfigEdited(false, false, true);
}

void MainWindow::addCustomGlyph() {
  core::CustomGlyphDefinition glyph;
  glyph.name = "glyph_" + std::to_string(config_.custom_glyphs.size() + 1);
  config_.custom_glyphs.push_back(std::move(glyph));
  syncCustomGlyphSectionFromConfig();
  setSelectedCustomGlyph(static_cast<int>(config_.custom_glyphs.size() - 1));
  markConfigEdited(false, false, false);
}

void MainWindow::removeSelectedCustomGlyph() {
  const int index = selectedCustomGlyphIndex();
  if (index < 0 || static_cast<std::size_t>(index) >= config_.custom_glyphs.size()) {
    return;
  }

  config_.custom_glyphs.erase(config_.custom_glyphs.begin() + index);
  syncCustomGlyphSectionFromConfig();
  if (!config_.custom_glyphs.empty()) {
    setSelectedCustomGlyph(std::min(index, static_cast<int>(config_.custom_glyphs.size() - 1)));
  }
  markConfigEdited(false, false, false);
}

void MainWindow::addPluginPaths() {
  const QStringList paths = QFileDialog::getOpenFileNames(
      this, "Add plugin libraries", QString{}, "Shared libraries (*.so);;All files (*)");
  if (paths.isEmpty()) {
    return;
  }

  for (const QString& path : paths) {
    config_.plugin_paths.push_back(path.toStdString());
  }
  syncPluginSectionFromConfig();
  markConfigEdited(true, true, false);
}

void MainWindow::removeSelectedPluginPath() {
  const int row = plugin_paths_list_->currentRow();
  if (row < 0 || static_cast<std::size_t>(row) >= config_.plugin_paths.size()) {
    return;
  }

  config_.plugin_paths.erase(config_.plugin_paths.begin() + row);
  syncPluginSectionFromConfig();
  markConfigEdited(true, true, false);
}

void MainWindow::focusFieldPath(const QString& field_path) {
  if (field_path.isEmpty()) {
    return;
  }

  if (field_path == "device") {
    device_edit_->setFocus();
    return;
  }
  if (field_path == "baudrate") {
    baudrate_spin_->setFocus();
    return;
  }
  if (field_path == "refresh_ms") {
    refresh_interval_spin_->setFocus();
    return;
  }
  if (field_path == "display.type") {
    display_type_combo_->setFocus();
    return;
  }
  if (field_path == "display.cols") {
    cols_spin_->setFocus();
    return;
  }
  if (field_path == "display.rows") {
    rows_spin_->setFocus();
    return;
  }
  if (field_path == "display.contrast") {
    contrast_spin_->setFocus();
    return;
  }
  if (field_path == "display.brightness") {
    brightness_spin_->setFocus();
    return;
  }
  if (field_path == "cpu_fan.rpm_path") {
    cpu_fan_rpm_path_edit_->setFocus();
    return;
  }
  if (field_path == "cpu_fan.max_rpm") {
    cpu_fan_max_rpm_spin_->setFocus();
    return;
  }

  if (const auto provider_field = parseIndexedField(field_path, "providers");
      provider_field.has_value()) {
    const int index = provider_field->first;
    if (index >= 0 && static_cast<std::size_t>(index) < config_.providers.size()) {
      const auto& provider_name = config_.providers[static_cast<std::size_t>(index)];
      for (const auto& [builtin_name, check_box] : provider_checks_) {
        if (builtin_name == provider_name) {
          check_box->setFocus();
          return;
        }
      }
    }
    return;
  }

  if (field_path.startsWith("plugin_paths[")) {
    plugin_paths_list_->setFocus();
    return;
  }

  if (const auto glyph_field = parseIndexedField(field_path, "custom_glyphs");
      glyph_field.has_value()) {
    const int glyph_index = glyph_field->first;
    const QString suffix = glyph_field->second;
    if (glyph_index >= 0 &&
        static_cast<std::size_t>(glyph_index) < config_.custom_glyphs.size()) {
      setSelectedCustomGlyph(glyph_index);
      if (suffix == ".name") {
        custom_glyph_name_edit_->setFocus();
        return;
      }
      if (suffix == ".rows" || suffix.startsWith(".rows[")) {
        custom_glyph_buttons_.front().front()->setFocus();
        return;
      }
    }
    return;
  }

  if (const auto screen_field = parseIndexedField(field_path, "screens");
      screen_field.has_value()) {
    const int screen_index = screen_field->first;
    const QString suffix = screen_field->second;
    if (screen_index >= 0 &&
        static_cast<std::size_t>(screen_index) < config_.screens.size()) {
      setSelectedScreen(screen_index);
      if (suffix == ".name") {
        screen_name_edit_->setFocus();
        return;
      }
      if (suffix == ".interval_ms") {
        screen_interval_spin_->setFocus();
        return;
      }
      if (suffix == ".lines" || suffix.startsWith(".lines[")) {
        if (const auto line_field = parseIndexedField(suffix.mid(1), "lines");
            line_field.has_value()) {
          const int line_index = line_field->first;
          if (line_index >= 0 &&
              static_cast<std::size_t>(line_index) < line_edits_.size()) {
            line_edits_[static_cast<std::size_t>(line_index)]->setFocus();
            return;
          }
        }
        if (!line_edits_.empty()) {
          line_edits_.front()->setFocus();
        }
      }
    }
  }
}

void MainWindow::markConfigEdited(bool metrics_service_changed,
                                  bool recollect_preview,
                                  bool rebuild_rotation_preview) {
  setDirty(true);
  if (metrics_service_changed) {
    discardPendingMetricsCollection();
    metrics_service_dirty_ = true;
    ++metrics_epoch_;
    preview_diagnostics_.clear();
  }
  if (rebuild_rotation_preview) {
    resetRotationPreview();
    updatePreviewScreenChoices();
  }
  refreshPreview(recollect_preview || metrics_service_changed);
}

}  // namespace sure_smartie::gui
