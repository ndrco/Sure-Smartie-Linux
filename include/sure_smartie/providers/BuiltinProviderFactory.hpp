#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "sure_smartie/providers/IProvider.hpp"

namespace sure_smartie::providers {

std::vector<std::string> builtinProviderNames();
bool isBuiltinProviderName(std::string_view provider_name);
std::unique_ptr<IProvider> createBuiltinProvider(std::string_view provider_name);
std::vector<std::unique_ptr<IProvider>> createBuiltinProviders(
    const std::vector<std::string>& provider_names);

}  // namespace sure_smartie::providers
