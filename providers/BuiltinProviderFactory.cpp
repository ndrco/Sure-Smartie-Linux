#include "sure_smartie/providers/BuiltinProviderFactory.hpp"

#include <memory>
#include <stdexcept>
#include <unordered_set>

#include "sure_smartie/providers/CpuProvider.hpp"
#include "sure_smartie/providers/NetworkProvider.hpp"
#include "sure_smartie/providers/RamProvider.hpp"
#include "sure_smartie/providers/SystemProvider.hpp"

namespace sure_smartie::providers {

std::vector<std::unique_ptr<IProvider>> createBuiltinProviders(
    const std::vector<std::string>& provider_names) {
  std::vector<std::unique_ptr<IProvider>> providers;
  std::unordered_set<std::string> seen;

  for (const auto& provider_name : provider_names) {
    if (!seen.insert(provider_name).second) {
      continue;
    }

    if (provider_name == "cpu") {
      providers.push_back(std::make_unique<CpuProvider>());
      continue;
    }

    if (provider_name == "ram") {
      providers.push_back(std::make_unique<RamProvider>());
      continue;
    }

    if (provider_name == "system") {
      providers.push_back(std::make_unique<SystemProvider>());
      continue;
    }

    if (provider_name == "network") {
      providers.push_back(std::make_unique<NetworkProvider>());
      continue;
    }

    throw std::invalid_argument("Unknown provider: " + provider_name);
  }

  return providers;
}

}  // namespace sure_smartie::providers
