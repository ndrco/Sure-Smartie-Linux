#include "sure_smartie/providers/CpuProvider.hpp"

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

  while (std::getline(input, line)) {
    if (line.rfind("cpu MHz", 0) != 0) {
      continue;
    }

    const auto delimiter = line.find(':');
    if (delimiter == std::string::npos) {
      continue;
    }

    const auto mhz = std::stod(trim(line.substr(delimiter + 1)));
    return std::to_string(static_cast<int>(std::lround(mhz))) + "M";
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

  metrics["cpu.load"] =
      std::to_string(static_cast<int>(std::lround(last_load_percent_)));
  metrics["cpu.temp"] = readCpuTemperature();
  metrics["cpu.clock"] = readCpuClock();
}

}  // namespace sure_smartie::providers
