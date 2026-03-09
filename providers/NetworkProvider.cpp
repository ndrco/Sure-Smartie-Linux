#include "sure_smartie/providers/NetworkProvider.hpp"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

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

std::pair<std::string, std::string> primaryIpv4() {
  ifaddrs* interfaces = nullptr;
  if (::getifaddrs(&interfaces) != 0) {
    return {"-", "-"};
  }

  std::pair<std::string, std::string> result{"-", "-"};
  for (ifaddrs* current = interfaces; current != nullptr; current = current->ifa_next) {
    if (current->ifa_addr == nullptr || current->ifa_name == nullptr) {
      continue;
    }

    if (current->ifa_addr->sa_family != AF_INET) {
      continue;
    }

    if ((current->ifa_flags & IFF_UP) == 0 ||
        (current->ifa_flags & IFF_LOOPBACK) != 0) {
      continue;
    }

    char address[INET_ADDRSTRLEN]{};
    const auto* socket_address =
        reinterpret_cast<const sockaddr_in*>(current->ifa_addr);
    if (::inet_ntop(AF_INET, &socket_address->sin_addr, address, sizeof(address)) ==
        nullptr) {
      continue;
    }

    result = {current->ifa_name, address};
    break;
  }

  ::freeifaddrs(interfaces);
  return result;
}

std::unordered_map<std::string, std::pair<std::uint64_t, std::uint64_t>> readNetDev() {
  std::ifstream input("/proc/net/dev");
  std::string line;
  std::unordered_map<std::string, std::pair<std::uint64_t, std::uint64_t>> counters;

  std::getline(input, line);
  std::getline(input, line);
  while (std::getline(input, line)) {
    const auto colon = line.find(':');
    if (colon == std::string::npos) {
      continue;
    }

    const auto iface = trim(line.substr(0, colon));
    std::istringstream payload(line.substr(colon + 1));
    std::uint64_t rx_bytes = 0;
    std::uint64_t rx_packets = 0;
    std::uint64_t rx_errs = 0;
    std::uint64_t rx_drop = 0;
    std::uint64_t rx_fifo = 0;
    std::uint64_t rx_frame = 0;
    std::uint64_t rx_compressed = 0;
    std::uint64_t rx_multicast = 0;
    std::uint64_t tx_bytes = 0;

    payload >> rx_bytes >> rx_packets >> rx_errs >> rx_drop >> rx_fifo >>
        rx_frame >> rx_compressed >> rx_multicast >> tx_bytes;
    counters[iface] = {rx_bytes, tx_bytes};
  }

  return counters;
}

std::string formatBytes(std::uint64_t bytes) {
  static constexpr const char* kUnits[] = {"B", "K", "M", "G", "T"};
  double value = static_cast<double>(bytes);
  std::size_t unit_index = 0;

  while (value >= 1024.0 && unit_index < std::size(kUnits) - 1) {
    value /= 1024.0;
    ++unit_index;
  }

  std::ostringstream output;
  if (unit_index == 0) {
    output << static_cast<std::uint64_t>(value) << kUnits[unit_index];
  } else {
    output << std::fixed << std::setprecision(1) << value << kUnits[unit_index];
  }
  return output.str();
}

}  // namespace

std::string NetworkProvider::name() const { return "network"; }

void NetworkProvider::collect(core::MetricMap& metrics) {
  const auto [iface, ip] = primaryIpv4();
  const auto counters = readNetDev();

  metrics["net.iface"] = iface;
  metrics["net.ip"] = ip;

  auto it = counters.find(iface);
  if (it == counters.end()) {
    for (const auto& [candidate_iface, values] : counters) {
      if (candidate_iface == "lo") {
        continue;
      }
      it = counters.find(candidate_iface);
      break;
    }
  }

  if (it != counters.end()) {
    metrics["net.rx_total"] = formatBytes(it->second.first);
    metrics["net.tx_total"] = formatBytes(it->second.second);
  } else {
    metrics["net.rx_total"] = "-";
    metrics["net.tx_total"] = "-";
  }
}

}  // namespace sure_smartie::providers
