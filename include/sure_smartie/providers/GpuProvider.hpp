#pragma once

#include <chrono>
#include <string>

#include "sure_smartie/providers/IProvider.hpp"

namespace sure_smartie::providers {

class GpuProvider : public IProvider {
 public:
  std::string name() const override;
  void collect(core::MetricMap& metrics) override;

 private:
  std::chrono::steady_clock::time_point last_sample_time_{};
  core::MetricMap cached_metrics_;
  bool has_cached_sample_{false};
};

}  // namespace sure_smartie::providers
