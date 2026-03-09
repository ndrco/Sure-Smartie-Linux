#include "sure_smartie/providers/RamProvider.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>

namespace sure_smartie::providers {
namespace {

std::string formatGiB(double kibibytes) {
  std::ostringstream output;
  output << std::fixed << std::setprecision(1)
         << (kibibytes / (1024.0 * 1024.0)) << "G";
  return output.str();
}

}  // namespace

std::string RamProvider::name() const { return "ram"; }

void RamProvider::collect(core::MetricMap& metrics) {
  std::ifstream input("/proc/meminfo");
  std::string key;
  long value = 0;
  std::string unit;
  std::unordered_map<std::string, long> meminfo;

  while (input >> key >> value >> unit) {
    if (!key.empty() && key.back() == ':') {
      key.pop_back();
    }
    meminfo[key] = value;
  }

  const auto total = meminfo["MemTotal"];
  const auto available = meminfo.contains("MemAvailable")
                             ? meminfo["MemAvailable"]
                             : meminfo["MemFree"];
  const auto used = total - available;
  const auto percent =
      total > 0 ? (100.0 * static_cast<double>(used) / static_cast<double>(total))
                : 0.0;

  metrics["ram.percent"] =
      std::to_string(static_cast<int>(percent + 0.5));
  metrics["ram.used_gb"] = formatGiB(static_cast<double>(used));
  metrics["ram.total_gb"] = formatGiB(static_cast<double>(total));
}

}  // namespace sure_smartie::providers
