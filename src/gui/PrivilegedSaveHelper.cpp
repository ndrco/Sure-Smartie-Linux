#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <string_view>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

std::filesystem::path normalizePath(const std::filesystem::path& path) {
  std::error_code error;
  const auto absolute = std::filesystem::absolute(path, error);
  return (error ? path : absolute).lexically_normal();
}

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return {};
  }

  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::string stripQuotes(std::string value) {
  if (value.size() >= 2 &&
      ((value.front() == '"' && value.back() == '"') ||
       (value.front() == '\'' && value.back() == '\''))) {
    return value.substr(1, value.size() - 2);
  }

  return value;
}

bool copyFileContents(const std::filesystem::path& source_path,
                      const std::filesystem::path& target_path,
                      std::string* error_message) {
  std::ifstream source(source_path, std::ios::binary);
  if (!source) {
    if (error_message != nullptr) {
      *error_message = "Unable to open temporary config: " + source_path.string();
    }
    return false;
  }

  std::ofstream target(target_path, std::ios::binary | std::ios::trunc);
  if (!target) {
    if (error_message != nullptr) {
      *error_message = "Unable to open privileged temp file: " + target_path.string();
    }
    return false;
  }

  target << source.rdbuf();
  if (!source.good() && !source.eof()) {
    if (error_message != nullptr) {
      *error_message = "Unable to read temporary config: " + source_path.string();
    }
    return false;
  }
  if (!target) {
    if (error_message != nullptr) {
      *error_message = "Unable to write privileged temp file: " + target_path.string();
    }
    return false;
  }

  target.flush();
  if (!target) {
    if (error_message != nullptr) {
      *error_message = "Unable to flush privileged temp file: " + target_path.string();
    }
    return false;
  }

  return true;
}

int runCommand(const std::vector<std::string>& arguments) {
  if (arguments.empty()) {
    return 1;
  }

  pid_t child = ::fork();
  if (child < 0) {
    return 1;
  }

  if (child == 0) {
    std::vector<char*> argv;
    argv.reserve(arguments.size() + 1);
    for (const auto& argument : arguments) {
      argv.push_back(const_cast<char*>(argument.c_str()));
    }
    argv.push_back(nullptr);
    ::execvp(argv[0], argv.data());
    _exit(127);
  }

  int status = 0;
  if (::waitpid(child, &status, 0) < 0) {
    return 1;
  }

  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }

  return 1;
}

bool commandSucceeds(const std::vector<std::string>& arguments) {
  return runCommand(arguments) == 0;
}

std::string configuredServiceName() {
#ifdef SURE_SMARTIE_ENV_FILE_PATH
  std::ifstream env_file(SURE_SMARTIE_ENV_FILE_PATH);
  std::string line;
  while (std::getline(env_file, line)) {
    line = trim(line);
    if (line.empty() || line[0] == '#') {
      continue;
    }

    constexpr const char* key = "SURE_SMARTIE_SERVICE_NAME=";
    if (line.rfind(key, 0) == 0) {
      return stripQuotes(trim(line.substr(std::strlen(key))));
    }
  }
#endif

  if (commandSucceeds({"systemctl", "is-active", "--quiet", "sure-smartie-linux-root.service"})) {
    return "sure-smartie-linux-root.service";
  }
  if (commandSucceeds({"systemctl", "is-active", "--quiet", "sure-smartie-linux.service"})) {
    return "sure-smartie-linux.service";
  }
  if (commandSucceeds({"systemctl", "is-enabled", "--quiet", "sure-smartie-linux-root.service"})) {
    return "sure-smartie-linux-root.service";
  }
  if (commandSucceeds({"systemctl", "is-enabled", "--quiet", "sure-smartie-linux.service"})) {
    return "sure-smartie-linux.service";
  }

  return "sure-smartie-linux-root.service";
}

bool restartConfiguredService(std::string* error_message) {
  const std::string service_name = configuredServiceName();
  if (runCommand({"systemctl", "restart", service_name}) == 0) {
    return true;
  }

  if (error_message != nullptr) {
    *error_message = "Unable to restart service: " + service_name;
  }
  return false;
}

}  // namespace

int main(int argc, char** argv) {
  bool restart_service = false;
  int argument_index = 1;
  if (argc > 1 && std::string_view(argv[1]) == "--restart-service") {
    restart_service = true;
    argument_index = 2;
  }

  if (argc - argument_index != 2) {
    std::cerr << "Usage: " << argv[0]
              << " [--restart-service] <source-temp-json> <target-config>\n";
    return 2;
  }

#ifndef SURE_SMARTIE_DEFAULT_CONFIG_PATH
  std::cerr << "Privileged save helper is missing SURE_SMARTIE_DEFAULT_CONFIG_PATH\n";
  return 2;
#else
  const auto source_path = normalizePath(argv[argument_index]);
  const auto target_path = normalizePath(argv[argument_index + 1]);
  const auto allowed_target =
      normalizePath(std::filesystem::path{SURE_SMARTIE_DEFAULT_CONFIG_PATH});

  if (target_path != allowed_target) {
    std::cerr << "Refusing to write outside the managed system config path\n";
    return 2;
  }

  std::error_code error;
  if (!std::filesystem::exists(source_path, error) ||
      !std::filesystem::is_regular_file(source_path, error)) {
    std::cerr << "Temporary config file is missing: " << source_path << '\n';
    return 2;
  }

  const auto parent_path = target_path.parent_path();
  if (!parent_path.empty()) {
    std::filesystem::create_directories(parent_path, error);
    if (error) {
      std::cerr << "Unable to create target directory: " << parent_path << '\n';
      return 1;
    }
  }

  std::string temp_template = (parent_path / ".sure-smartie-config-XXXXXX").string();
  std::vector<char> writable_template(temp_template.begin(), temp_template.end());
  writable_template.push_back('\0');

  const int temp_fd = ::mkstemp(writable_template.data());
  if (temp_fd < 0) {
    std::cerr << "Unable to create privileged temp file: " << std::strerror(errno) << '\n';
    return 1;
  }

  const std::filesystem::path temp_path{writable_template.data()};
  ::close(temp_fd);

  bool success = false;
  std::string message;
  try {
    if (!copyFileContents(source_path, temp_path, &message)) {
      throw std::runtime_error(message);
    }

    std::filesystem::permissions(
        temp_path,
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write |
            std::filesystem::perms::group_read | std::filesystem::perms::others_read,
        std::filesystem::perm_options::replace,
        error);
    if (error) {
      throw std::runtime_error("Unable to set permissions on privileged temp file");
    }

    std::filesystem::rename(temp_path, target_path, error);
    if (error) {
      throw std::runtime_error("Unable to replace target config: " + error.message());
    }

    if (restart_service && !restartConfiguredService(&message)) {
      throw std::runtime_error(message);
    }

    success = true;
  } catch (const std::exception& exception) {
    std::cerr << exception.what() << '\n';
  }

  if (!success) {
    std::filesystem::remove(temp_path, error);
    return 1;
  }

  return 0;
#endif
}
