#pragma once

#include <functional>
#include <memory>

class Display {
 public:
  virtual ~Display() = default;

  virtual size_t Width() = 0;
  virtual size_t Height() = 0;

  using Renderer = std::function<void(uint32_t*)>;
  virtual size_t RenderBatchSize() = 0;
  virtual void Render(const Renderer&) = 0;

  static std::unique_ptr<Display> Create();
};
