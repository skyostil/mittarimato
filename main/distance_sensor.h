#pragma once

class DistanceSensor {
 public:
  DistanceSensor();

  void Start();
  void Stop();

  int GetDistanceCM();
};