#pragma once
#include <string>
#include <vector>
namespace esphome { namespace text_sensor {
class TextSensor { public: std::vector<std::string> pubs; void publish_state(const std::string &v) { pubs.push_back(v); } };
} }
#define LOG_TEXT_SENSOR(a, b, c)
