#include "esphome/core/hal.h"
#include "me007ys_sensor.h"
#include <cstdio>
using namespace esphome; using namespace esphome::me007ys;
uint32_t esphome::g_now = 0; bool esphome_log_on = false;
int main() {
  for (int dist : {511, 766, 767, 1023, 1279, 1000}) {
    int bad = 0, total = 0;
    for (int drop = 0; drop < 4; drop++) {           // start the stream mid-frame
      ME007YSSensor s; sensor::Sensor raw; s.set_raw_mm_sensor(&raw); s.set_guard_cm(0); s.setup();
      uint8_t h = dist >> 8, l = dist & 0xFF, f[4] = {0xFF, h, l, (uint8_t)(0xFF + h + l)};
      for (int i = drop; i < 4; i++) s.rx.push_back(f[i]);
      for (int k = 0; k < 20; k++) { for (int i = 0; i < 4; i++) s.rx.push_back(f[i]); s.loop(); g_now += 500; s.update(); total++; if (raw.pubs.empty() || raw.pubs.back() != dist) bad++; }
    }
    printf("%5d mm: %d/%d updates with wrong raw value\n", dist, bad, total);
  }
}
