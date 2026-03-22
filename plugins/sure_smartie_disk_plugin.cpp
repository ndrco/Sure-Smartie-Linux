#include "sure_smartie/plugins/ProviderPlugin.hpp"

#include <sys/statvfs.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

constexpr std::string_view kMissingValue = "-";

struct MountEntry {
  std::string device;
  std::string mount_point;
  std::string fs_type;
};

struct DiskUsage {
  MountEntry mount;
  bool mounted{false};
  std::uint64_t total_bytes{0};
  std::uint64_t used_bytes{0};
};

std::vector<MountEntry> defaultTrackedDisks() {
  return {
      MountEntry{
          .device = std::string(kMissingValue),
          .mount_point = "/",
          .fs_type = std::string(kMissingValue),
      },
  };
}

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

bool isTrackedMountPoint(std::string_view mount_point) {
  return mount_point == "/" || mount_point == "/mnt" || startsWith(mount_point, "/mnt/");
}

std::string normalizeOrMissing(std::string value) {
  if (value.empty()) {
    return std::string(kMissingValue);
  }

  return value;
}

std::string shortPathLabel(std::string_view value) {
  if (value.empty() || value == kMissingValue) {
    return std::string(kMissingValue);
  }
  if (value == "/") {
    return "/";
  }

  while (value.size() > 1 && value.back() == '/') {
    value.remove_suffix(1);
  }

  const auto last_slash = value.find_last_of('/');
  if (last_slash == std::string_view::npos || last_slash + 1 >= value.size()) {
    return std::string(value);
  }

  return std::string(value.substr(last_slash + 1));
}

std::vector<MountEntry> readTrackedDisks() {
  std::vector<MountEntry> tracked = defaultTrackedDisks();

  std::set<std::string> seen_mount_points{"/"};
  std::ifstream fstab("/etc/fstab");
  if (!fstab) {
    return tracked;
  }

  std::string line;

  while (std::getline(fstab, line)) {
    const auto first_non_space = line.find_first_not_of(" \t");
    if (first_non_space == std::string::npos || line[first_non_space] == '#') {
      continue;
    }

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

    if (!isTrackedMountPoint(mount_point)) {
      continue;
    }

    if (mount_point == "/") {
      tracked.front().device = normalizeOrMissing(device);
      tracked.front().fs_type = normalizeOrMissing(fs_type);
      continue;
    }

    if (!seen_mount_points.insert(mount_point).second) {
      continue;
    }

    tracked.push_back(MountEntry{
        .device = normalizeOrMissing(device),
        .mount_point = mount_point,
        .fs_type = normalizeOrMissing(fs_type),
    });
  }

  return tracked;
}

std::unordered_map<std::string, MountEntry> readMountedDisks() {
  std::ifstream mounts("/proc/self/mounts");
  if (!mounts) {
    return {};
  }

  std::unordered_map<std::string, MountEntry> entries;
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

    if (!seen_mount_points.insert(mount_point).second) {
      continue;
    }

    entries.emplace(mount_point, MountEntry{
                                    .device = normalizeOrMissing(device),
                                    .mount_point = mount_point,
                                    .fs_type = normalizeOrMissing(fs_type),
                                });
  }

  return entries;
}

