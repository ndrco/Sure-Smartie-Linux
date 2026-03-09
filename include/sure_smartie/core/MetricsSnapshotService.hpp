#pragma once

#include <memory>
#include <vector>

#include "sure_smartie/core/Types.hpp"

namespace sure_smartie::providers {
class IProvider;
}

namespace sure_smartie::core {

class MetricsSnapshotService {
 public:
  explicit MetricsSnapshotService(const AppConfig& config);

  MetricMap collect();
  const std::vector<Diagnostic>& lastDiagnostics() const;

 private:
  std::vector<std::unique_ptr<providers::IProvider>> providers_;
  std::vector<Diagnostic> base_diagnostics_;
  std::vector<Diagnostic> last_diagnostics_;
};

}  // namespace sure_smartie::core
