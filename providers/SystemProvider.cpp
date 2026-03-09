#include "sure_smartie/providers/SystemProvider.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

#include <unistd.h>

namespace sure_smartie::providers {
namespace {

std::string formatUptime(double seconds) {
  const auto total_minutes = static_cast<long>(seconds / 60.0);
  const auto days = total_minutes / (24 * 60);
  const auto hours = (total_minutes / 60) % 24;
  const auto minutes = total_minutes % 60;

  std::ostringstream output;
  if (days > 0) {
    output << days << "d " << std::setw(2) << std::setfill('0') << hours << "h";
  } else if (hours > 0) {
    output << std::setw(2) << std::setfill('0') << hours << "h " << std::setw(2)
           << minutes << "m";
  } else {
    output << minutes << "m";
  }
  return output.str();
}

}  // namespace

std::string SystemProvider::name() const { return "system"; }

void SystemProvider::collect(core::MetricMap& metrics) {
  char hostname[256]{};
  if (::gethostname(hostname, sizeof(hostname) - 1) != 0) {
    hostname[0] = '-';
    hostname[1] = '\0';
  }

  const auto now = std::chrono::system_clock::now();
  const std::time_t time_now = std::chrono::system_clock::to_time_t(now);
  const auto local_time = *std::localtime(&time_now);

  std::ostringstream time_stream;
  time_stream << std::put_time(&local_time, "%H:%M");

  std::ifstream uptime_file("/proc/uptime");
  double uptime_seconds = 0.0;
  uptime_file >> uptime_seconds;

  metrics["system.hostname"] = hostname;
  metrics["system.time"] = time_stream.str();
  metrics["system.uptime"] = formatUptime(uptime_seconds);
}

}  // namespace sure_smartie::providers
