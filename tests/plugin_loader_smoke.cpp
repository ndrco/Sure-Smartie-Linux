#include <cassert>
#include <string>
#include <vector>

#include "sure_smartie/core/Types.hpp"
#include "sure_smartie/plugins/PluginLoader.hpp"

int main(int argc, char** argv) {
  assert(argc == 2);

  auto providers =
      sure_smartie::plugins::loadProviderPlugins(std::vector<std::string>{argv[1]});
  assert(providers.size() == 1);
  assert(providers.front()->name() == "demo");

  sure_smartie::core::MetricMap metrics;
  providers.front()->collect(metrics);
  assert(metrics["demo.counter"] == "1");
  assert(metrics["demo.message"] == "plugin-ok");

  providers.front()->collect(metrics);
  assert(metrics["demo.counter"] == "2");

  return 0;
}
