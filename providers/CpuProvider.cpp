#include "sure_smartie/providers/CpuProvider.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

namespace sure_smartie::providers {
namespace {

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t");
  if (first == std::string::npos) {
    return {};
  }
  const auto last = value.find_last_not_of(" \t");
  return value.substr(first, last - first + 1);
}

std::string toLower(std::string value) {
  std::transform(value.begin(),
                 value.end(),
                 value.begin(),
                 [](unsigned char symbol) { return static_cast<char>(std::tolower(symbol)); });
  return value;
}

bool containsToken(std::string_view value, std::string_view token) {
  return toLower(std::string(value)).find(toLower(std::string(token))) != std::string::npos;
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
  output << std::fixed << std::setprecision(1) << watts;
  return output.str();
}

std::optional<double> readPwmPercent(const std::filesystem::path& hwmon_path, int index) {
  const auto pwm = readIntegerFile(hwmon_path / ("pwm" + std::to_string(index)));
  if (!pwm.has_value()) {
    return std::nullopt;
  }

  const auto pwm_max = readIntegerFile(hwmon_path / ("pwm" + std::to_string(index) + "_max"));
  const double max_value =
      pwm_max.has_value() && *pwm_max > 0 ? static_cast<double>(*pwm_max) : 255.0;
  return std::clamp(100.0 * static_cast<double>(*pwm) / max_value, 0.0, 100.0);
}

struct FanMetrics {
  std::string rpm{"--"};
  std::string percent{"--"};
};

FanMetrics readCpuFanMetrics() {
  const std::filesystem::path hwmon_root{"/sys/class/hwmon"};
  std::error_code error;
  if (!std::filesystem::exists(hwmon_root, error)) {
    return {};
  }

  int best_score = -100;
  FanMetrics best_metrics;

  for (const auto& hwmon_entry : std::filesystem::directory_iterator(hwmon_root, error)) {
    if (error || !hwmon_entry.is_directory(error)) {
      continue;
    }

    const auto hwmon_name = toLower(readTextFile(hwmon_entry.path() / "name").value_or(""));
    if (containsToken(hwmon_name, "amdgpu") || containsToken(hwmon_name, "nvidia")) {
      continue;
    }

    for (int index = 1; index <= 4; ++index) {
      const auto rpm =
          readIntegerFile(hwmon_entry.path() / ("fan" + std::to_string(index) + "_input"));
      const auto label = toLower(
          readTextFile(hwmon_entry.path() / ("fan" + std::to_string(index) + "_label"))
              .value_or(""));
      const auto percent = readPwmPercent(hwmon_entry.path(), index);

      if (!rpm.has_value() && !percent.has_value()) {
        continue;
      }

      int score = 0;
      if (rpm.has_value() && *rpm > 0) {
        score += 4;
      }
      if (containsToken(label, "cpu") || containsToken(label, "proc") ||
          containsToken(label, "processor")) {
        score += 8;
      }
      if (containsToken(label, "gpu")) {
        score -= 10;
      }
      if (containsToken(hwmon_name, "coretemp") || containsToken(hwmon_name, "k10temp") ||
          containsToken(hwmon_name, "zenpower") || containsToken(hwmon_name, "acpitz")) {
        score += 1;
      }

      if (score <= best_score) {
        continue;
      }

      best_score = score;
      best_metrics.rpm = rpm.has_value() ? std::to_string(*rpm) : "--";
      best_metrics.percent = percent.has_value()
                                 ? std::to_string(static_cast<int>(std::lround(*percent)))
                                 : "--";
    }
  }

  return best_metrics;
}

std::optional<double> readCpuPowerFromPowercapWatts() {
  const std::filesystem::path powercap_root{"/sys/class/powercap"};
  std::error_code error;
  if (!std::filesystem::exists(powercap_root, error)) {
    return std::nullopt;
  }

  int best_score = -1;
  std::optional<double> best_power;

  for (const auto& entry : std::filesystem::recursive_directory_iterator(
           powercap_root,
           std::filesystem::directory_options::skip_permission_denied,
           error)) {
    if (error || !entry.is_directory(error)) {
      continue;
    }

    const auto domain_name = toLower(readTextFile(entry.path() / "name").value_or(""));
    if (domain_name.empty() || containsToken(domain_name, "dram") ||
        containsToken(domain_name, "psys")) {
      continue;
    }

    int score = 1;
    if (containsToken(domain_name, "package") || containsToken(domain_name, "pkg")) {
      score = 4;
    } else if (containsToken(domain_name, "cpu")) {
      score = 3;
    } else if (containsToken(domain_name, "core")) {
      score = 2;
    }

    const auto power_uw = readIntegerFile(entry.path() / "power_uw");
    if (!power_uw.has_value() || *power_uw <= 0) {
      continue;
    }

    if (score > best_score) {
      best_score = score;
      best_power = static_cast<double>(*power_uw) / 1000000.0;
    }
  }

  return best_power;
}

std::optional<std::uint64_t> readCpuEnergyMicrojoules() {
  const std::filesystem::path powercap_root{"/sys/class/powercap"};
  std::error_code error;
  if (!std::filesystem::exists(powercap_root, error)) {
    return std::nullopt;
  }

  int best_score = -1;
  std::optional<std::uint64_t> best_energy;

  for (const auto& entry : std::filesystem::recursive_directory_iterator(
           powercap_root,
           std::filesystem::directory_options::skip_permission_denied,
           error)) {
    if (error || !entry.is_directory(error)) {
      continue;
    }

    const auto domain_name = toLower(readTextFile(entry.path() / "name").value_or(""));
    if (domain_name.empty() || containsToken(domain_name, "dram") ||
        containsToken(domain_name, "psys")) {
      continue;
    }

    int score = 1;
    if (containsToken(domain_name, "package") || containsToken(domain_name, "pkg")) {
      score = 4;
    } else if (containsToken(domain_name, "cpu")) {
      score = 3;
    } else if (containsToken(domain_name, "core")) {
      score = 2;
    }

    const auto energy = readIntegerFile(entry.path() / "energy_uj");
    if (!energy.has_value() || *energy <= 0) {
      continue;
    }

    if (score > best_score) {
      best_score = score;
      best_energy = static_cast<std::uint64_t>(*energy);
    }
  }

  return best_energy;
}

std::optional<double> readCpuPowerFromHwmonWatts() {
  const std::filesystem::path hwmon_root{"/sys/class/hwmon"};
  std::error_code error;
  if (!std::filesystem::exists(hwmon_root, error)) {
    return std::nullopt;
  }

  int best_score = -1;
  std::optional<double> best_power;

  for (const auto& hwmon_entry : std::filesystem::directory_iterator(hwmon_root, error)) {
    if (error || !hwmon_entry.is_directory(error)) {
      continue;
    }

    const auto hwmon_name = toLower(readTextFile(hwmon_entry.path() / "name").value_or(""));
    int score = 0;
    if (containsToken(hwmon_name, "coretemp") || containsToken(hwmon_name, "k10temp") ||
        containsToken(hwmon_name, "zenpower")) {
      score = 4;
    } else if (containsToken(hwmon_name, "cpu")) {
      score = 3;
    }

    for (int index = 1; index <= 4; ++index) {
      const auto power_average =
          readIntegerFile(hwmon_entry.path() / ("power" + std::to_string(index) + "_average"));
      const auto power_input =
          readIntegerFile(hwmon_entry.path() / ("power" + std::to_string(index) + "_input"));
      const auto raw_power = power_average.has_value() ? power_average : power_input;
      if (!raw_power.has_value() || *raw_power <= 0) {
        continue;
      }

      if (score > best_score) {
        best_score = score;
        best_power = static_cast<double>(*raw_power) / 1000000.0;
      }
    }
  }

  return best_power;
}

}  // namespace

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

