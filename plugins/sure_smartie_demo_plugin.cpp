#include "sure_smartie/plugins/ProviderPlugin.hpp"

#include <string>

namespace {

class DemoProvider final : public sure_smartie::providers::IProvider {
 public:
  std::string name() const override { return "demo"; }

  void collect(sure_smartie::core::MetricMap& metrics) override {
    ++counter_;
    metrics["demo.counter"] = std::to_string(counter_);
    metrics["demo.message"] = "plugin-ok";
  }

 private:
  int counter_{0};
};

sure_smartie::providers::IProvider* createProvider() { return new DemoProvider(); }

void destroyProvider(sure_smartie::providers::IProvider* provider) { delete provider; }

}  // namespace

extern "C" const sure_smartie::plugins::ProviderPluginDescriptor*
sure_smartie_provider_plugin() {
  static const sure_smartie::plugins::ProviderPluginDescriptor descriptor{
      .api_version = sure_smartie::plugins::kProviderPluginApiVersion,
      .name = "sure_smartie_demo_plugin",
      .create = &createProvider,
      .destroy = &destroyProvider,
  };

  return &descriptor;
}
