#pragma once

#include <memory>

class Display {
 public:
  virtual ~Display() = default;

  virtual size_t Width() = 0;
  virtual size_t Height() = 0;

  static std::unique_ptr<Display> Create();
};
