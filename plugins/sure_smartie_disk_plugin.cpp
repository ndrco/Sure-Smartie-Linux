#include "sure_smartie/plugins/ProviderPlugin.hpp"

#include <sys/statvfs.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct MountEntry {
  std::string device;
  std::string mount_point;
  std::string fs_type;
};

struct DiskUsage {
  MountEntry mount;
  std::uint64_t total_bytes{0};
  std::uint64_t used_bytes{0};
};

std::string decodeMountField(std::string_view value) {
  std::string decoded;
  decoded.reserve(value.size());

  for (std::size_t index = 0; index < value.size(); ++index) {
    if (value[index] == '\\' && index + 3 < value.size()) {
      const char a = value[index + 1];
      const char b = value[index + 2];
      const char c = value[index + 3];
      if (a >= '0' && a <= '7' && b >= '0' && b <= '7' && c >= '0' && c <= '7') {
        const int decoded_value = ((a - '0') << 6) | ((b - '0') << 3) | (c - '0');
        decoded.push_back(static_cast<char>(decoded_value));
        index += 3;
        continue;
      }
    }

    decoded.push_back(value[index]);
  }

  return decoded;
}

bool startsWith(std::string_view value, std::string_view prefix) {
  return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

bool isPseudoFilesystem(std::string_view fs_type) {
  static constexpr std::array<std::string_view, 17> kIgnoredFsTypes{
      "proc",       "sysfs",     "devtmpfs", "devpts",    "tmpfs",   "cgroup",
      "cgroup2",    "overlay",   "squashfs", "tracefs",   "ramfs",   "fusectl",
      "securityfs", "pstore",    "debugfs",  "configfs",  "mqueue",
  };

  return std::find(kIgnoredFsTypes.begin(), kIgnoredFsTypes.end(), fs_type) !=
         kIgnoredFsTypes.end();
}

std::vector<MountEntry> readMountedDisks() {
  std::ifstream mounts("/proc/self/mounts");
  if (!mounts) {
    return {};
  }

  std::vector<MountEntry> entries;
  std::set<std::string> seen_mount_points;
  std::string line;

  while (std::getline(mounts, line)) {
    std::istringstream row(line);
    std::string raw_device;
    std::string raw_mount_point;
    std::string fs_type;
    std::string options;
    int dump_frequency = 0;
    int pass_number = 0;

    if (!(row >> raw_device >> raw_mount_point >> fs_type >> options >> dump_frequency >>
          pass_number)) {
      continue;
    }

    const std::string device = decodeMountField(raw_device);
    const std::string mount_point = decodeMountField(raw_mount_point);

    if (!startsWith(device, "/dev/")) {
      continue;
    }
    if (startsWith(device, "/dev/loop")) {
      continue;
    }
    if (isPseudoFilesystem(fs_type)) {
      continue;
    }
    if (!seen_mount_points.insert(mount_point).second) {
      continue;
    }

    entries.push_back(MountEntry{
        .device = device,
        .mount_point = mount_point,
        .fs_type = fs_type,
    });
  }

  std::sort(entries.begin(), entries.end(), [](const MountEntry& left, const MountEntry& right) {
    return left.mount_point < right.mount_point;
  });
  return entries;
}

std::vector<DiskUsage> collectDiskUsage() {
  std::vector<DiskUsage> disks;

  for (const auto& mount : readMountedDisks()) {
    struct statvfs stat {
    };
    if (::statvfs(mount.mount_point.c_str(), &stat) != 0) {
      continue;
    }

    const std::uint64_t block_size =
        stat.f_frsize > 0 ? static_cast<std::uint64_t>(stat.f_frsize)
                          : static_cast<std::uint64_t>(stat.f_bsize);
    if (block_size == 0) {
      continue;
    }

    const std::uint64_t total = static_cast<std::uint64_t>(stat.f_blocks) * block_size;
    const std::uint64_t free = static_cast<std::uint64_t>(stat.f_bfree) * block_size;
    const std::uint64_t used = total > free ? total - free : 0;

    if (total == 0) {
      continue;
    }

    disks.push_back(DiskUsage{
        .mount = mount,
        .total_bytes = total,
        .used_bytes = used,
    });
  }

  return disks;
}

std::string formatGigabytes(std::uint64_t bytes) {
  constexpr double kBytesInGigabyte = 1024.0 * 1024.0 * 1024.0;
  const double gigabytes = static_cast<double>(bytes) / kBytesInGigabyte;

  std::ostringstream output;
  output << std::fixed << std::setprecision(1) << gigabytes << 'G';
  return output.str();
}

std::string formatPercent(std::uint64_t used_bytes, std::uint64_t total_bytes) {
  if (total_bytes == 0) {
    return "0";
  }

  const double percent =
      (static_cast<double>(used_bytes) * 100.0) / static_cast<double>(total_bytes);
  const int rounded = static_cast<int>(percent + 0.5);
  return std::to_string(std::clamp(rounded, 0, 100));
}

class DiskProvider final : public sure_smartie::providers::IProvider {
 public:
  std::string name() const override { return "disk"; }

  void collect(sure_smartie::core::MetricMap& metrics) override {
    const auto disks = collectDiskUsage();

    std::uint64_t total_used = 0;
    std::uint64_t total_capacity = 0;

    for (std::size_t index = 0; index < disks.size(); ++index) {
      const auto& disk = disks[index];
      const std::string prefix = "disk." + std::to_string(index + 1) + ".";

      metrics[prefix + "device"] = disk.mount.device;
      metrics[prefix + "mount"] = disk.mount.mount_point;
      metrics[prefix + "fs"] = disk.mount.fs_type;
      metrics[prefix + "total_gb"] = formatGigabytes(disk.total_bytes);
      metrics[prefix + "used_gb"] = formatGigabytes(disk.used_bytes);
      metrics[prefix + "used_percent"] = formatPercent(disk.used_bytes, disk.total_bytes);

      total_used += disk.used_bytes;
      total_capacity += disk.total_bytes;
    }

    metrics["disk.count"] = std::to_string(disks.size());
    metrics["disk.total_gb"] = formatGigabytes(total_capacity);
    metrics["disk.used_gb"] = formatGigabytes(total_used);
    metrics["disk.used_percent"] = formatPercent(total_used, total_capacity);
  }
};

sure_smartie::providers::IProvider* createProvider() { return new DiskProvider(); }

void destroyProvider(sure_smartie::providers::IProvider* provider) { delete provider; }

}  // namespace

extern "C" const sure_smartie::plugins::ProviderPluginDescriptor*
sure_smartie_provider_plugin() {
  static const sure_smartie::plugins::ProviderPluginDescriptor descriptor{
      .api_version = sure_smartie::plugins::kProviderPluginApiVersion,
      .name = "sure_smartie_disk_plugin",
      .create = &createProvider,
      .destroy = &destroyProvider,
  };

  return &descriptor;
}
