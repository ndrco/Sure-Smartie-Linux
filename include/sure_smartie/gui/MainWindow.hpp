#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <QMainWindow>
#include <QString>

#include "sure_smartie/core/PreviewFrameRenderer.hpp"
#include "sure_smartie/core/Types.hpp"

class QCheckBox;
class QCloseEvent;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QSpinBox;
class QTimer;
class QVBoxLayout;
class QWidget;

namespace sure_smartie::core {
class MetricsSnapshotService;
}

namespace sure_smartie::engine {
class ScreenManager;
}

namespace sure_smartie::gui {

class LcdPreviewWidget;

class MainWindow : public QMainWindow {
 public:
  explicit MainWindow(QWidget* parent = nullptr);

  bool loadConfigFile(const QString& path, QString* error_message = nullptr);
  bool saveConfigFile(const QString& path, QString* error_message = nullptr);

  bool isDirty() const;
  QString currentConfigPath() const;
  void setScreenLine(int screen_index, int line_index, const QString& text);
  QString previewLine(int row) const;
  void refreshPreviewNow();

 protected:
  void closeEvent(QCloseEvent* event) override;

 private:
  void buildUi();
  void applyStyle();
  void updateWindowTitle();
  void updateProjectSummary();
  void setDirty(bool dirty);
  bool maybeDiscardChanges();

  void syncUiFromConfig();
  void syncDisplaySectionFromConfig();
  void syncProvidersSectionFromConfig();
  void syncPluginSectionFromConfig();
  void syncScreensSectionFromConfig();
  void syncScreenEditorFromSelection();
  void rebuildLineEditors();
  void updateLineLabels();
  void updatePreviewScreenChoices();
  void updateValidationList();

  void rebuildMetricsService();
  void resetRotationPreview();
  void refreshPreview(bool recollect_metrics);
  core::Frame blankFrame() const;

  int selectedScreenIndex() const;
  void setSelectedScreen(int index);
  void addScreen();
  void removeSelectedScreen();
  void duplicateSelectedScreen();
  void moveSelectedScreen(int delta);
  void addPluginPaths();
  void removeSelectedPluginPath();
  void focusFieldPath(const QString& field_path);

  void markConfigEdited(bool metrics_service_changed,
                        bool recollect_preview,
                        bool rebuild_rotation_preview);

  core::AppConfig config_;
  std::filesystem::path current_config_path_;
  bool dirty_{false};
  bool updating_ui_{false};
  bool metrics_service_dirty_{true};
  core::MetricMap last_metrics_;
  std::vector<core::Diagnostic> preview_diagnostics_;
  std::vector<std::string> extra_provider_names_;
  std::unique_ptr<core::MetricsSnapshotService> metrics_service_;
  std::unique_ptr<engine::ScreenManager> rotation_manager_;
  core::PreviewFrameRenderer preview_renderer_;

  QLabel* project_path_label_{nullptr};
  QLabel* dirty_state_label_{nullptr};
  QLabel* extra_providers_label_{nullptr};
  QLabel* preview_status_label_{nullptr};

  QLineEdit* device_edit_{nullptr};
  QSpinBox* refresh_interval_spin_{nullptr};
  QSpinBox* baudrate_spin_{nullptr};
  QComboBox* display_type_combo_{nullptr};
  QSpinBox* cols_spin_{nullptr};
  QSpinBox* rows_spin_{nullptr};
  QCheckBox* backlight_check_{nullptr};
  QSpinBox* contrast_spin_{nullptr};
  QSpinBox* brightness_spin_{nullptr};

  std::vector<std::pair<std::string, QCheckBox*>> provider_checks_;
  QListWidget* plugin_paths_list_{nullptr};
  QListWidget* screens_list_{nullptr};
  QLineEdit* screen_name_edit_{nullptr};
  QSpinBox* screen_interval_spin_{nullptr};
  QVBoxLayout* lines_layout_{nullptr};
  std::vector<QLineEdit*> line_edits_;
  std::vector<QLabel*> line_length_labels_;

  QComboBox* preview_screen_combo_{nullptr};
  QCheckBox* rotation_preview_check_{nullptr};
  QSpinBox* preview_refresh_spin_{nullptr};
  QTimer* preview_timer_{nullptr};
  LcdPreviewWidget* preview_widget_{nullptr};
  QListWidget* validation_list_{nullptr};
};

}  // namespace sure_smartie::gui