std::vector<DiskUsage> collectDiskUsage(
    const std::vector<MountEntry>& tracked_disks) {
  const auto mounted_disks = readMountedDisks();
  std::vector<DiskUsage> disks;
  disks.reserve(tracked_disks.size());

  for (const auto& tracked : tracked_disks) {
    DiskUsage disk{.mount = tracked};
    const auto mounted = mounted_disks.find(tracked.mount_point);
    if (mounted == mounted_disks.end()) {
      disks.push_back(std::move(disk));
      continue;
    }

    disk.mount = mounted->second;
    struct statvfs stat {
    };
    if (::statvfs(disk.mount.mount_point.c_str(), &stat) != 0) {
      disks.push_back(std::move(disk));
      continue;
    }

    const std::uint64_t block_size =
        stat.f_frsize > 0 ? static_cast<std::uint64_t>(stat.f_frsize)
                          : static_cast<std::uint64_t>(stat.f_bsize);
    if (block_size == 0) {
      disks.push_back(std::move(disk));
      continue;
    }

    const std::uint64_t total = static_cast<std::uint64_t>(stat.f_blocks) * block_size;
    const std::uint64_t free = static_cast<std::uint64_t>(stat.f_bfree) * block_size;
    const std::uint64_t used = total > free ? total - free : 0;

    if (total == 0) {
      disks.push_back(std::move(disk));
      continue;
    }

    disk.mounted = true;
    disk.total_bytes = total;
    disk.used_bytes = used;
    disks.push_back(std::move(disk));
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
    const auto& tracked_disks = trackedDisks();
    const auto& disks = diskUsage(tracked_disks);

    std::uint64_t total_used = 0;
    std::uint64_t total_capacity = 0;
    bool has_missing_sizes = false;

    for (std::size_t index = 0; index < disks.size(); ++index) {
      const auto& disk = disks[index];
      const std::string prefix = "disk." + std::to_string(index + 1) + ".";

      metrics[prefix + "device"] = disk.mount.device;
      metrics[prefix + "device_short"] = shortPathLabel(disk.mount.device);
      metrics[prefix + "mount"] = disk.mount.mount_point;
      metrics[prefix + "mount_short"] = shortPathLabel(disk.mount.mount_point);
      metrics[prefix + "fs"] = disk.mount.fs_type;
      metrics[prefix + "mounted"] = disk.mounted ? "1" : "0";

      if (disk.mounted) {
        metrics[prefix + "total_gb"] = formatGigabytes(disk.total_bytes);
        metrics[prefix + "used_gb"] = formatGigabytes(disk.used_bytes);
        metrics[prefix + "used_percent"] = formatPercent(disk.used_bytes, disk.total_bytes);

        total_used += disk.used_bytes;
        total_capacity += disk.total_bytes;
      } else {
        metrics[prefix + "total_gb"] = std::string(kMissingValue);
        metrics[prefix + "used_gb"] = std::string(kMissingValue);
        metrics[prefix + "used_percent"] = std::string(kMissingValue);
        has_missing_sizes = true;
      }
    }

    metrics["disk.count"] = std::to_string(disks.size());
    if (disks.empty() || has_missing_sizes) {
      metrics["disk.total_gb"] = std::string(kMissingValue);
      metrics["disk.used_gb"] = std::string(kMissingValue);
      metrics["disk.used_percent"] = std::string(kMissingValue);
    } else {
      metrics["disk.total_gb"] = formatGigabytes(total_capacity);
      metrics["disk.used_gb"] = formatGigabytes(total_used);
      metrics["disk.used_percent"] = formatPercent(total_used, total_capacity);
    }
  }

 private:
  using Clock = std::chrono::steady_clock;
  static constexpr auto kUsageCacheTtl = std::chrono::seconds(5);

  const std::vector<MountEntry>& trackedDisks() {
    std::error_code error;
    const auto current_mtime =
        std::filesystem::last_write_time("/etc/fstab", error);
    if (!error) {
      if (!tracked_cache_valid_ || current_mtime != cached_fstab_mtime_) {
        tracked_disks_cache_ = readTrackedDisks();
        cached_fstab_mtime_ = current_mtime;
        tracked_cache_valid_ = true;
        usage_cache_valid_ = false;
      }
      return tracked_disks_cache_;
    }

    if (!tracked_cache_valid_) {
      tracked_disks_cache_ = defaultTrackedDisks();
      tracked_cache_valid_ = true;
      usage_cache_valid_ = false;
    }

    return tracked_disks_cache_;
  }

  const std::vector<DiskUsage>& diskUsage(const std::vector<MountEntry>& tracked_disks) {
    const auto now = Clock::now();
    if (usage_cache_valid_ &&
        (now - usage_cache_updated_at_) < kUsageCacheTtl) {
      return usage_cache_;
    }

    usage_cache_ = collectDiskUsage(tracked_disks);
    usage_cache_updated_at_ = now;
    usage_cache_valid_ = true;
    return usage_cache_;
  }

  bool tracked_cache_valid_{false};
  bool usage_cache_valid_{false};
  std::filesystem::file_time_type cached_fstab_mtime_{};
  std::vector<MountEntry> tracked_disks_cache_{};
  std::vector<DiskUsage> usage_cache_{};
  Clock::time_point usage_cache_updated_at_{};
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
