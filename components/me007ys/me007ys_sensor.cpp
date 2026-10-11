#include "me007ys_sensor.h"
#include <algorithm>
#include <cstring>
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace me007ys {

static const char *const TAG = "me007ys";

// Frames within this distance of the previous one count as "the same reading".
static const float SIM_TOL_MM = 10.0f;
// A sustained jump beyond max_step_cm is accepted after at least this many agreeing frames.
static const uint8_t MIN_STEP_CONFIRM = 3;
// The sensor's frame period (~500 ms observed) can match update_interval, so an interval
// with no frame is normal; only report idle after this long without any frame.
static const uint32_t IDLE_TIMEOUT_MS = 3000;

static uint8_t frame_checksum(const uint8_t *f) { return static_cast<uint8_t>(f[0] + f[1] + f[2]); }

void ME007YSSensor::setup() {
  this->last_update_ms_ = millis();
  this->last_frame_ms_ = this->last_update_ms_;
}

void ME007YSSensor::loop() {
  // Frame: [0xFF][HIGH][LOW][SUM], SUM = (0xFF + HIGH + LOW) & 0xFF
  uint8_t byte;
  while (this->available() > 0 && this->read_byte(&byte)) {
    if (this->buf_len_ == 0 && byte != 0xFF)
      continue;
    this->buf_[this->buf_len_++] = byte;
    if (this->buf_len_ < 4)
      continue;

    if (this->debug_raw_) {
      ESP_LOGD(TAG, "Frame: %02X %02X %02X %02X (calc %02X)", this->buf_[0], this->buf_[1], this->buf_[2],
               this->buf_[3], frame_checksum(this->buf_));
    }

    // HIGH can never be 0xFF (max range 4500 mm = 0x1194). A window starting on a 0xFF data
    // byte can still pass the checksum (e.g. 511 mm = FF 01 FF FF), so treat it as misaligned.
    if (this->buf_[1] != 0xFF && this->buf_[3] == frame_checksum(this->buf_)) {
      this->handle_frame_((static_cast<uint16_t>(this->buf_[1]) << 8) | this->buf_[2]);
      this->buf_len_ = 0;
      continue;
    }

    // Bad frame: the 0xFF we synced on may have been a data byte. Resync on the next 0xFF
    // inside the buffer instead of discarding all four bytes, so a real frame is not lost.
    this->checksum_errors_++;
    uint8_t keep = 0;
    for (uint8_t i = 1; i < 4; i++) {
      if (this->buf_[i] == 0xFF) {
        keep = 4 - i;
        memmove(this->buf_, this->buf_ + i, keep);
        break;
      }
    }
    this->buf_len_ = keep;
  }
}

void ME007YSSensor::handle_frame_(uint16_t distance_mm) {
  this->frames_++;
  this->last_frame_ms_ = millis();
  this->latest_raw_mm_ = distance_mm;
  const float d = distance_mm;

  if (d < this->min_valid_mm_ + this->guard_mm_ || d >= this->max_valid_mm_) {
    this->outcome_ = (d >= this->max_valid_mm_) ? FrameOutcome::TOO_FAR : FrameOutcome::TOO_CLOSE;
    this->last_candidate_mm_ = NAN;
    this->streak_ = 0;
    this->step_candidate_mm_ = NAN;
    this->step_streak_ = 0;
    return;
  }

  bool rebaselined = false;
  if (this->max_step_mm_ > 0.0f && !std::isnan(this->last_accepted_mm_) &&
      fabsf(d - this->last_accepted_mm_) > this->max_step_mm_) {
    // Compare against the jump candidate, not only the stale reference, so a real level
    // change is eventually accepted instead of being rejected until reboot.
    if (!std::isnan(this->step_candidate_mm_) && fabsf(d - this->step_candidate_mm_) <= SIM_TOL_MM) {
      if (this->step_streak_ < 255)
        this->step_streak_++;
    } else {
      this->step_streak_ = 1;
    }
    this->step_candidate_mm_ = d;

    const uint8_t needed = std::max(this->require_consecutive_, MIN_STEP_CONFIRM);
    if (this->step_streak_ < needed) {
      if (this->outcome_ != FrameOutcome::OK)
        this->outcome_ = FrameOutcome::PENDING;
      this->last_candidate_mm_ = NAN;
      this->streak_ = 0;
      return;
    }
    rebaselined = true;
  }
  this->step_candidate_mm_ = NAN;
  this->step_streak_ = 0;

  if (rebaselined) {
    // The step-confirm frames already agreed with each other; no second confirmation round.
    this->streak_ = this->require_consecutive_;
  } else if (std::isnan(this->last_candidate_mm_) || fabsf(d - this->last_candidate_mm_) <= SIM_TOL_MM) {
    if (this->streak_ < 255)
      this->streak_++;
  } else {
    this->streak_ = 1;
  }
  this->last_candidate_mm_ = d;

  // The streak is not reset on accept: once N frames agree, every further agreeing frame
  // publishes, instead of one publish per N frames.
  if (this->streak_ < this->require_consecutive_) {
    if (this->outcome_ != FrameOutcome::OK)
      this->outcome_ = FrameOutcome::PENDING;
    return;
  }

  this->last_accepted_mm_ = d;
  this->outcome_ = FrameOutcome::OK;
}

void ME007YSSensor::update() {
  const uint32_t now = millis();
  const uint32_t elapsed_ms = now - this->last_update_ms_;
  this->last_update_ms_ = now;

  if (this->frame_rate_sensor_ != nullptr && elapsed_ms > 0)
    this->frame_rate_sensor_->publish_state(this->frames_ * 1000.0f / elapsed_ms);

  if (this->checksum_errors_ > 0)
    ESP_LOGW(TAG, "%u frame(s) failed checksum since last update", (unsigned) this->checksum_errors_);

  const bool raw_all = this->raw_policy_ == RawPolicy::ALL;
  switch (this->outcome_) {
    case FrameOutcome::NONE:
      // Keep the last distance; a disconnected sensor shows up in status, not as a fake value.
      if (this->checksum_errors_ > 0)
        this->publish_status_("checksum_error");
      else if (now - this->last_frame_ms_ >= std::max(IDLE_TIMEOUT_MS, 2 * this->get_update_interval()))
        this->publish_status_("idle");
      break;

    case FrameOutcome::OK:
      this->publish_state(this->last_accepted_mm_ / 10.0f);
      this->publish_status_("ok");
      if (this->raw_mm_sensor_ != nullptr)
        this->raw_mm_sensor_->publish_state(raw_all ? this->latest_raw_mm_ : this->last_accepted_mm_);
      break;

    case FrameOutcome::PENDING:
      // In range but not confirmed yet (require_consecutive / max_step_cm): no new distance.
      this->publish_status_("settling");
      if (this->raw_mm_sensor_ != nullptr && raw_all)
        this->raw_mm_sensor_->publish_state(this->latest_raw_mm_);
      break;

    case FrameOutcome::TOO_CLOSE:
      switch (this->too_close_behavior_) {
        case OutOfRangeBehavior::PUBLISH_LIMIT:
          this->publish_state(this->min_valid_mm_ / 10.0f);
          break;
        case OutOfRangeBehavior::HOLD_LAST:
          this->publish_state(std::isnan(this->last_accepted_mm_) ? NAN : this->last_accepted_mm_ / 10.0f);
          break;
        case OutOfRangeBehavior::NAN_OUT:
        default:
          this->publish_state(NAN);
          break;
      }
      this->publish_status_("too_close");
      if (this->raw_mm_sensor_ != nullptr)
        this->raw_mm_sensor_->publish_state(raw_all ? this->latest_raw_mm_ : NAN);
      break;

    case FrameOutcome::TOO_FAR:
      this->publish_state(NAN);
      this->publish_status_("too_far");
      if (this->raw_mm_sensor_ != nullptr)
        this->raw_mm_sensor_->publish_state(raw_all ? this->latest_raw_mm_ : NAN);
      break;
  }

  this->outcome_ = FrameOutcome::NONE;
  this->frames_ = 0;
  this->checksum_errors_ = 0;
}

void ME007YSSensor::publish_status_(const char *s) {
  // Text sensors do not dedup, so only publish on change.
  if (this->status_sensor_ == nullptr || (this->last_status_ != nullptr && strcmp(this->last_status_, s) == 0))
    return;
  this->last_status_ = s;
  this->status_sensor_->publish_state(s);
}

void ME007YSSensor::dump_config() {
  LOG_SENSOR("", "ME007YS", this);
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG,
                "  Valid range: %.1f - %.1f cm (guard %.1f cm)\n"
                "  Max step: %.1f cm, require consecutive: %u",
                this->min_valid_mm_ / 10.0f, this->max_valid_mm_ / 10.0f, this->guard_mm_ / 10.0f,
                this->max_step_mm_ / 10.0f, (unsigned) this->require_consecutive_);
  LOG_SENSOR("  ", "Raw distance", this->raw_mm_sensor_);
  LOG_SENSOR("  ", "Frame rate", this->frame_rate_sensor_);
  LOG_TEXT_SENSOR("  ", "Status", this->status_sensor_);
}

}  // namespace me007ys
}  // namespace esphome
