#include "sure_smartie/core/ConfigValidator.hpp"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "sure_smartie/engine/TemplateEngine.hpp"
#include "sure_smartie/providers/BuiltinProviderFactory.hpp"

namespace sure_smartie::core {
namespace {

void addDiagnostic(std::vector<Diagnostic>& diagnostics,
                   DiagnosticSeverity severity,
                   std::string field_path,
                   std::string message) {
  diagnostics.push_back(Diagnostic{
      .severity = severity,
      .field_path = std::move(field_path),
      .message = std::move(message),
  });
}

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

std::optional<double> parseDouble(const std::string& value) {
  try {
    return std::stod(trim(value));
  } catch (...) {
    return std::nullopt;
  }
}

void validateTemplateLine(std::vector<Diagnostic>& diagnostics,
                          const AppConfig& config,
                          std::size_t screen_index,
                          std::size_t line_index,
                          const std::string& line) {
  const auto field_prefix =
      "screens[" + std::to_string(screen_index) + "].lines[" + std::to_string(line_index) + "]";

  if (config.display.cols > 0) {
    const auto estimated_width = engine::TemplateEngine::estimateRenderedWidth(line);
    if (estimated_width.has_value() &&
        static_cast<int>(*estimated_width) > config.display.cols) {
      addDiagnostic(diagnostics,
                    DiagnosticSeverity::warning,
                    field_prefix,
                    "rendered line is wider than display width and will be truncated");
    }
  }

  for (std::size_t index = 0; index < line.size(); ++index) {
    if (line[index] != '{') {
      continue;
    }

    const auto end = line.find('}', index + 1);
    if (end == std::string::npos) {
      addDiagnostic(diagnostics,
                    DiagnosticSeverity::error,
                    field_prefix,
                    "unclosed template expression");
      return;
    }

    const auto token = line.substr(index + 1, end - index - 1);
    if (token.rfind("bar:", 0) == 0) {
      const auto args = split(token.substr(4), ',');
      if (args.size() < 2 || args.size() > 3) {
        addDiagnostic(diagnostics,
                      DiagnosticSeverity::error,
                      field_prefix,
                      "bar macro must be {bar:metric,width[,max]}");
      } else {
        const auto metric_key = trim(args[0]);
        const auto width = parseInt(args[1]);
        const auto max_value =
            args.size() == 3 ? parseDouble(args[2]) : std::optional<double>{100.0};

        if (metric_key.empty()) {
          addDiagnostic(diagnostics,
                        DiagnosticSeverity::error,
                        field_prefix,
                        "bar macro requires a metric key");
        }
        if (!width.has_value() || *width <= 0) {
          addDiagnostic(diagnostics,
                        DiagnosticSeverity::error,
                        field_prefix,
                        "bar macro width must be a positive integer");
        }
        if (!max_value.has_value() || *max_value <= 0.0) {
          addDiagnostic(diagnostics,
                        DiagnosticSeverity::error,
                        field_prefix,
                        "bar macro max must be a positive number");
        }
      }
    } else if (token.rfind("at:", 0) == 0) {
      const auto column = parseInt(token.substr(3));
      if (!column.has_value() || *column <= 0) {
        addDiagnostic(diagnostics,
                      DiagnosticSeverity::error,
                      field_prefix,
                      "at macro must be {at:column} with a positive column number");
      }
    } else if (token.empty()) {
      addDiagnostic(diagnostics,
                    DiagnosticSeverity::error,
                    field_prefix,
                    "empty template expression");
    }

    index = end;
  }
}

}  // namespace

std::vector<Diagnostic> ConfigValidator::validate(const AppConfig& config) {
  std::vector<Diagnostic> diagnostics;

  if (config.display.type.empty()) {
    addDiagnostic(diagnostics,
                  DiagnosticSeverity::error,
                  "display.type",
                  "display type must not be empty");
  } else if (config.display.type != "sure" && config.display.type != "stdout") {
    addDiagnostic(diagnostics,
                  DiagnosticSeverity::error,
                  "display.type",
                  "display type must be either 'sure' or 'stdout'");
  }

  if (config.device.empty() && config.display.type != "stdout") {
    addDiagnostic(diagnostics,
                  DiagnosticSeverity::error,
                  "device",
                  "device path must not be empty for non-stdout displays");
  }

  if (!config.device.empty() && config.display.type != "stdout" &&
      !std::filesystem::exists(config.device)) {
    addDiagnostic(diagnostics,
                  DiagnosticSeverity::warning,
                  "device",
                  "device path does not exist on this machine");
  }

  if (config.baudrate <= 0) {
    addDiagnostic(diagnostics,
                  DiagnosticSeverity::error,
                  "baudrate",
                  "baudrate must be positive");
  }

  if (config.refresh_interval.count() <= 0) {
    addDiagnostic(diagnostics,
                  DiagnosticSeverity::error,
                  "refresh_ms",
                  "refresh interval must be positive");
  }

  if (config.display.cols <= 0) {
    addDiagnostic(diagnostics,
                  DiagnosticSeverity::error,
                  "display.cols",
                  "display cols must be positive");
  }

  if (config.display.rows <= 0) {
    addDiagnostic(diagnostics,
                  DiagnosticSeverity::error,
                  "display.rows",
                  "display rows must be positive");
  }

  if (config.display.contrast < 1 || config.display.contrast > 254) {
    addDiagnostic(diagnostics,
                  DiagnosticSeverity::error,
                  "display.contrast",
                  "contrast must be in range 1..254");
  }

  if (config.display.brightness < 1 || config.display.brightness > 254) {
    addDiagnostic(diagnostics,
                  DiagnosticSeverity::error,
                  "display.brightness",
                  "brightness must be in range 1..254");
  }

  std::vector<std::string> seen_providers;
  for (std::size_t index = 0; index < config.providers.size(); ++index) {
    const auto& provider_name = config.providers[index];
    const auto field = "providers[" + std::to_string(index) + "]";
    if (!providers::isBuiltinProviderName(provider_name)) {
      addDiagnostic(diagnostics,
                    DiagnosticSeverity::error,
                    field,
                    "unknown built-in provider");
    }

    if (std::find(seen_providers.begin(), seen_providers.end(), provider_name) !=
        seen_providers.end()) {
      addDiagnostic(diagnostics,
                    DiagnosticSeverity::warning,
                    field,
                    "provider is duplicated");
    } else {
      seen_providers.push_back(provider_name);
    }
  }

  for (std::size_t index = 0; index < config.plugin_paths.size(); ++index) {
    const auto& plugin_path = config.plugin_paths[index];
    const auto field = "plugin_paths[" + std::to_string(index) + "]";
    if (plugin_path.empty()) {
      addDiagnostic(diagnostics,
                    DiagnosticSeverity::error,
                    field,
                    "plugin path must not be empty");
      continue;
    }

    if (!std::filesystem::exists(plugin_path)) {
      addDiagnostic(diagnostics,
                    DiagnosticSeverity::warning,
                    field,
                    "plugin path does not exist on this machine");
    }
  }

  if (config.screens.empty()) {
    addDiagnostic(diagnostics,
                  DiagnosticSeverity::error,
                  "screens",
                  "at least one screen must be configured");
  }

  for (std::size_t screen_index = 0; screen_index < config.screens.size(); ++screen_index) {
    const auto& screen = config.screens[screen_index];
    const auto screen_prefix = "screens[" + std::to_string(screen_index) + "]";

    if (screen.name.empty()) {
      addDiagnostic(diagnostics,
                    DiagnosticSeverity::warning,
                    screen_prefix + ".name",
                    "screen name is empty");
    }

    if (screen.interval.count() <= 0) {
      addDiagnostic(diagnostics,
                    DiagnosticSeverity::error,
                    screen_prefix + ".interval_ms",
                    "screen interval must be positive");
    }

    if (screen.lines.empty()) {
      addDiagnostic(diagnostics,
                    DiagnosticSeverity::error,
                    screen_prefix + ".lines",
                    "screen must contain at least one line");
      continue;
    }

    if (config.display.rows > 0 &&
        static_cast<int>(screen.lines.size()) > config.display.rows) {
      addDiagnostic(diagnostics,
                    DiagnosticSeverity::warning,
                    screen_prefix + ".lines",
                    "screen has more lines than display rows");
    }

    for (std::size_t line_index = 0; line_index < screen.lines.size(); ++line_index) {
      validateTemplateLine(diagnostics, config, screen_index, line_index,
                           screen.lines[line_index]);
    }
  }

  return diagnostics;
}

}  // namespace sure_smartie::core
