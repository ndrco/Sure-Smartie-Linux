#include "sure_smartie/gui/MainWindow.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>

#include <QAction>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QFileDialog>
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
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QStringList>
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

MainWindow::~MainWindow() = default;

bool MainWindow::loadConfigFile(const QString& path, QString* error_message) {
  try {
    config_ = core::ConfigLoader::loadFromFile(toPath(path));
    current_config_path_ = toPath(path);
    metrics_service_dirty_ = true;
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
    if (error_message != nullptr) {
      *error_message = error.what();
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
    QString target_path = currentConfigPath();
    if (target_path.isEmpty()) {
      target_path = QFileDialog::getSaveFileName(
          this, "Save config", "sure-smartie.json", "JSON files (*.json)");
      if (target_path.isEmpty()) {
        return;
      }
    }

    QString error_message;
    if (!saveConfigFile(target_path, &error_message)) {
      QMessageBox::critical(this, "Save failed", error_message);
    }
  });

  connect(save_as_action, &QAction::triggered, this, [this]() {
    const QString target_path = QFileDialog::getSaveFileName(
        this, "Save config as", currentConfigPath().isEmpty() ? "sure-smartie.json"
                                                              : currentConfigPath(),
        "JSON files (*.json)");
    if (target_path.isEmpty()) {
      return;
    }

    QString error_message;
    if (!saveConfigFile(target_path, &error_message)) {
      QMessageBox::critical(this, "Save failed", error_message);
    }
  });

  connect(quit_action, &QAction::triggered, this, [this]() { close(); });

  auto* central = new QWidget(this);
  auto* root_layout = new QHBoxLayout(central);
  root_layout->setContentsMargins(12, 12, 12, 12);

  auto* splitter = new QSplitter(Qt::Horizontal, central);
  root_layout->addWidget(splitter);
  setCentralWidget(central);

  auto* left_scroll = new QScrollArea(splitter);
  left_scroll->setWidgetResizable(true);
  left_scroll->setFrameShape(QFrame::NoFrame);

  auto* editor_root = new QWidget(left_scroll);
  auto* editor_layout = new QVBoxLayout(editor_root);
  editor_layout->setContentsMargins(0, 0, 12, 0);
  editor_layout->setSpacing(14);
  left_scroll->setWidget(editor_root);

  auto* project_group = new QGroupBox("Project", editor_root);
  auto* project_layout = new QVBoxLayout(project_group);
  project_path_label_ = new QLabel(project_group);
  dirty_state_label_ = new QLabel(project_group);
  project_layout->addWidget(project_path_label_);
  project_layout->addWidget(dirty_state_label_);
  editor_layout->addWidget(project_group);

  auto* display_group = new QGroupBox("Display Settings", editor_root);
  auto* display_layout = new QFormLayout(display_group);
  device_edit_ = new QLineEdit(display_group);
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

  auto* providers_group = new QGroupBox("Providers & Plugins", editor_root);
  auto* providers_layout = new QVBoxLayout(providers_group);
  auto* provider_grid = new QGridLayout();
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
  extra_providers_label_->setWordWrap(true);
  providers_layout->addWidget(extra_providers_label_);
  providers_layout->addWidget(new QLabel("Plugin paths", providers_group));
  plugin_paths_list_ = new QListWidget(providers_group);
  plugin_paths_list_->setSelectionMode(QAbstractItemView::SingleSelection);
  providers_layout->addWidget(plugin_paths_list_);
  auto* plugin_buttons = new QHBoxLayout();
  auto* add_plugin_button = new QPushButton("Add plugin", providers_group);
  auto* remove_plugin_button = new QPushButton("Remove plugin", providers_group);
  plugin_buttons->addWidget(add_plugin_button);
  plugin_buttons->addWidget(remove_plugin_button);
  providers_layout->addLayout(plugin_buttons);
  editor_layout->addWidget(providers_group);

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
  screen_form->addRow("Name", screen_name_edit_);
  screen_form->addRow("Interval ms", screen_interval_spin_);
  screen_editor_column->addLayout(screen_form);
  auto* lines_group = new QGroupBox("Lines", screens_group);
  lines_layout_ = new QVBoxLayout(lines_group);
  screen_editor_column->addWidget(lines_group);
  screens_layout->addLayout(screen_editor_column, 2);
  editor_layout->addWidget(screens_group);
  editor_layout->addStretch(1);

  auto* right_panel = new QWidget(splitter);
  auto* right_layout = new QVBoxLayout(right_panel);
  right_layout->setContentsMargins(8, 0, 0, 0);
  right_layout->setSpacing(14);

  auto* preview_group = new QGroupBox("Preview", right_panel);
  auto* preview_layout = new QVBoxLayout(preview_group);
  auto* preview_controls = new QHBoxLayout();
  preview_screen_combo_ = new QComboBox(preview_group);
  rotation_preview_check_ = new QCheckBox("Rotation preview", preview_group);
  preview_refresh_spin_ = new QSpinBox(preview_group);
  preview_refresh_spin_->setRange(200, 10000);
  preview_refresh_spin_->setSingleStep(100);
  preview_refresh_spin_->setValue(1000);
  auto* refresh_button = new QPushButton("Refresh now", preview_group);
  preview_controls->addWidget(new QLabel("Screen", preview_group));
  preview_controls->addWidget(preview_screen_combo_, 1);
  preview_controls->addWidget(rotation_preview_check_);
  preview_controls->addWidget(new QLabel("Refresh ms", preview_group));
  preview_controls->addWidget(preview_refresh_spin_);
  preview_controls->addWidget(refresh_button);
  preview_layout->addLayout(preview_controls);
  preview_status_label_ = new QLabel(preview_group);
  preview_layout->addWidget(preview_status_label_);
  preview_widget_ = new LcdPreviewWidget(preview_group);
  preview_layout->addWidget(preview_widget_, 1);
  right_layout->addWidget(preview_group, 2);

  auto* validation_group = new QGroupBox("Validation", right_panel);
  auto* validation_layout = new QVBoxLayout(validation_group);
  validation_list_ = new QListWidget(validation_group);
  validation_layout->addWidget(validation_list_);
  right_layout->addWidget(validation_group, 1);

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
  connect(preview_timer_, &QTimer::timeout, this, [this]() { refreshPreview(true); });

  connect(validation_list_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
    focusFieldPath(item->data(Qt::UserRole).toString());
  });

  statusBar()->showMessage("Ready", 1500);
}

