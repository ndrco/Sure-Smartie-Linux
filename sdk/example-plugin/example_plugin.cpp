#include "sure_smartie/plugins/ProviderPlugin.hpp"

#include <string>

namespace {

class ExternalExampleProvider final : public sure_smartie::providers::IProvider {
 public:
  std::string name() const override { return "external-example"; }

  void collect(sure_smartie::core::MetricMap& metrics) override {
    metrics["external.message"] = "from-sdk";
  }
};

sure_smartie::providers::IProvider* createProvider() {
  return new ExternalExampleProvider();
}

void destroyProvider(sure_smartie::providers::IProvider* provider) {
  delete provider;
}

}  // namespace

extern "C" const sure_smartie::plugins::ProviderPluginDescriptor*
sure_smartie_provider_plugin() {
  static const sure_smartie::plugins::ProviderPluginDescriptor descriptor{
      .api_version = sure_smartie::plugins::kProviderPluginApiVersion,
      .name = "sure_smartie_external_example",
      .create = &createProvider,
      .destroy = &destroyProvider,
  };

  return &descriptor;
}