std::string CpuProvider::readCpuTemperature() {
  const std::filesystem::path thermal_root{"/sys/class/thermal"};
  std::error_code error;
  if (!std::filesystem::exists(thermal_root, error)) {
    return "--";
  }

  int best_score = -1;
  long best_temp = -1;

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
      best_temp = temp_milli_c;
    }
  }

  if (best_temp < 0) {
    return "--";
  }

  return std::to_string(static_cast<int>(std::lround(best_temp / 1000.0)));
}

std::string CpuProvider::readCpuClock() {
  std::ifstream input("/proc/cpuinfo");
  std::string line;
  double mhz_sum = 0.0;
  std::size_t mhz_count = 0;

  while (std::getline(input, line)) {
    if (line.rfind("cpu MHz", 0) != 0) {
      continue;
    }

    const auto delimiter = line.find(':');
    if (delimiter == std::string::npos) {
      continue;
    }

    try {
      mhz_sum += std::stod(trim(line.substr(delimiter + 1)));
      ++mhz_count;
    } catch (...) {
      continue;
    }
  }

  if (mhz_count > 0) {
    return std::to_string(static_cast<int>(std::lround(mhz_sum / mhz_count))) + "M";
  }

  return "--";
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
  if (const auto direct_power = readCpuPowerFromPowercapWatts(); direct_power.has_value()) {
    cpu_power = formatWatts(*direct_power);
    previous_energy_sample_.reset();
  } else if (const auto hwmon_power = readCpuPowerFromHwmonWatts(); hwmon_power.has_value()) {
    cpu_power = formatWatts(*hwmon_power);
    previous_energy_sample_.reset();
  } else if (const auto energy = readCpuEnergyMicrojoules(); energy.has_value()) {
    const auto now = std::chrono::steady_clock::now();
    if (previous_energy_sample_.has_value() && *energy >= previous_energy_sample_->first) {
      const auto elapsed =
          std::chrono::duration<double>(now - previous_energy_sample_->second).count();
      if (elapsed > 0.1) {
        const auto delta = static_cast<double>(*energy - previous_energy_sample_->first);
        cpu_power = formatWatts((delta / elapsed) / 1000000.0);
      }
    }
    previous_energy_sample_ = std::make_pair(*energy, now);
  } else {
    previous_energy_sample_.reset();
  }

  const auto fan_metrics = readCpuFanMetrics();

  metrics["cpu.load"] =
      std::to_string(static_cast<int>(std::lround(last_load_percent_)));
  metrics["cpu.temp"] = readCpuTemperature();
  metrics["cpu.clock"] = readCpuClock();
  metrics["cpu.power_w"] = cpu_power;
  metrics["cpu.fan_rpm"] = fan_metrics.rpm;
  metrics["cpu.fan_percent"] = fan_metrics.percent;
}

}  // namespace sure_smartie::providers
