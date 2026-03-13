#include "sure_smartie/providers/GpuProvider.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace sure_smartie::providers {
namespace {

struct GpuMetrics {
  std::string name{"--"};
  std::string vendor{"--"};
  std::string load{"--"};
  std::string clock{"--"};
  std::string temp{"--"};
  std::string mem_used{"--"};
  std::string mem_total{"--"};
  std::string mem_percent{"--"};
  std::string power_w{"--"};
  std::string fan_rpm{"--"};
  std::string fan_percent{"--"};
};

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t");
  if (first == std::string::npos) {
    return {};
  }
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::vector<std::string> splitCsvLine(const std::string& line) {
  std::vector<std::string> fields;
  std::string current;

  for (char symbol : line) {
    if (symbol == ',') {
      fields.push_back(trim(current));
      current.clear();
      continue;
    }
    current.push_back(symbol);
  }

  fields.push_back(trim(current));
  return fields;
}

std::string toLower(std::string value) {
  std::transform(value.begin(),
                 value.end(),
                 value.begin(),
                 [](unsigned char symbol) { return static_cast<char>(std::tolower(symbol)); });
  return value;
}

std::optional<double> parseNumber(const std::string& value) {
  const auto trimmed = trim(value);
  if (trimmed.empty() || trimmed == "[N/A]" || trimmed == "N/A" || trimmed == "--") {
    return std::nullopt;
  }

  try {
    return std::stod(trimmed);
  } catch (...) {
    return std::nullopt;
  }
}

std::string formatWatts(double watts) {
  std::ostringstream output;
  output << static_cast<int>(std::lround(watts));
  return output.str();
}

std::string formatGiB(double bytes) {
  std::ostringstream output;
  output << std::fixed << std::setprecision(1)
         << (bytes / (1024.0 * 1024.0 * 1024.0)) << "G";
  return output.str();
}

std::string formatMiB(double mib) {
  std::ostringstream output;
  output << std::fixed << std::setprecision(1) << (mib / 1024.0) << "G";
  return output.str();
}

bool isCardDirectory(const std::filesystem::path& path) {
  const auto name = path.filename().string();
  if (name.size() < 5 || name.rfind("card", 0) != 0) {
    return false;
  }

  for (std::size_t index = 4; index < name.size(); ++index) {
    if (!std::isdigit(static_cast<unsigned char>(name[index]))) {
      return false;
    }
  }

  return true;
}

std::optional<std::string> readFile(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) {
    return std::nullopt;
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();
  return trim(buffer.str());
}

std::optional<long long> readIntegerFile(const std::filesystem::path& path) {
  const auto content = readFile(path);
  if (!content.has_value()) {
    return std::nullopt;
  }

  try {
    return std::stoll(*content);
  } catch (...) {
    return std::nullopt;
  }
}

std::string formatMhz(long long mhz) {
  return std::to_string(static_cast<int>(mhz)) + "M";
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

std::optional<double> readHwmonPowerWatts(const std::filesystem::path& device_path) {
  std::error_code error;
  const auto hwmon_root = device_path / "hwmon";
  if (!std::filesystem::exists(hwmon_root, error)) {
    return std::nullopt;
  }

  for (const auto& entry : std::filesystem::directory_iterator(hwmon_root, error)) {
    if (error || !entry.is_directory(error)) {
      continue;
    }

    for (int index = 1; index <= 4; ++index) {
      const auto power_average =
          readIntegerFile(entry.path() / ("power" + std::to_string(index) + "_average"));
      const auto power_input =
          readIntegerFile(entry.path() / ("power" + std::to_string(index) + "_input"));
      const auto raw_power = power_average.has_value() ? power_average : power_input;
      if (raw_power.has_value() && *raw_power > 0) {
        return static_cast<double>(*raw_power) / 1000000.0;
      }
    }
  }

  return std::nullopt;
}

std::optional<std::string> readGpuClock(const std::filesystem::path& device_path) {
  if (const auto intel_mhz = readIntegerFile(device_path / "gt_cur_freq_mhz");
      intel_mhz.has_value() && *intel_mhz > 0) {
    return formatMhz(*intel_mhz);
  }
  if (const auto intel_act_mhz = readIntegerFile(device_path / "gt_act_freq_mhz");
      intel_act_mhz.has_value() && *intel_act_mhz > 0) {
    return formatMhz(*intel_act_mhz);
  }

  if (const auto dpm = readFile(device_path / "pp_dpm_sclk"); dpm.has_value()) {
    std::istringstream input(*dpm);
    std::string line;
    while (std::getline(input, line)) {
      if (line.find('*') == std::string::npos) {
        continue;
      }

      const auto colon = line.find(':');
      const auto mhz = toLower(line).find("mhz");
      if (colon == std::string::npos || mhz == std::string::npos || mhz <= colon) {
        continue;
      }

      try {
        return formatMhz(static_cast<long long>(
            std::lround(std::stod(trim(line.substr(colon + 1, mhz - colon - 1))))));
      } catch (...) {
        continue;
      }
    }
  }

  if (const auto freq_khz = readIntegerFile(device_path / "pp_dpm_sclk_min");
      freq_khz.has_value() && *freq_khz > 0) {
    return formatMhz(static_cast<long long>(std::lround(*freq_khz / 1000.0)));
  }

  return std::nullopt;
}

std::optional<std::string> readHwmonFanRpm(const std::filesystem::path& device_path) {
  std::error_code error;
  const auto hwmon_root = device_path / "hwmon";
  if (!std::filesystem::exists(hwmon_root, error)) {
    return std::nullopt;
  }

  for (const auto& entry : std::filesystem::directory_iterator(hwmon_root, error)) {
    if (error || !entry.is_directory(error)) {
      continue;
    }

    for (int index = 1; index <= 4; ++index) {
      const auto rpm =
          readIntegerFile(entry.path() / ("fan" + std::to_string(index) + "_input"));
      if (rpm.has_value() && *rpm > 0) {
        return std::to_string(*rpm);
      }
    }
  }

  return std::nullopt;
}

std::optional<std::string> readHwmonFanPercent(const std::filesystem::path& device_path) {
  std::error_code error;
  const auto hwmon_root = device_path / "hwmon";
  if (!std::filesystem::exists(hwmon_root, error)) {
    return std::nullopt;
  }

  for (const auto& entry : std::filesystem::directory_iterator(hwmon_root, error)) {
    if (error || !entry.is_directory(error)) {
      continue;
    }

    for (int index = 1; index <= 4; ++index) {
      const auto percent = readPwmPercent(entry.path(), index);
      if (percent.has_value()) {
        return std::to_string(static_cast<int>(std::lround(*percent)));
      }
    }
  }

  return std::nullopt;
}

std::optional<GpuMetrics> queryNvidiaSmi() {
  struct PipeCloser {
    void operator()(FILE* pipe) const {
      if (pipe != nullptr) {
        ::pclose(pipe);
      }
    }
  };

  constexpr const char* kCommand =
      "nvidia-smi --query-gpu=utilization.gpu,temperature.gpu,memory.used,"
      "memory.total,name,power.draw,fan.speed,clocks.current.graphics "
      "--format=csv,noheader,nounits 2>/dev/null";
  std::unique_ptr<FILE, PipeCloser> pipe(::popen(kCommand, "r"));
  if (!pipe) {
    return std::nullopt;
  }

  char buffer[512]{};
  if (::fgets(buffer, sizeof(buffer), pipe.get()) == nullptr) {
    return std::nullopt;
  }

  const auto fields = splitCsvLine(buffer);
  if (fields.size() < 8) {
    return std::nullopt;
  }

  GpuMetrics metrics;
  metrics.vendor = "nvidia";
  metrics.name = fields[4];

  if (const auto load = parseNumber(fields[0]); load.has_value()) {
    metrics.load = std::to_string(static_cast<int>(std::lround(*load)));
  }
  if (const auto temp = parseNumber(fields[1]); temp.has_value()) {
    metrics.temp = std::to_string(static_cast<int>(std::lround(*temp)));
  }
  if (const auto clock = parseNumber(fields[7]); clock.has_value()) {
    metrics.clock = formatMhz(static_cast<long long>(std::lround(*clock)));
  }

  const auto mem_used = parseNumber(fields[2]);
  const auto mem_total = parseNumber(fields[3]);
  if (mem_used.has_value()) {
    metrics.mem_used = formatMiB(*mem_used);
  }
  if (mem_total.has_value()) {
    metrics.mem_total = formatMiB(*mem_total);
  }
  if (mem_used.has_value() && mem_total.has_value() && *mem_total > 0.0) {
    metrics.mem_percent =
        std::to_string(static_cast<int>(std::lround(100.0 * *mem_used / *mem_total)));
  }
  if (const auto power = parseNumber(fields[5]); power.has_value()) {
    metrics.power_w = formatWatts(*power);
  }
  if (const auto fan = parseNumber(fields[6]); fan.has_value()) {
    metrics.fan_percent = std::to_string(static_cast<int>(std::lround(*fan)));
  }

  return metrics;
}

std::string vendorName(std::string_view vendor_id) {
  if (vendor_id == "0x10de") {
    return "nvidia";
  }
  if (vendor_id == "0x1002") {
    return "amd";
  }
  if (vendor_id == "0x8086") {
    return "intel";
  }
  return "gpu";
}

std::optional<std::string> readHwmonTemperature(const std::filesystem::path& device_path) {
  std::error_code error;
  const auto hwmon_root = device_path / "hwmon";
  if (!std::filesystem::exists(hwmon_root, error)) {
    return std::nullopt;
  }

  for (const auto& entry : std::filesystem::directory_iterator(hwmon_root, error)) {
    if (error) {
      break;
    }

    if (!entry.is_directory(error)) {
      continue;
    }

    const auto value = readIntegerFile(entry.path() / "temp1_input");
    if (!value.has_value() || *value <= 0) {
      continue;
    }

    return std::to_string(static_cast<int>(std::lround(*value / 1000.0)));
  }

  return std::nullopt;
}

void mergeGpuMetrics(GpuMetrics& base, const GpuMetrics& fallback) {
  if (base.name == "--") {
    base.name = fallback.name;
  }
  if (base.vendor == "--") {
    base.vendor = fallback.vendor;
  }
  if (base.load == "--") {
    base.load = fallback.load;
  }
  if (base.clock == "--") {
    base.clock = fallback.clock;
  }
  if (base.temp == "--") {
    base.temp = fallback.temp;
  }
  if (base.mem_used == "--") {
    base.mem_used = fallback.mem_used;
  }
  if (base.mem_total == "--") {
    base.mem_total = fallback.mem_total;
  }
  if (base.mem_percent == "--") {
    base.mem_percent = fallback.mem_percent;
  }
  if (base.power_w == "--") {
    base.power_w = fallback.power_w;
  }
  if (base.fan_rpm == "--") {
    base.fan_rpm = fallback.fan_rpm;
  }
  if (base.fan_percent == "--") {
    base.fan_percent = fallback.fan_percent;
  }
}

std::optional<GpuMetrics> queryDrmSysfs() {
  const std::filesystem::path drm_root{"/sys/class/drm"};
  std::error_code error;
  if (!std::filesystem::exists(drm_root, error)) {
    return std::nullopt;
  }

  for (const auto& entry : std::filesystem::directory_iterator(drm_root, error)) {
    if (error || !isCardDirectory(entry.path())) {
      continue;
    }

    const auto device_path = entry.path() / "device";
    if (!std::filesystem::exists(device_path, error)) {
      continue;
    }

    const auto vendor_id = readFile(device_path / "vendor");
    if (!vendor_id.has_value()) {
      continue;
    }

    GpuMetrics metrics;
    metrics.vendor = vendorName(*vendor_id);
    metrics.name = metrics.vendor + " " + entry.path().filename().string();

    if (const auto load = readIntegerFile(device_path / "gpu_busy_percent");
        load.has_value()) {
      metrics.load = std::to_string(static_cast<int>(*load));
    } else if (const auto load = readIntegerFile(device_path / "busy_percent");
               load.has_value()) {
      metrics.load = std::to_string(static_cast<int>(*load));
    }
    if (const auto clock = readGpuClock(device_path); clock.has_value()) {
      metrics.clock = *clock;
    }

    if (const auto temp = readHwmonTemperature(device_path); temp.has_value()) {
      metrics.temp = *temp;
    }
    if (const auto power = readHwmonPowerWatts(device_path); power.has_value()) {
      metrics.power_w = formatWatts(*power);
    }
    if (const auto fan_rpm = readHwmonFanRpm(device_path); fan_rpm.has_value()) {
      metrics.fan_rpm = *fan_rpm;
    }
    if (const auto fan_percent = readHwmonFanPercent(device_path); fan_percent.has_value()) {
      metrics.fan_percent = *fan_percent;
    }

    const auto mem_used = readIntegerFile(device_path / "mem_info_vram_used");
    const auto mem_total = readIntegerFile(device_path / "mem_info_vram_total");
    if (mem_used.has_value()) {
      metrics.mem_used = formatGiB(static_cast<double>(*mem_used));
    }
    if (mem_total.has_value()) {
      metrics.mem_total = formatGiB(static_cast<double>(*mem_total));
    }
    if (mem_used.has_value() && mem_total.has_value() && *mem_total > 0) {
      metrics.mem_percent = std::to_string(
          static_cast<int>(std::lround(100.0 * static_cast<double>(*mem_used) /
                                       static_cast<double>(*mem_total))));
    }

    return metrics;
  }

  return std::nullopt;
}

GpuMetrics readGpuMetrics() {
  if (const auto nvidia = queryNvidiaSmi(); nvidia.has_value()) {
    auto result = *nvidia;
    if (const auto sysfs = queryDrmSysfs(); sysfs.has_value()) {
      mergeGpuMetrics(result, *sysfs);
    }
    return result;
  }

  if (const auto sysfs = queryDrmSysfs(); sysfs.has_value()) {
    return *sysfs;
  }

  return {};
}

}  // namespace

std::string GpuProvider::name() const { return "gpu"; }

void GpuProvider::collect(core::MetricMap& metrics) {
  const auto gpu = readGpuMetrics();

  metrics["gpu.name"] = gpu.name;
  metrics["gpu.vendor"] = gpu.vendor;
  metrics["gpu.load"] = gpu.load;
  metrics["gpu.clock"] = gpu.clock;
  metrics["gpu.temp"] = gpu.temp;
  metrics["gpu.mem_used"] = gpu.mem_used;
  metrics["gpu.mem_total"] = gpu.mem_total;
  metrics["gpu.mem_percent"] = gpu.mem_percent;
  metrics["gpu.power_w"] = gpu.power_w;
  metrics["gpu.fan_rpm"] = gpu.fan_rpm;
  metrics["gpu.fan_percent"] = gpu.fan_percent;
}

}  // namespace sure_smartie::providers
