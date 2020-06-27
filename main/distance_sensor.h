#pragma once

#include <memory>

class DistanceSensor {
 public:
  virtual ~DistanceSensor() = default;

  virtual void Start(uint32_t period_ms) = 0;
  virtual void Stop() = 0;
  virtual bool GetDistanceMM(uint32_t& distance_mm) = 0;

  enum class Range {
    kShort,   // Up to 1.3 m.
    kMedium,  // Up to 3 m.
    kLong,    // Up to 4 m.
  };
  virtual void SetRange(Range) = 0;

  static std::unique_ptr<DistanceSensor> Create();
};