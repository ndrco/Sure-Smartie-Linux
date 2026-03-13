#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

#include "sure_smartie/providers/IProvider.hpp"

namespace sure_smartie::providers {

class CpuProvider : public IProvider {
 public:
  explicit CpuProvider(core::CpuFanConfig fan_config = {});

  std::string name() const override;
  void collect(core::MetricMap& metrics) override;

 private:
  struct CpuTimes {
    std::uint64_t user{0};
    std::uint64_t nice{0};
    std::uint64_t system{0};
    std::uint64_t idle{0};
    std::uint64_t iowait{0};
    std::uint64_t irq{0};
    std::uint64_t softirq{0};
    std::uint64_t steal{0};

    [[nodiscard]] std::uint64_t total() const;
    [[nodiscard]] std::uint64_t active() const;
  };

  struct EnergySample {
    std::string source;
    std::uint64_t microjoules{0};
    std::chrono::steady_clock::time_point timestamp;
  };

  static std::optional<CpuTimes> readCpuTimes();
  static std::string readCpuTemperature();
  static std::string readCpuClock();

  core::CpuFanConfig fan_config_;
  std::optional<CpuTimes> previous_sample_;
  std::optional<EnergySample> previous_energy_sample_;
  double last_load_percent_{0.0};
};

}  // namespace sure_smartie::providers
