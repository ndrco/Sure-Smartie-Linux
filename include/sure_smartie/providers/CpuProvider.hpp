#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

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
  std::string readCpuTemperature();
  std::string readCpuClock();
  std::optional<std::filesystem::path> resolvedFanPath();
  std::optional<std::filesystem::path> resolvedCpuTemperaturePath();
  std::optional<std::filesystem::path> resolvedCpuEnergyPath();
  void resolveCpuClockPaths();

  core::CpuFanConfig fan_config_;
  std::optional<CpuTimes> previous_sample_;
  std::optional<EnergySample> previous_energy_sample_;
  double last_load_percent_{0.0};
  std::optional<std::filesystem::path> fan_path_;
  std::optional<std::filesystem::path> cpu_temperature_path_;
  std::optional<std::filesystem::path> cpu_energy_path_;
  std::vector<std::filesystem::path> cpu_clock_paths_;
  bool cpu_clock_paths_resolved_{false};
};

}  // namespace sure_smartie::providers
