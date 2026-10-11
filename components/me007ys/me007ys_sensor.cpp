#include "me007ys_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace me007ys {

static const char *const TAG = "me007ys";

void ME007YSSensor::update() {
  // Parse frames: [0xFF][HIGH][LOW][SUM]
  uint32_t valid_frames = 0;
  bool any_bytes = false;

  while (this->available() >= 4) {
    any_bytes = true;

    uint8_t start = this->read();
    if (this->debug_raw_) ESP_LOGVV(TAG, "RX start: 0x%02X", start);
    if (start != 0xFF) continue;  // resync

    int h = this->read();
    int l = this->read();
    int s = this->read();
    if (h < 0 || l < 0 || s < 0) break;

    uint8_t high = static_cast<uint8_t>(h);
    uint8_t low  = static_cast<uint8_t>(l);
    uint8_t sum  = static_cast<uint8_t>(s);
    uint8_t calc_sum = static_cast<uint8_t>((0xFF + high + low) & 0xFF);

    if (this->debug_raw_) ESP_LOGVV(TAG, "FRAME: ff %02x %02x %02x (calc=%02x)", high, low, sum, calc_sum);

    if (sum != calc_sum) {
      ESP_LOGW(TAG, "Checksum mismatch: got 0x%02X, expected 0x%02X", sum, calc_sum);
      this->publish_status_("checksum_error");
      continue;
    }

    const uint16_t distance_mm = (static_cast<uint16_t>(high) << 8) | low;

    // raw_mm: ALL -> publish now, VALID_ONLY -> publish later if accepted
    if (this->raw_mm_sensor_ != nullptr && this->raw_policy_ == RawPolicy::ALL) {
      this->raw_mm_sensor_->publish_state(distance_mm);
    }

    // Lower bound + guard band
    const uint16_t guard_min_mm = static_cast<uint16_t>(this->min_valid_mm_ + this->guard_mm_);
    if (distance_mm <= this->min_valid_mm_ || distance_mm < guard_min_mm) {
      if (this->raw_mm_sensor_ != nullptr && this->raw_policy_ == RawPolicy::VALID_ONLY) {
        this->raw_mm_sensor_->publish_state(NAN);
      }

      switch (this->too_close_behavior_) {
        case OutOfRangeBehavior::PUBLISH_LIMIT: {
          const float cm = this->min_valid_mm_ / 10.0f;
          this->publish_state(cm);
          this->last_valid_cm_ = cm;
          break;
        }
        case OutOfRangeBehavior::HOLD_LAST: {
          if (!std::isnan(this->last_valid_cm_)) this->publish_state(this->last_valid_cm_);
          else this->publish_state(NAN);
          break;
        }
        case OutOfRangeBehavior::NAN_OUT:
        default:
          this->publish_state(NAN);
          break;
      }
      this->last_candidate_cm_ = NAN;
      this->streak_ = 0;
      this->publish_status_("too_close");
      valid_frames++;
      continue;
    }

    // Upper bound: always NaN + too_far
    if (distance_mm >= this->max_valid_mm_) {
      if (this->raw_mm_sensor_ != nullptr && this->raw_policy_ == RawPolicy::VALID_ONLY) {
        this->raw_mm_sensor_->publish_state(NAN);
      }
      this->publish_state(NAN);
      this->last_candidate_cm_ = NAN;
      this->streak_ = 0;
      this->publish_status_("too_far");
      valid_frames++;
      continue;
    }

    // Candidate valid reading
    const float cm = distance_mm / 10.0f;

    // Step limiter (vs last published)
    if (this->max_step_mm_ > 0 && !std::isnan(this->last_published_cm_)) {
      float step_cm = fabsf(cm - this->last_published_cm_);
      if (step_cm * 10.0f > this->max_step_mm_) {
        this->last_candidate_cm_ = NAN;
        this->streak_ = 0;
        valid_frames++;
        continue;
      }
    }

    // Consecutive confirmation (within 1.0 cm similarity)
    const float SIM_TOL_CM = 1.0f;
    if (std::isnan(this->last_candidate_cm_) || fabsf(cm - this->last_candidate_cm_) <= SIM_TOL_CM) {
      if (this->streak_ < 255) this->streak_++;
    } else {
      this->streak_ = 1;
    }
    this->last_candidate_cm_ = cm;

    if (this->streak_ < this->require_consecutive_) {
      valid_frames++;
      continue;
    }

    // Accept and publish
    this->publish_state(cm);
    this->last_valid_cm_ = cm;
    this->last_published_cm_ = cm;
    this->streak_ = 0;
    this->last_candidate_cm_ = NAN;
    this->publish_status_("ok");

    // raw_mm: VALID_ONLY -> publish now (accepted in-range mm)
    if (this->raw_mm_sensor_ != nullptr && this->raw_policy_ == RawPolicy::VALID_ONLY) {
      this->raw_mm_sensor_->publish_state(distance_mm);
    }

    valid_frames++;
  }

  // Frame rate over polling window (optional)
  if (this->frame_rate_sensor_ != nullptr) {
    float dt_s = this->get_update_interval() / 1000.0f;
    if (dt_s <= 0.0f) dt_s = 0.5f;
    float rate_hz = valid_frames / dt_s;
    this->frame_rate_sensor_->publish_state(rate_hz);
  }

  if (!any_bytes) this->publish_status_("idle");
}

}  // namespace me007ys
}  // namespace esphome
