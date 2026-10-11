
# Changelog

## v1.3.0 -- unreleased
### Fixed
- `max_step_cm` no longer freezes the distance after a real level change: a jump that persists for `max(require_consecutive, 3)` agreeing frames becomes the new level.
- One publish per `update_interval` instead of one per frame; `status` only publishes on change.
- `require_consecutive` no longer resets after each publish (stable readings were published only every N frames).
- `debug_raw` now logs at DEBUG; it was compiled out unless the logger was VERY_VERBOSE.
- Thresholds stored as float; `min_valid_cm`/`max_valid_cm` above ~6553 cm no longer overflow uint16 storage.
- Frames whose HIGH byte is 0xFF are treated as misaligned; distances with a 0xFF LOW byte (e.g. 511 mm) could lock onto a wrong frame boundary and read as 65281 mm.
- UART is parsed in `loop()`, so a long `update_interval` can no longer overflow the RX buffer; a bad checksum resyncs on the next 0xFF instead of dropping four bytes.
- `text_sensor` is auto-loaded (build failed without another text sensor in the config).

### Added
- `settling` status while an in-range reading awaits confirmation.
- Config validation: `min_valid_cm + guard_cm < max_valid_cm`, UART must be 9600 baud with an RX pin.
- `dump_config()` output; diagnostic sub-sensors default to `entity_category: diagnostic`.

### Changed
- Component moved to `components/me007ys/` (ESPHome external component layout).
- `min_valid_cm`/`max_valid_cm` capped at 1000 cm (sensor range is 28-450 cm).

## v1.2.0 — 2025-08-11
### Added
- `raw_mm` diagnostic sensor to expose raw millimeter frames after checksum verification.
- `raw_mm_policy` option:
  - `all` — publish every valid-checksum frame (default)
  - `valid_only` — publish only when in-range; NaN on too_close/too_far
- Robustness controls:
  - `guard_cm` — ignore band above `min_valid_cm`
  - `max_step_cm` — drop unrealistic single-frame jumps
  - `require_consecutive` — require N similar frames before publish

### Changed
- Upper bound handling simplified: distance ≥ `max_valid_cm` → publish NaN + `too_far` (no `too_far_behavior`).
- README updated with synced inches example (Option A).