void MainWindow::applyStyle() {
  setStyleSheet(
      "QMainWindow { background: #f5efe4; }"
      "QGroupBox {"
      "  font-weight: 600;"
      "  border: 1px solid #d8ccb5;"
      "  border-radius: 12px;"
      "  margin-top: 10px;"
      "  background: #fffaf0;"
      "}"
      "QGroupBox::title {"
      "  subcontrol-origin: margin;"
      "  left: 14px;"
      "  padding: 0 6px;"
      "  color: #5b4630;"
      "}"
      "QLineEdit, QSpinBox, QComboBox, QListWidget {"
      "  background: #fffdf8;"
      "  border: 1px solid #d7ccb7;"
      "  border-radius: 8px;"
      "  padding: 6px 8px;"
      "}"
      "QPushButton {"
      "  background: #274c45;"
      "  color: #f7f4ea;"
      "  border: none;"
      "  border-radius: 8px;"
      "  padding: 8px 12px;"
      "}"
      "QPushButton:hover { background: #1f3d38; }"
      "QPushButton:pressed { background: #17302d; }"
      "QLabel { color: #443422; }");
}

void MainWindow::updateWindowTitle() {
  const QString name = current_config_path_.empty() ? "untitled"
                                                    : toQString(current_config_path_.filename());
  const QString dirty_marker = dirty_ ? " *" : "";
  setWindowTitle(QString("sure-smartie-gui | %1%2").arg(name, dirty_marker));
}

void MainWindow::updateProjectSummary() {
  const QString path_text =
      current_config_path_.empty() ? "Config: unsaved buffer"
                                   : QString("Config: %1").arg(currentConfigPath());
  project_path_label_->setText(path_text);
  dirty_state_label_->setText(dirty_ ? "State: modified" : "State: saved");
}

void MainWindow::setDirty(bool dirty) {
  dirty_ = dirty;
  updateProjectSummary();
  updateWindowTitle();
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

  QString target_path = currentConfigPath();
  if (target_path.isEmpty()) {
    target_path = QFileDialog::getSaveFileName(
        this, "Save config", "sure-smartie.json", "JSON files (*.json)");
    if (target_path.isEmpty()) {
      return false;
    }
  }

  QString error_message;
  if (!saveConfigFile(target_path, &error_message)) {
    QMessageBox::critical(this, "Save failed", error_message);
    return false;
  }

  return true;
}

void MainWindow::syncUiFromConfig() {
  const bool previous_updating = updating_ui_;
  updating_ui_ = true;

  syncDisplaySectionFromConfig();
  syncProvidersSectionFromConfig();
  syncPluginSectionFromConfig();
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

void MainWindow::syncScreensSectionFromConfig() {
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
  if (metrics_service_dirty_) {
    rebuildMetricsService();
  }

  if (recollect_metrics || last_metrics_.empty()) {
    if (metrics_service_ != nullptr) {
      last_metrics_ = metrics_service_->collect();
      preview_diagnostics_ = metrics_service_->lastDiagnostics();
    } else {
      last_metrics_.clear();
      preview_diagnostics_.clear();
    }
  }

  const auto geometry = core::normalizedGeometry(config_.display);
  auto frame = blankFrame();
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
        frame = preview_renderer_.render(screen, geometry, last_metrics_);
        status = QString("Rotation preview: %1").arg(QString::fromStdString(screen.name));
      }
    } else {
      int preview_index = preview_screen_combo_->currentIndex();
      if (preview_index < 0) {
        preview_index = selectedScreenIndex();
      }
      if (preview_index >= 0 &&
          static_cast<std::size_t>(preview_index) < config_.screens.size()) {
        frame = preview_renderer_.renderScreen(config_, last_metrics_, preview_index);
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
    frame = blankFrame();
    status = "Preview unavailable";
  }

  if (rotation_preview_check_->isChecked() && config_.screens.size() > 1) {
    status += QString(" | %1 screens").arg(config_.screens.size());
  }
  if (diagnosticsHaveErrors(core::ConfigValidator::validate(config_))) {
    status += " | invalid config";
  }

  preview_status_label_->setText(status);
  preview_widget_->setFrame(geometry, std::move(frame));
  updateValidationList();
}

core::Frame MainWindow::blankFrame() const {
  const auto geometry = core::normalizedGeometry(config_.display);
  return core::Frame(geometry.rows, std::string(geometry.cols, ' '));
}

int MainWindow::selectedScreenIndex() const {
  return screens_list_ != nullptr ? screens_list_->currentRow() : -1;
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
    metrics_service_dirty_ = true;
    preview_diagnostics_.clear();
  }
  if (rebuild_rotation_preview) {
    resetRotationPreview();
    updatePreviewScreenChoices();
  }
  refreshPreview(recollect_preview || metrics_service_changed);
}

}  // namespace sure_smartie::gui
