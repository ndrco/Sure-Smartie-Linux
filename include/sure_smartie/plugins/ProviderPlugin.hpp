#pragma once

#include <cstdint>

#include "sure_smartie/providers/IProvider.hpp"

namespace sure_smartie::plugins {

inline constexpr std::uint32_t kProviderPluginApiVersion = 1;
inline constexpr const char* kProviderPluginEntryPoint =
    "sure_smartie_provider_plugin";

struct ProviderPluginDescriptor {
  std::uint32_t api_version;
  const char* name;
  sure_smartie::providers::IProvider* (*create)();
  void (*destroy)(sure_smartie::providers::IProvider*);
};

using ProviderPluginEntryPointFn = const ProviderPluginDescriptor* (*)();

}  // namespace sure_smartie::plugins
