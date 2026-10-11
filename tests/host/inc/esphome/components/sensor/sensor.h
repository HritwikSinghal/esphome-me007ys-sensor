#pragma once
#include <vector>
namespace esphome { namespace sensor {
class Sensor { public: std::vector<float> pubs; void publish_state(float v) { pubs.push_back(v); } };
} }
#define LOG_SENSOR(a, b, c)
