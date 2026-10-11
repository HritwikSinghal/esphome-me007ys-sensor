#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace me007ys {

enum class OutOfRangeBehavior : uint8_t { NAN_OUT, PUBLISH_LIMIT, HOLD_LAST };
enum class RawPolicy : uint8_t { ALL, VALID_ONLY };

// Outcome of the most recent frame; update() publishes from this once per interval.
enum class FrameOutcome : uint8_t { NONE, OK, PENDING, TOO_CLOSE, TOO_FAR };

// Bytes are parsed in loop() so the UART buffer never fills, whatever update_interval is;
// update() then publishes one state per interval.
class ME007YSSensor : public PollingComponent, public uart::UARTDevice, public sensor::Sensor {
 public:
  // Thresholds are kept in float mm: uint16 storage wrapped for large YAML values.
  void set_min_valid_cm(float cm) { min_valid_mm_ = cm * 10.0f; }
  void set_max_valid_cm(float cm) { max_valid_mm_ = cm * 10.0f; }
  void set_too_close_behavior(const std::string &s) {
    if (s == "min")
      too_close_behavior_ = OutOfRangeBehavior::PUBLISH_LIMIT;
    else if (s == "last")
      too_close_behavior_ = OutOfRangeBehavior::HOLD_LAST;
    else
      too_close_behavior_ = OutOfRangeBehavior::NAN_OUT;
  }
  void set_guard_cm(float v) { guard_mm_ = v * 10.0f; }
  void set_max_step_cm(float v) { max_step_mm_ = v * 10.0f; }
  void set_require_consecutive(uint8_t n) { require_consecutive_ = n < 1 ? 1 : n; }
  void set_debug_raw(bool v) { debug_raw_ = v; }

  void set_frame_rate_sensor(sensor::Sensor *s) { frame_rate_sensor_ = s; }
  void set_status_sensor(text_sensor::TextSensor *s) { status_sensor_ = s; }
  void set_raw_mm_sensor(sensor::Sensor *s) { raw_mm_sensor_ = s; }
  void set_raw_mm_policy(const std::string &s) {
    raw_policy_ = (s == "valid_only") ? RawPolicy::VALID_ONLY : RawPolicy::ALL;
  }

  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void handle_frame_(uint16_t distance_mm);
  void publish_status_(const char *s);

  // Thresholds (mm)
  float min_valid_mm_{280.0f};
  float max_valid_mm_{4500.0f};
  float guard_mm_{50.0f};
  float max_step_mm_{0.0f};  // 0 = off
  uint8_t require_consecutive_{1};
  OutOfRangeBehavior too_close_behavior_{OutOfRangeBehavior::NAN_OUT};
  bool debug_raw_{false};

  // Frame assembly
  uint8_t buf_[4]{};
  uint8_t buf_len_{0};

  // Filter state (mm)
  float last_accepted_mm_{NAN};  // reference for the step limiter and HOLD_LAST
  float last_candidate_mm_{NAN};
  uint8_t streak_{0};
  float step_candidate_mm_{NAN};  // a sustained jump beyond max_step re-baselines after N frames
  uint8_t step_streak_{0};

  // Per-interval results, consumed by update()
  FrameOutcome outcome_{FrameOutcome::NONE};
  uint16_t latest_raw_mm_{0};
  uint32_t frames_{0};
  uint32_t checksum_errors_{0};
  uint32_t last_update_ms_{0};
  uint32_t last_frame_ms_{0};

  sensor::Sensor *frame_rate_sensor_{nullptr};
  text_sensor::TextSensor *status_sensor_{nullptr};
  sensor::Sensor *raw_mm_sensor_{nullptr};
  RawPolicy raw_policy_{RawPolicy::ALL};
  const char *last_status_{nullptr};
};

}  // namespace me007ys
}  // namespace esphome
