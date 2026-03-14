#include "sure_smartie/engine/TemplateEngine.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <unordered_map>

#include "sure_smartie/core/GlyphSupport.hpp"

namespace sure_smartie::engine {
namespace {

std::vector<std::string> split(const std::string& value, char delimiter) {
  std::vector<std::string> parts;
  std::string current;

  for (char symbol : value) {
    if (symbol == delimiter) {
      parts.push_back(current);
      current.clear();
      continue;
    }
    current.push_back(symbol);
  }

  parts.push_back(current);
  return parts;
}

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t");
  if (first == std::string::npos) {
    return {};
  }
  const auto last = value.find_last_not_of(" \t");
  return value.substr(first, last - first + 1);
}

std::optional<double> parseNumber(const std::string& value) {
  try {
    return std::stod(value);
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<int> parseInt(const std::string& value) {
  int parsed = 0;
  const auto trimmed = trim(value);
  const auto* begin = trimmed.data();
  const auto* end = trimmed.data() + trimmed.size();
  const auto result = std::from_chars(begin, end, parsed);
  if (result.ec != std::errc{} || result.ptr != end) {
    return std::nullopt;
  }
  return parsed;
}

enum class TokenKind {
  literal,
  metric,
  bar,
  glyph,
  column,
};

struct CompiledToken {
  TokenKind kind{TokenKind::literal};
  std::string text;
  std::size_t width{0};
  double max_value{100.0};
  std::size_t column{0};
};

struct CompiledLine {
  std::vector<CompiledToken> tokens;
  bool uses_bar{false};
  std::optional<std::size_t> estimated_width;
};

std::mutex g_compiled_line_cache_mutex;
std::unordered_map<std::string, std::shared_ptr<const CompiledLine>> g_compiled_line_cache;

void pushLiteral(CompiledLine& compiled, std::string text) {
  if (text.empty()) {
    return;
  }

  if (!compiled.tokens.empty() &&
      compiled.tokens.back().kind == TokenKind::literal) {
    compiled.tokens.back().text += std::move(text);
    return;
  }

  compiled.tokens.push_back(CompiledToken{
      .kind = TokenKind::literal,
      .text = std::move(text),
  });
}

CompiledLine compileLine(const std::string& line) {
  CompiledLine compiled;
  std::size_t estimated_width = 0;
  bool width_known = true;
  std::size_t cursor = 0;

  while (cursor < line.size()) {
    const auto open = line.find('{', cursor);
    if (open == std::string::npos) {
      const auto literal = line.substr(cursor);
      pushLiteral(compiled, literal);
      estimated_width += literal.size();
      break;
    }

    if (open > cursor) {
      const auto literal = line.substr(cursor, open - cursor);
      pushLiteral(compiled, literal);
      estimated_width += literal.size();
    }

    const auto end = line.find('}', open + 1);
    if (end == std::string::npos) {
      const auto literal = line.substr(open);
      pushLiteral(compiled, literal);
      estimated_width += literal.size();
      break;
    }

    const auto key = line.substr(open + 1, end - open - 1);
    if (key.rfind("bar:", 0) == 0) {
      compiled.uses_bar = true;
      const auto arguments = split(key.substr(4), ',');
      if (arguments.size() >= 2 && arguments.size() <= 3) {
        const auto metric_key = trim(arguments[0]);
        const auto bar_width = parseInt(arguments[1]);
        const auto max_value = arguments.size() > 2
                                   ? parseNumber(trim(arguments[2]))
                                   : std::optional<double>{100.0};

        if (!metric_key.empty() && bar_width.has_value() && *bar_width > 0 &&
            max_value.has_value() && *max_value > 0.0) {
          compiled.tokens.push_back(CompiledToken{
              .kind = TokenKind::bar,
              .text = std::move(metric_key),
              .width = static_cast<std::size_t>(*bar_width),
              .max_value = *max_value,
          });
          estimated_width += static_cast<std::size_t>(*bar_width);
        } else {
          pushLiteral(compiled, "-");
          width_known = false;
        }
      } else {
        pushLiteral(compiled, "-");
        width_known = false;
      }
    } else if (key.rfind("glyph:", 0) == 0) {
      const auto glyph_name = trim(key.substr(6));
      compiled.tokens.push_back(CompiledToken{
          .kind = TokenKind::glyph,
          .text = glyph_name,
      });
      if (glyph_name.empty()) {
        width_known = false;
      } else {
        ++estimated_width;
      }
    } else if (key.rfind("at:", 0) == 0) {
      const auto column = parseInt(key.substr(3));
      compiled.tokens.push_back(CompiledToken{
          .kind = TokenKind::column,
          .text = {},
          .column = column.has_value() && *column > 0
                        ? static_cast<std::size_t>(*column)
                        : 0,
      });
      if (column.has_value() && *column > 0) {
        estimated_width = std::max(estimated_width, static_cast<std::size_t>(*column - 1));
      } else {
        width_known = false;
      }
    } else {
      const auto metric_key = trim(key);
      compiled.tokens.push_back(CompiledToken{
          .kind = TokenKind::metric,
          .text = metric_key,
      });
      if (metric_key.empty()) {
        width_known = false;
      } else {
        ++estimated_width;
      }
    }

    cursor = end + 1;
  }

  compiled.estimated_width =
      width_known ? std::optional<std::size_t>{estimated_width} : std::nullopt;
  return compiled;
}

std::shared_ptr<const CompiledLine> compiledLineFor(const std::string& line) {
  std::lock_guard<std::mutex> lock(g_compiled_line_cache_mutex);
  if (const auto it = g_compiled_line_cache.find(line);
      it != g_compiled_line_cache.end()) {
    return it->second;
  }

  auto compiled = std::make_shared<CompiledLine>(compileLine(line));
  g_compiled_line_cache.emplace(line, compiled);
  return compiled;
}

bool screenUsesBarGlyphs(const core::ScreenDefinition& screen) {
  for (const auto& line : screen.lines) {
    if (compiledLineFor(line)->uses_bar) {
      return true;
    }
  }

  return false;
}

}  // namespace

struct TemplateEngine::RenderContext {
  explicit RenderContext(const core::ScreenDefinition& screen,
                         const std::vector<core::CustomGlyphDefinition>& glyph_list)
      : custom_glyphs(glyph_list) {
    if (screenUsesBarGlyphs(screen)) {
      glyphs[1] = {.active = true, .name = "bar1", .pattern = core::barGlyphPattern(1)};
      glyphs[2] = {.active = true, .name = "bar2", .pattern = core::barGlyphPattern(2)};
      glyphs[3] = {.active = true, .name = "bar3", .pattern = core::barGlyphPattern(3)};
      glyphs[4] = {.active = true, .name = "bar4", .pattern = core::barGlyphPattern(4)};
      glyphs[5] = {.active = true, .name = "bar5", .pattern = core::barGlyphPattern(5)};
      glyphs[6] = {.active = true, .name = "bar_base", .pattern = core::barGlyphPattern(0)};
    }
  }

  std::optional<char> resolveGlyph(std::string_view raw_name) {
    const auto glyph_name = trim(std::string(raw_name));
    if (const auto it = user_slots.find(glyph_name); it != user_slots.end()) {
      return static_cast<char>(it->second);
    }

    const auto pattern = core::findCustomGlyphPattern(glyph_name, custom_glyphs);
    if (!pattern.has_value()) {
      return std::nullopt;
    }

    for (std::size_t slot = 0; slot < glyphs.size(); ++slot) {
      if (glyphs[slot].active) {
        continue;
      }

      glyphs[slot] = {
          .active = true,
          .name = glyph_name,
          .pattern = *pattern,
      };
      user_slots[glyph_name] = static_cast<std::uint8_t>(slot);
      return static_cast<char>(slot);
    }

    return std::nullopt;
  }

  core::GlyphSlotBank glyphs;
  const std::vector<core::CustomGlyphDefinition>& custom_glyphs;
  std::unordered_map<std::string, std::uint8_t> user_slots;
};

core::Frame TemplateEngine::render(const core::ScreenDefinition& screen,
                                   const core::MetricMap& metrics,
                                   const core::DisplayGeometry& geometry) const {
  return render(screen, metrics, geometry, {});
}

core::Frame TemplateEngine::render(
    const core::ScreenDefinition& screen,
    const core::MetricMap& metrics,
    const core::DisplayGeometry& geometry,
    const std::vector<core::CustomGlyphDefinition>& custom_glyphs) const {
  return renderDetailed(screen, metrics, geometry, custom_glyphs).frame;
}

core::RenderedFrame TemplateEngine::renderDetailed(
    const core::ScreenDefinition& screen,
    const core::MetricMap& metrics,
    const core::DisplayGeometry& geometry,
    const std::vector<core::CustomGlyphDefinition>& custom_glyphs) const {
  RenderContext context(screen, custom_glyphs);
  core::RenderedFrame rendered;
  rendered.frame.reserve(geometry.rows);

  for (std::size_t row = 0; row < geometry.rows; ++row) {
    const std::string source =
        row < screen.lines.size() ? screen.lines[row] : std::string{};
    rendered.frame.push_back(fitToWidth(renderLine(source, metrics, context), geometry.cols));
  }

  rendered.glyphs = context.glyphs;
  return rendered;
}

std::string TemplateEngine::fitToWidth(std::string text, std::size_t width) {
  for (char& symbol : text) {
    const auto code = static_cast<unsigned char>(symbol);
    if (std::iscntrl(code) && code >= static_cast<unsigned char>(core::kGlyphSlotCount)) {
      symbol = ' ';
    }
  }

  if (text.size() > width) {
    text.resize(width);
  } else if (text.size() < width) {
    text.resize(width, ' ');
  }

  return text;
}

std::optional<std::size_t> TemplateEngine::estimateRenderedWidth(
    const std::string& line) {
  return compiledLineFor(line)->estimated_width;
}

std::string TemplateEngine::renderBar(const std::string& metric_key,
                                      std::size_t width,
                                      double max_value,
                                      const core::MetricMap& metrics) {
  std::string bar(width, core::kGlyphBarBase);
  const auto metric = metrics.find(metric_key);
  if (metric == metrics.end() || width == 0 || max_value <= 0.0) {
    return bar;
  }

  const auto value = parseNumber(metric->second);
  if (!value.has_value()) {
    return bar;
  }

  const auto clamped_value = std::clamp(*value, 0.0, max_value);
  const auto total_steps = static_cast<int>(width * 5);
  const auto filled_steps = static_cast<int>(
      std::lround((clamped_value / max_value) * static_cast<double>(total_steps)));

  for (std::size_t index = 0; index < width; ++index) {
    const int cell_fill = std::clamp(filled_steps - static_cast<int>(index * 5), 0, 5);
    if (cell_fill > 0) {
      bar[index] = static_cast<char>(cell_fill);
    }
  }

  return bar;
}

std::string TemplateEngine::renderLine(const std::string& line,
                                       const core::MetricMap& metrics,
                                       RenderContext& context) {
  const auto compiled = compiledLineFor(line);
  std::string output;
  output.reserve(line.size());

  for (const auto& token : compiled->tokens) {
    switch (token.kind) {
      case TokenKind::literal:
        output.append(token.text);
        break;
      case TokenKind::metric: {
        const auto metric = metrics.find(token.text);
        output.append(metric != metrics.end() ? metric->second : "-");
        break;
      }
      case TokenKind::bar:
        output.append(renderBar(token.text, token.width, token.max_value, metrics));
        break;
      case TokenKind::glyph:
        if (const auto glyph = context.resolveGlyph(token.text); glyph.has_value()) {
          output.push_back(*glyph);
        } else {
          output.push_back('?');
        }
        break;
      case TokenKind::column:
        if (token.column > 0) {
          const auto target = token.column - 1;
          if (output.size() < target) {
            output.resize(target, ' ');
          }
        }
        break;
    }
  }

  return output;
}

}  // namespace sure_smartie::engine
