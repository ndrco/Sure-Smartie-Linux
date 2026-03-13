#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "sure_smartie/core/Types.hpp"

namespace sure_smartie::core {

GlyphPattern barGlyphPattern(int level);
bool isReservedGlyphName(std::string_view name);
bool isValidCustomGlyphName(std::string_view name);
std::optional<GlyphPattern> findCustomGlyphPattern(
    std::string_view name,
    const std::vector<CustomGlyphDefinition>& custom_glyphs);

}  // namespace sure_smartie::core
