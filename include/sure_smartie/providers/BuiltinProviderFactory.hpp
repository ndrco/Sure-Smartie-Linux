#pragma once

#include <memory>
#include <string>
#include <vector>

#include "sure_smartie/providers/IProvider.hpp"

namespace sure_smartie::providers {

std::vector<std::unique_ptr<IProvider>> createBuiltinProviders(
    const std::vector<std::string>& provider_names);

}  // namespace sure_smartie::providers
