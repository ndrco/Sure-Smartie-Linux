#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "sure_smartie/core/Types.hpp"
#include "sure_smartie/providers/IProvider.hpp"

namespace sure_smartie::providers {

std::vector<std::string> builtinProviderNames();
bool isBuiltinProviderName(std::string_view provider_name);
std::unique_ptr<IProvider> createBuiltinProvider(std::string_view provider_name,
                                                 const core::AppConfig& config);
std::vector<std::unique_ptr<IProvider>> createBuiltinProviders(
    const std::vector<std::string>& provider_names,
    const core::AppConfig& config);

}  // namespace sure_smartie::providers
