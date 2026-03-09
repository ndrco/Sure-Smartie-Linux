#pragma once

#include <memory>
#include <string>
#include <vector>

#include "sure_smartie/providers/IProvider.hpp"

namespace sure_smartie::plugins {

std::vector<std::unique_ptr<sure_smartie::providers::IProvider>> loadProviderPlugins(
    const std::vector<std::string>& plugin_paths);

}  // namespace sure_smartie::plugins
