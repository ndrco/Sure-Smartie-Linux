#pragma once

#include <memory>

#include "sure_smartie/core/Types.hpp"
#include "sure_smartie/display/IDisplay.hpp"

namespace sure_smartie::display {

std::unique_ptr<IDisplay> createDisplay(const core::AppConfig& config,
                                        const core::RuntimeOptions& options);

}  // namespace sure_smartie::display
