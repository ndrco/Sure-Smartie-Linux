#include "sure_smartie/providers/CpuProvider.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

namespace sure_smartie::providers {
namespace {

bool isHwmonDirectoryName(const std::string& name) {
  return name.size() > 5 && name.rfind("hwmon", 0) == 0 &&
         std::all_of(name.begin() + 5, name.end(), [](unsigned char ch) {
           return std::isdigit(ch) != 0;
         });
}

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t");
  if (first == std::string::npos) {
    return {};
  }
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::optional<std::string> readTextFile(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) {
    return std::nullopt;
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();
  return trim(buffer.str());
}

std::optional<long long> readIntegerFile(const std::filesystem::path& path) {
  const auto content = readTextFile(path);
  if (!content.has_value()) {
    return std::nullopt;
  }

  try {
    return std::stoll(*content);
  } catch (...) {
    return std::nullopt;
  }
}

std::string formatWatts(double watts) {
  std::ostringstream output;
  output << static_cast<int>(std::lround(watts));
  return output.str();
}

struct FanMetrics {
  std::string rpm{"--"};
  std::string percent{"--"};
};

std::optional<std::filesystem::path> resolveFanPath(
    const std::filesystem::path& configured_path) {
  std::error_code error;
  if (std::filesystem::exists(configured_path, error)) {
    return configured_path;
  }

  const auto file_name = configured_path.filename();
  if (file_name.empty()) {
    return std::nullopt;
  }

  const auto hwmon_dir = configured_path.parent_path();
  if (hwmon_dir.empty() || !isHwmonDirectoryName(hwmon_dir.filename().string())) {
    return std::nullopt;
  }

  const auto hwmon_root = hwmon_dir.parent_path();
  if (hwmon_root.filename() != "hwmon" || !std::filesystem::exists(hwmon_root, error)) {
    return std::nullopt;
  }

  std::optional<std::filesystem::path> resolved_path;
  for (const auto& entry : std::filesystem::directory_iterator(hwmon_root, error)) {
    if (error || !entry.is_directory(error)) {
      continue;
    }

    if (!isHwmonDirectoryName(entry.path().filename().string())) {
      continue;
    }

    const auto candidate = entry.path() / file_name;
    if (!std::filesystem::exists(candidate, error)) {
      continue;
    }

    if (resolved_path.has_value()) {
      return std::nullopt;
    }

    resolved_path = candidate;
  }

  return resolved_path;
}

FanMetrics readConfiguredCpuFanMetrics(const core::CpuFanConfig& fan_config) {
  if (fan_config.rpm_path.empty()) {
    return {};
  }

  const auto resolved_path = resolveFanPath(fan_config.rpm_path);
  if (!resolved_path.has_value()) {
    return {};
  }

  const auto rpm = readIntegerFile(*resolved_path);
  if (!rpm.has_value() || *rpm < 0) {
    return {};
  }

  FanMetrics metrics;
  metrics.rpm = std::to_string(*rpm);
  if (fan_config.max_rpm > 0) {
    const double percent = std::clamp(
        100.0 * static_cast<double>(*rpm) / static_cast<double>(fan_config.max_rpm),
        0.0,
        100.0);
    metrics.percent = std::to_string(static_cast<int>(std::lround(percent)));
  }

  return metrics;
}

struct EnergyReading {
  std::string source;
  std::uint64_t microjoules{0};
};

std::optional<EnergyReading> readCpuPackageEnergyMicrojoules() {
  const std::array<std::filesystem::path, 2> preferred_paths{
      "/sys/devices/virtual/powercap/intel-rapl/intel-rapl:0/energy_uj",
      "/sys/class/powercap/intel-rapl:0/energy_uj",
  };

  for (const auto& path : preferred_paths) {
    if (const auto energy = readIntegerFile(path); energy.has_value() && *energy >= 0) {
      return EnergyReading{
          .source = path.string(),
          .microjoules = static_cast<std::uint64_t>(*energy),
      };
    }
  }

  const std::filesystem::path powercap_root{"/sys/class/powercap"};
  std::error_code error;
  if (!std::filesystem::exists(powercap_root, error)) {
    return std::nullopt;
  }

  for (const auto& entry : std::filesystem::directory_iterator(powercap_root, error)) {
    if (error || !entry.is_directory(error)) {
      continue;
    }

    const auto zone_name = entry.path().filename().string();
    if (zone_name.rfind("intel-rapl:", 0) != 0 ||
        std::count(zone_name.begin(), zone_name.end(), ':') != 1) {
      continue;
    }

    const auto energy_path = entry.path() / "energy_uj";
    if (const auto energy = readIntegerFile(energy_path); energy.has_value() && *energy >= 0) {
      return EnergyReading{
          .source = energy_path.string(),
          .microjoules = static_cast<std::uint64_t>(*energy),
      };
    }
  }

  return std::nullopt;
}

}  // namespace

CpuProvider::CpuProvider(core::CpuFanConfig fan_config)
    : fan_config_(std::move(fan_config)) {}

std::string CpuProvider::name() const { return "cpu"; }

std::uint64_t CpuProvider::CpuTimes::total() const {
  return user + nice + system + idle + iowait + irq + softirq + steal;
}

std::uint64_t CpuProvider::CpuTimes::active() const {
  return user + nice + system + irq + softirq + steal;
}

std::optional<CpuProvider::CpuTimes> CpuProvider::readCpuTimes() {
  std::ifstream input("/proc/stat");
  std::string label;
  CpuTimes times;

  if (!(input >> label >> times.user >> times.nice >> times.system >> times.idle >>
        times.iowait >> times.irq >> times.softirq >> times.steal)) {
    return std::nullopt;
  }

  if (label != "cpu") {
    return std::nullopt;
  }

  return times;
}

std::optional<std::filesystem::path> CpuProvider::resolvedFanPath() {
  std::error_code error;
  if (fan_path_.has_value() && std::filesystem::exists(*fan_path_, error)) {
    return fan_path_;
  }

  fan_path_ = resolveFanPath(fan_config_.rpm_path);
  return fan_path_;
}

std::optional<std::filesystem::path> CpuProvider::resolvedCpuTemperaturePath() {
  if (cpu_temperature_path_.has_value()) {
    if (const auto temp = readIntegerFile(*cpu_temperature_path_);
        temp.has_value() && *temp > 0 && *temp <= 150000) {
      return cpu_temperature_path_;
    }
    cpu_temperature_path_.reset();
  }

  const std::filesystem::path thermal_root{"/sys/class/thermal"};
  std::error_code error;
  if (!std::filesystem::exists(thermal_root, error)) {
    return std::nullopt;
  }

  int best_score = -1;
  std::optional<std::filesystem::path> best_path;

  for (const auto& entry : std::filesystem::directory_iterator(thermal_root, error)) {
    if (error) {
      break;
    }

    if (!entry.is_directory(error)) {
      continue;
    }

    const auto type_path = entry.path() / "type";
    const auto temp_path = entry.path() / "temp";

    std::ifstream type_file(type_path);
    std::ifstream temp_file(temp_path);
    if (!type_file || !temp_file) {
      continue;
    }

    std::string type;
    long temp_milli_c = 0;
    std::getline(type_file, type);
    temp_file >> temp_milli_c;

    if (temp_milli_c <= 0 || temp_milli_c > 150000) {
      continue;
    }

    int score = 1;
    type = trim(type);
    if (type.find("cpu") != std::string::npos ||
        type.find("pkg") != std::string::npos ||
        type.find("x86") != std::string::npos) {
      score = 3;
    } else if (type.find("soc") != std::string::npos) {
      score = 2;
    }

    if (score > best_score) {
      best_score = score;
      best_path = temp_path;
    }
  }

  if (!best_path.has_value()) {
    return std::nullopt;
  }

  cpu_temperature_path_ = best_path;
  return cpu_temperature_path_;
}

std::string CpuProvider::readCpuTemperature() {
  const auto temp_path = resolvedCpuTemperaturePath();
  if (!temp_path.has_value()) {
    return "--";
  }

  const auto best_temp = readIntegerFile(*temp_path);
  if (!best_temp.has_value() || *best_temp <= 0 || *best_temp > 150000) {
    cpu_temperature_path_.reset();
    return "--";
  }

  return std::to_string(static_cast<int>(std::lround(*best_temp / 1000.0)));
}

std::string CpuProvider::readCpuClock() {
  resolveCpuClockPaths();
  long long khz_max = 0;
  for (const auto& path : cpu_clock_paths_) {
    if (const auto candidate = readIntegerFile(path); candidate.has_value() && *candidate > 0) {
      khz_max = std::max(khz_max, *candidate);
    }
  }

  if (khz_max > 0) {
    return std::to_string(static_cast<int>(std::lround(khz_max / 1000.0))) + "M";
  }

  std::ifstream input("/proc/cpuinfo");
  std::string line;
  double mhz_max = 0.0;

  while (std::getline(input, line)) {
    if (line.rfind("cpu MHz", 0) != 0) {
      continue;
    }

    const auto delimiter = line.find(':');
    if (delimiter == std::string::npos) {
      continue;
    }

    try {
      mhz_max = std::max(mhz_max, std::stod(trim(line.substr(delimiter + 1))));
    } catch (...) {
      continue;
    }
  }

  if (mhz_max > 0.0) {
    return std::to_string(static_cast<int>(std::lround(mhz_max))) + "M";
  }

  return "--";
}

std::optional<std::filesystem::path> CpuProvider::resolvedCpuEnergyPath() {
  if (cpu_energy_path_.has_value()) {
    if (const auto energy = readIntegerFile(*cpu_energy_path_);
        energy.has_value() && *energy >= 0) {
      return cpu_energy_path_;
    }
    cpu_energy_path_.reset();
  }

  if (const auto energy = readCpuPackageEnergyMicrojoules(); energy.has_value()) {
    cpu_energy_path_ = energy->source;
    return cpu_energy_path_;
  }

  return std::nullopt;
}

void CpuProvider::resolveCpuClockPaths() {
  if (cpu_clock_paths_resolved_) {
    return;
  }

  cpu_clock_paths_resolved_ = true;
  const std::filesystem::path cpufreq_root{"/sys/devices/system/cpu/cpufreq"};
  std::error_code error;
  if (!std::filesystem::exists(cpufreq_root, error)) {
    return;
  }

  for (const auto& entry : std::filesystem::directory_iterator(cpufreq_root, error)) {
    if (error || !entry.is_directory(error)) {
      continue;
    }

    const auto cpuinfo_cur = entry.path() / "cpuinfo_cur_freq";
    const auto scaling_cur = entry.path() / "scaling_cur_freq";
    if (std::filesystem::exists(cpuinfo_cur, error)) {
      cpu_clock_paths_.push_back(cpuinfo_cur);
      continue;
    }
    if (std::filesystem::exists(scaling_cur, error)) {
      cpu_clock_paths_.push_back(scaling_cur);
    }
  }
}

void CpuProvider::collect(core::MetricMap& metrics) {
  const auto current_sample = readCpuTimes();
  if (current_sample.has_value() && previous_sample_.has_value()) {
    const auto total_delta =
        current_sample->total() - previous_sample_->total();
    const auto active_delta =
        current_sample->active() - previous_sample_->active();

    if (total_delta > 0) {
      last_load_percent_ =
          100.0 * static_cast<double>(active_delta) /
          static_cast<double>(total_delta);
    }
  }

  previous_sample_ = current_sample;

  std::string cpu_power = "--";
  if (const auto energy_path = resolvedCpuEnergyPath(); energy_path.has_value()) {
    const auto energy_value = readIntegerFile(*energy_path);
    if (energy_value.has_value() && *energy_value >= 0) {
      const EnergyReading energy{
          .source = energy_path->string(),
          .microjoules = static_cast<std::uint64_t>(*energy_value),
      };
      const auto now = std::chrono::steady_clock::now();
      if (previous_energy_sample_.has_value() &&
          previous_energy_sample_->source == energy.source &&
          energy.microjoules >= previous_energy_sample_->microjoules) {
        const auto elapsed =
            std::chrono::duration<double>(now - previous_energy_sample_->timestamp).count();
        if (elapsed > 0.0) {
          const auto delta =
              static_cast<double>(energy.microjoules - previous_energy_sample_->microjoules);
          cpu_power = formatWatts((delta / elapsed) / 1000000.0);
        }
      }
      previous_energy_sample_ = EnergySample{
          .source = energy.source,
          .microjoules = energy.microjoules,
          .timestamp = now,
      };
    } else {
      cpu_energy_path_.reset();
      previous_energy_sample_.reset();
    }
  } else {
    previous_energy_sample_.reset();
  }

  FanMetrics fan_metrics;
  if (const auto fan_path = resolvedFanPath(); fan_path.has_value()) {
    const auto rpm = readIntegerFile(*fan_path);
    if (rpm.has_value() && *rpm >= 0) {
      fan_metrics.rpm = std::to_string(*rpm);
      if (fan_config_.max_rpm > 0) {
        const double percent = std::clamp(
            100.0 * static_cast<double>(*rpm) / static_cast<double>(fan_config_.max_rpm),
            0.0,
            100.0);
        fan_metrics.percent = std::to_string(static_cast<int>(std::lround(percent)));
      }
    } else {
      fan_path_.reset();
    }
  } else if (!fan_config_.rpm_path.empty()) {
    fan_metrics = readConfiguredCpuFanMetrics(fan_config_);
  }

  metrics["cpu.load"] =
      std::to_string(static_cast<int>(std::lround(last_load_percent_)));
  metrics["cpu.temp"] = readCpuTemperature();
  metrics["cpu.clock"] = readCpuClock();
  metrics["cpu.power_w"] = cpu_power;
  metrics["cpu.fan_rpm"] = fan_metrics.rpm;
  metrics["cpu.fan_percent"] = fan_metrics.percent;
}

}  // namespace sure_smartie::providers
