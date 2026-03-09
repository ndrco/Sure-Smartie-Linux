#include "sure_smartie/providers/BuiltinProviderFactory.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <unordered_set>

#include "sure_smartie/providers/CpuProvider.hpp"
#include "sure_smartie/providers/GpuProvider.hpp"
#include "sure_smartie/providers/NetworkProvider.hpp"
#include "sure_smartie/providers/RamProvider.hpp"
#include "sure_smartie/providers/SystemProvider.hpp"

namespace sure_smartie::providers {

std::vector<std::string> builtinProviderNames() {
  return {"cpu", "gpu", "ram", "system", "network"};
}

bool isBuiltinProviderName(std::string_view provider_name) {
  const auto names = builtinProviderNames();
  return std::find(names.begin(), names.end(), provider_name) != names.end();
}

std::unique_ptr<IProvider> createBuiltinProvider(std::string_view provider_name) {
  if (provider_name == "cpu") {
    return std::make_unique<CpuProvider>();
  }

  if (provider_name == "ram") {
    return std::make_unique<RamProvider>();
  }

  if (provider_name == "gpu") {
    return std::make_unique<GpuProvider>();
  }

  if (provider_name == "system") {
    return std::make_unique<SystemProvider>();
  }

  if (provider_name == "network") {
    return std::make_unique<NetworkProvider>();
  }

  throw std::invalid_argument("Unknown provider: " + std::string(provider_name));
}

std::vector<std::unique_ptr<IProvider>> createBuiltinProviders(
    const std::vector<std::string>& provider_names) {
  std::vector<std::unique_ptr<IProvider>> providers;
  std::unordered_set<std::string> seen;

  for (const auto& provider_name : provider_names) {
    if (!seen.insert(provider_name).second) {
      continue;
    }
    providers.push_back(createBuiltinProvider(provider_name));
  }

  return providers;
}

}  // namespace sure_smartie::providers
