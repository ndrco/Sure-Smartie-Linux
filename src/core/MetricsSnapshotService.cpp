#include "sure_smartie/core/MetricsSnapshotService.hpp"

#include <exception>
#include <iterator>
#include <string>
#include <unordered_set>

#include "sure_smartie/plugins/PluginLoader.hpp"
#include "sure_smartie/providers/BuiltinProviderFactory.hpp"

namespace sure_smartie::core {

MetricsSnapshotService::MetricsSnapshotService(const AppConfig& config) {
  std::unordered_set<std::string> seen_builtin;

  for (const auto& provider_name : config.providers) {
    if (!seen_builtin.insert(provider_name).second) {
      continue;
    }

    try {
      providers_.push_back(providers::createBuiltinProvider(provider_name));
    } catch (const std::exception& error) {
      base_diagnostics_.push_back(Diagnostic{
          .severity = DiagnosticSeverity::warning,
          .field_path = "providers",
          .message = "failed to initialize provider '" + provider_name + "': " +
                     error.what(),
      });
    }
  }

  for (const auto& plugin_path : config.plugin_paths) {
    try {
      auto plugin_providers = plugins::loadProviderPlugins({plugin_path});
      providers_.insert(providers_.end(),
                        std::make_move_iterator(plugin_providers.begin()),
                        std::make_move_iterator(plugin_providers.end()));
    } catch (const std::exception& error) {
      base_diagnostics_.push_back(Diagnostic{
          .severity = DiagnosticSeverity::warning,
          .field_path = "plugin_paths",
          .message = "failed to load plugin '" + plugin_path + "': " + error.what(),
      });
    }
  }

  last_diagnostics_ = base_diagnostics_;
}

MetricMap MetricsSnapshotService::collect() {
  last_diagnostics_ = base_diagnostics_;
  MetricMap metrics;

  for (const auto& provider : providers_) {
    try {
      provider->collect(metrics);
    } catch (const std::exception& error) {
      last_diagnostics_.push_back(Diagnostic{
          .severity = DiagnosticSeverity::warning,
          .field_path = "providers",
          .message = "provider '" + provider->name() + "' failed: " + error.what(),
      });
    }
  }

  return metrics;
}

const std::vector<Diagnostic>& MetricsSnapshotService::lastDiagnostics() const {
  return last_diagnostics_;
}

}  // namespace sure_smartie::core
