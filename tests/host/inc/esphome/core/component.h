#pragma once
#include <cstdint>
namespace esphome {
namespace setup_priority { const float DATA = 600.0f; }
class Component { public: virtual ~Component() = default; virtual void setup() {} virtual void loop() {} virtual void dump_config() {} virtual float get_setup_priority() const { return 0; } };
class PollingComponent : public Component { public: virtual void update() = 0; uint32_t get_update_interval() const { return 500; } };
#define LOG_UPDATE_INTERVAL(x)
}
