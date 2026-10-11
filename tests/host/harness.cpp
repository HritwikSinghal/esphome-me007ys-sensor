#include "esphome/core/hal.h"
#include "me007ys_sensor.h"
#include <cassert>
#include <cstdio>
#include <string>
using namespace esphome;
using namespace esphome::me007ys;
uint32_t esphome::g_now = 0;
bool esphome_log_on = false;
static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } else printf("ok:   %s\n", msg); } while (0)

struct Rig {
  ME007YSSensor s; sensor::Sensor raw, fr; text_sensor::TextSensor st;
  Rig(float min=28, float guard=0, float step=0, int n=1, const char *policy="all") {
    s.set_min_valid_cm(min); s.set_max_valid_cm(450); s.set_guard_cm(guard); s.set_max_step_cm(step);
    s.set_require_consecutive(n); s.set_raw_mm_sensor(&raw); s.set_frame_rate_sensor(&fr); s.set_status_sensor(&st);
    s.set_raw_mm_policy(policy); s.setup();
  }
  void frame(uint16_t mm) { uint8_t h = mm >> 8, l = mm & 0xFF; for (uint8_t b : {(uint8_t)0xFF, h, l, (uint8_t)(0xFF + h + l)}) s.rx.push_back(b); }
  void bytes(std::initializer_list<uint8_t> bs) { for (auto b : bs) s.rx.push_back(b); }
  void tick() { s.loop(); g_now += 500; s.update(); }
  // one frame per update, the device's real cadence
  void feed(uint16_t mm, int count) { for (int i = 0; i < count; i++) { frame(mm); tick(); } }
  float last() { return s.pubs.empty() ? -1 : (s.pubs.back()); }
  std::string status() { return st.pubs.empty() ? "" : st.pubs.back(); }
};

int main() {
  { // Bug 1: step limiter lockout
    Rig r(28, 12, 120, 3);
    r.feed(1000, 5);
    CHECK(r.last() == 100.0f, "B1 settles at 100.0 cm");
    r.feed(3000, 2);
    CHECK(r.last() == 100.0f && r.status() == "settling", "B1 single 300 cm jump held back, status=settling (not ok)");
    r.feed(3000, 2);
    CHECK(r.last() == 300.0f, "B1 sustained 300 cm accepted after confirmation (was: never)");
    r.feed(3000, 1);
    CHECK(r.last() == 300.0f, "B1 stays at 300 cm");
    Rig q(28, 12, 120, 1);
    q.feed(1000, 3); q.feed(3000, 1); q.feed(1000, 1); q.feed(3000, 1);
    CHECK(q.last() == 100.0f, "B1 alternating spikes still rejected (limiter still works)");
  }
  { // Bug 2: multiple frames per update -> one publish each
    Rig r;
    for (int i = 0; i < 5; i++) r.frame(1234);
    size_t d0 = r.s.pubs.size(), r0 = r.raw.pubs.size();
    r.tick();
    CHECK(r.s.pubs.size() - d0 == 1 && r.raw.pubs.size() - r0 == 1, "B2 5 frames in one interval -> 1 distance + 1 raw publish");
    CHECK(r.fr.pubs.back() == 10.0f, "B2 frame rate = 5 frames / 0.5 s = 10 Hz");
    size_t s0 = r.st.pubs.size();
    r.feed(1234, 5);
    CHECK(r.st.pubs.size() == s0, "B2 unchanged status not re-published");
    r.tick();
    CHECK(r.status() == "ok", "B2 one empty interval does not flap to idle");
    for (int i = 0; i < 6; i++) r.tick();
    CHECK(r.status() == "idle", "B2 no frames for 3 s -> idle");
  }
  { // require_consecutive no longer resets after publish
    Rig r(28, 0, 0, 3);
    size_t before = r.s.pubs.size();
    r.feed(1000, 12);
    CHECK(r.s.pubs.size() - before == 10, "RC stable 12 frames with N=3 -> publishes on frames 3..12 (10), was 4");
    r.feed(1050, 1);
    CHECK(r.status() == "settling", "RC 5 cm jump restarts confirmation");
    r.feed(1050, 2);
    CHECK(r.last() == 105.0f, "RC new level published after N agreeing frames");
  }
  { // Bug 6: long interval, many frames buffered: no overflow in loop(); latest wins
    Rig r;
    for (int i = 0; i < 600; i++) { r.frame(1000 + i); r.s.loop(); }
    g_now += 60000; r.s.update();
    CHECK(r.last() == 159.9f || (r.last() > 159.89f && r.last() < 159.91f), "B6 60 s interval -> publishes latest frame (159.9 cm)");
    CHECK(r.fr.pubs.back() == 10.0f, "B6 frame rate 600/60 s = 10 Hz");
  }
  { // resync: false 0xFF start, garbage, bad checksum
    Rig r;
    r.bytes({0x12, 0xFF});            // garbage then a stray 0xFF
    r.frame(2000);                    // stray FF + FF 07 D0 -> bad checksum, must resync onto real frame
    r.tick();
    CHECK(r.last() == 200.0f, "RS stray 0xFF before a frame -> frame still parsed");
    r.bytes({0xFF, 0x07, 0xD0, 0x00}); r.tick();
    CHECK(r.status() == "checksum_error", "RS only bad frames -> checksum_error");
  }
  { // too close / too far / valid_only
    Rig r(28, 0, 0, 1, "valid_only");
    r.feed(200, 1);
    CHECK(std::isnan(r.last()) && std::isnan(r.raw.pubs.back()) && r.status() == "too_close", "TC 20 cm -> NaN, raw NaN, too_close");
    r.feed(384, 1);
    CHECK(r.last() == 38.4f && r.raw.pubs.back() == 384.0f, "TC 38.4 cm accepted with guard 0 (user's real case)");
    r.feed(4600, 1);
    CHECK(std::isnan(r.last()) && r.status() == "too_far", "TF 460 cm -> NaN, too_far");
    Rig h(28, 12, 0, 1); h.s.set_too_close_behavior("last");
    h.feed(1000, 1); h.feed(384, 1);
    CHECK(h.last() == 100.0f, "TC hold_last keeps 100 cm when 38.4 cm is under min+guard");
  }
  { // Bug 5: large thresholds no longer wrap
    Rig r(28, 0, 0, 1); r.s.set_max_valid_cm(1000); r.feed(4600, 1);
    CHECK(r.last() == 460.0f, "B5 max_valid_cm 1000 (10000 mm) keeps 460 cm valid (uint16 math would also pass; checks float path)");
  }
  printf("\n%s (%d failure(s))\n", fails ? "FAILED" : "ALL PASSED", fails);
  return fails != 0;
}
