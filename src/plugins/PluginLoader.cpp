#include "sure_smartie/plugins/PluginLoader.hpp"

#include <dlfcn.h>

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "sure_smartie/plugins/ProviderPlugin.hpp"

namespace sure_smartie::plugins {
namespace {

struct LoadedLibrary {
  explicit LoadedLibrary(std::filesystem::path path)
      : path(std::move(path)) {
    handle = ::dlopen(this->path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr) {
      throw std::runtime_error("dlopen failed for " + this->path.string() + ": " +
                               ::dlerror());
    }
  }

  ~LoadedLibrary() {
    if (handle != nullptr) {
      ::dlclose(handle);
    }
  }

  std::filesystem::path path;
  void* handle{nullptr};
};

class DynamicProvider final : public sure_smartie::providers::IProvider {
 public:
  DynamicProvider(std::shared_ptr<LoadedLibrary> library,
                  const ProviderPluginDescriptor* descriptor)
      : library_(std::move(library)), descriptor_(descriptor) {
    if (descriptor_ == nullptr || descriptor_->create == nullptr ||
        descriptor_->destroy == nullptr) {
      throw std::runtime_error("Invalid provider plugin descriptor");
    }

    provider_ = descriptor_->create();
    if (provider_ == nullptr) {
      throw std::runtime_error("Plugin " + library_->path.string() +
                               " returned null provider");
    }
  }

  ~DynamicProvider() override {
    if (provider_ != nullptr) {
      descriptor_->destroy(provider_);
      provider_ = nullptr;
    }
  }

  std::string name() const override { return provider_->name(); }

  void collect(core::MetricMap& metrics) override { provider_->collect(metrics); }

 private:
  std::shared_ptr<LoadedLibrary> library_;
  const ProviderPluginDescriptor* descriptor_;
  sure_smartie::providers::IProvider* provider_{nullptr};
};

}  // namespace

std::vector<std::unique_ptr<sure_smartie::providers::IProvider>> loadProviderPlugins(
    const std::vector<std::string>& plugin_paths) {
  std::vector<std::unique_ptr<sure_smartie::providers::IProvider>> providers;

  for (const auto& plugin_path : plugin_paths) {
    auto library =
        std::make_shared<LoadedLibrary>(std::filesystem::path{plugin_path});
    ::dlerror();
    const auto entry = reinterpret_cast<ProviderPluginEntryPointFn>(
        ::dlsym(library->handle, kProviderPluginEntryPoint));
    if (entry == nullptr) {
      throw std::runtime_error("dlsym failed for " + library->path.string() + ": " +
                               ::dlerror());
    }

    const auto* descriptor = entry();
    if (descriptor == nullptr) {
      throw std::runtime_error("Plugin " + library->path.string() +
                               " returned null descriptor");
    }
    if (descriptor->api_version != kProviderPluginApiVersion) {
      throw std::runtime_error("Plugin " + library->path.string() +
                               " has unsupported API version");
    }

    providers.push_back(
        std::make_unique<DynamicProvider>(std::move(library), descriptor));
  }

  return providers;
}

}  // namespace sure_smartie::plugins
