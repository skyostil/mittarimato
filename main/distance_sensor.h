#pragma once

#include <memory>

class DistanceSensor {
 public:
  virtual ~DistanceSensor() = default;

  virtual void Start() = 0;
  virtual void Stop() = 0;
  virtual int GetDistanceMM() = 0;

  enum class Range {
    kShort,
    kMedium,
    kLong,
  };
  virtual void SetRange(Range) = 0;

  static std::unique_ptr<DistanceSensor> Create();
};