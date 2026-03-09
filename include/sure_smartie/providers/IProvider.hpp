#pragma once

#include <string>

#include "sure_smartie/core/Types.hpp"

namespace sure_smartie::providers {

class IProvider {
 public:
  virtual ~IProvider() = default;

  virtual std::string name() const = 0;
  virtual void collect(core::MetricMap& metrics) = 0;
};

}  // namespace sure_smartie::providers
