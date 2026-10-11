#pragma once

#include <string>
#include <cmath>
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace me007ys {

enum class OutOfRangeBehavior : uint8_t { NAN_OUT, PUBLISH_LIMIT, HOLD_LAST };
enum class RawPolicy : uint8_t { ALL, VALID_ONLY };

class ME007YSSensor : public PollingComponent, public uart::UARTDevice, public sensor::Sensor {
 public:
  // YAML setters (cm -> mm where needed)
  void set_min_valid_cm(float cm) { min_valid_mm_ = static_cast<uint16_t>(cm * 10.0f + 0.5f); }
  void set_max_valid_cm(float cm) { max_valid_mm_ = static_cast<uint16_t>(cm * 10.0f + 0.5f); }
  void set_too_close_behavior(const std::string &s) {
    if (s == "min") too_close_behavior_ = OutOfRangeBehavior::PUBLISH_LIMIT;
    else if (s == "last") too_close_behavior_ = OutOfRangeBehavior::HOLD_LAST;
    else too_close_behavior_ = OutOfRangeBehavior::NAN_OUT;
  }
  void set_guard_cm(float v) { guard_mm_ = static_cast<uint16_t>(v * 10.0f + 0.5f); }
  void set_max_step_cm(float v) { max_step_mm_ = static_cast<uint16_t>(v * 10.0f + 0.5f); }
  void set_require_consecutive(uint8_t n) { require_consecutive_ = n; }
  void set_debug_raw(bool v) { debug_raw_ = v; }

  // Diagnostics wiring
  void set_frame_rate_sensor(sensor::Sensor *s) { frame_rate_sensor_ = s; }
  void set_status_sensor(text_sensor::TextSensor *s) { status_sensor_ = s; }
  void set_raw_mm_sensor(sensor::Sensor *s) { raw_mm_sensor_ = s; }
  void set_raw_mm_policy(const std::string &s) { raw_policy_ = (s == "valid_only") ? RawPolicy::VALID_ONLY : RawPolicy::ALL; }

  void update() override;

 protected:
  void publish_status_(const char *s) {
    if (status_sensor_ != nullptr) status_sensor_->publish_state(s);
  }

  // Thresholds (mm)
  uint16_t min_valid_mm_{280};   // 28.0 cm
  uint16_t max_valid_mm_{4500};  // 450.0 cm
  uint16_t guard_mm_{50};        // 5.0 cm above min

  // Behavior
  OutOfRangeBehavior too_close_behavior_{OutOfRangeBehavior::NAN_OUT};

  // Robustness
  uint16_t max_step_mm_{0};      // 0 = off
  uint8_t  require_consecutive_{1};

  // State
  float last_valid_cm_{NAN};
  float last_published_cm_{NAN};
  float last_candidate_cm_{NAN};
  uint8_t streak_{0};

  bool debug_raw_{false};

  // Diagnostics
  sensor::Sensor *frame_rate_sensor_{nullptr};
  text_sensor::TextSensor *status_sensor_{nullptr};
  sensor::Sensor *raw_mm_sensor_{nullptr};
  RawPolicy raw_policy_{RawPolicy::ALL};
};

}  // namespace me007ys
}  // namespace esphome
