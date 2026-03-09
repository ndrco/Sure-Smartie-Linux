#pragma once

#include <string>

#include "sure_smartie/providers/IProvider.hpp"

namespace sure_smartie::providers {

class RamProvider : public IProvider {
 public:
  std::string name() const override;
  void collect(core::MetricMap& metrics) override;
};

}  // namespace sure_smartie::providers
