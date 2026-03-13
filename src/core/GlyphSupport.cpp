#include "sure_smartie/core/GlyphSupport.hpp"

#include <algorithm>
#include <cctype>

namespace sure_smartie::core {
namespace {

std::string normalizeName(std::string_view name) {
  std::string normalized(name);
  const auto first = std::find_if_not(normalized.begin(), normalized.end(), [](unsigned char symbol) {
    return std::isspace(symbol) != 0;
  });
  if (first == normalized.end()) {
    return {};
  }

  const auto last =
      std::find_if_not(normalized.rbegin(), normalized.rend(), [](unsigned char symbol) {
        return std::isspace(symbol) != 0;
      }).base();
  normalized = std::string(first, last);
  return normalized;
}

}  // namespace

GlyphPattern barGlyphPattern(int level) {
  switch (level) {
    case 1:
      return GlyphPattern{0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
    case 2:
      return GlyphPattern{0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1F};
    case 3:
      return GlyphPattern{0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1F};
    case 4:
      return GlyphPattern{0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1F};
    case 5:
      return GlyphPattern{0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F};
    default:
      return GlyphPattern{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F};
  }
}

bool isReservedGlyphName(std::string_view name) {
  const auto normalized = normalizeName(name);
  return normalized == "bar1" || normalized == "bar2" || normalized == "bar3" ||
         normalized == "bar4" || normalized == "bar5" || normalized == "bar_base";
}

bool isValidCustomGlyphName(std::string_view name) {
  const auto normalized = normalizeName(name);
  if (normalized.empty()) {
    return false;
  }

  return std::all_of(normalized.begin(), normalized.end(), [](unsigned char symbol) {
    return std::isalnum(symbol) != 0 || symbol == '_' || symbol == '-';
  });
}

std::optional<GlyphPattern> findCustomGlyphPattern(
    std::string_view name,
    const std::vector<CustomGlyphDefinition>& custom_glyphs) {
  const auto normalized = normalizeName(name);
  for (const auto& glyph : custom_glyphs) {
    if (normalizeName(glyph.name) == normalized) {
      return glyph.pattern;
    }
  }

  return std::nullopt;
}

}  // namespace sure_smartie::core
