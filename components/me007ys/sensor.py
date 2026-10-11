import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, sensor, text_sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_DISTANCE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    ICON_ARROW_EXPAND_VERTICAL,
    UNIT_CENTIMETER,
)

CODEOWNERS = ["@HritwikSinghal"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "text_sensor"]

me007ys_ns = cg.esphome_ns.namespace("me007ys")
ME007YSSensor = me007ys_ns.class_("ME007YSSensor", sensor.Sensor, cg.PollingComponent, uart.UARTDevice)

# Options
CONF_MIN_VALID_CM = "min_valid_cm"
CONF_MAX_VALID_CM = "max_valid_cm"
CONF_TOO_CLOSE_BEHAVIOR = "too_close_behavior"   # nan|min|last
CONF_DEBUG_RAW = "debug_raw"

# Robustness
CONF_GUARD_CM = "guard_cm"
CONF_MAX_STEP_CM = "max_step_cm"                 # 0 = off
CONF_REQUIRE_CONSECUTIVE = "require_consecutive"

# Diagnostics
CONF_FRAME_RATE_HZ = "frame_rate_hz"
CONF_STATUS = "status"
CONF_RAW_MM = "raw_mm"
CONF_RAW_MM_POLICY = "raw_mm_policy"             # all|valid_only


def _validate_range(config):
    lower = config[CONF_MIN_VALID_CM] + config[CONF_GUARD_CM]
    if lower >= config[CONF_MAX_VALID_CM]:
        raise cv.Invalid(
            f"{CONF_MIN_VALID_CM} + {CONF_GUARD_CM} ({lower}) must be below {CONF_MAX_VALID_CM}"
        )
    return config


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        unit_of_measurement=UNIT_CENTIMETER,
        icon=ICON_ARROW_EXPAND_VERTICAL,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_DISTANCE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(ME007YSSensor),

            cv.Optional(CONF_MIN_VALID_CM, default=28.0): cv.float_range(min=0.0, max=1000.0),
            cv.Optional(CONF_MAX_VALID_CM, default=450.0): cv.float_range(min=0.0, max=1000.0),

            cv.Optional(CONF_TOO_CLOSE_BEHAVIOR, default="nan"): cv.one_of("nan", "min", "last", lower=True),
            cv.Optional(CONF_DEBUG_RAW, default=False): cv.boolean,

            cv.Optional(CONF_GUARD_CM, default=5.0): cv.float_range(min=0.0, max=100.0),
            cv.Optional(CONF_MAX_STEP_CM, default=0.0): cv.float_range(min=0.0, max=1000.0),
            cv.Optional(CONF_REQUIRE_CONSECUTIVE, default=1): cv.int_range(min=1, max=10),

            cv.Optional(CONF_FRAME_RATE_HZ): sensor.sensor_schema(
                unit_of_measurement="Hz",
                accuracy_decimals=2,
                icon="mdi:sine-wave",
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_STATUS): text_sensor.text_sensor_schema(
                icon="mdi:information-outline", entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ),
            cv.Optional(CONF_RAW_MM): sensor.sensor_schema(
                unit_of_measurement="mm",
                accuracy_decimals=0,
                icon="mdi:ruler",
                device_class=DEVICE_CLASS_DISTANCE,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_RAW_MM_POLICY, default="all"): cv.one_of("all", "valid_only", lower=True),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.polling_component_schema("500ms")),
    _validate_range,
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "me007ys", baud_rate=9600, require_rx=True, data_bits=8, parity=None, stop_bits=1
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await uart.register_uart_device(var, config)
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    cg.add(var.set_min_valid_cm(config[CONF_MIN_VALID_CM]))
    cg.add(var.set_max_valid_cm(config[CONF_MAX_VALID_CM]))
    cg.add(var.set_too_close_behavior(config[CONF_TOO_CLOSE_BEHAVIOR]))
    cg.add(var.set_debug_raw(config[CONF_DEBUG_RAW]))

    cg.add(var.set_guard_cm(config[CONF_GUARD_CM]))
    cg.add(var.set_max_step_cm(config[CONF_MAX_STEP_CM]))
    cg.add(var.set_require_consecutive(config[CONF_REQUIRE_CONSECUTIVE]))

    if CONF_FRAME_RATE_HZ in config:
        fr = await sensor.new_sensor(config[CONF_FRAME_RATE_HZ])
        cg.add(var.set_frame_rate_sensor(fr))
    if CONF_STATUS in config:
        st = await text_sensor.new_text_sensor(config[CONF_STATUS])
        cg.add(var.set_status_sensor(st))
    if CONF_RAW_MM in config:
        raw = await sensor.new_sensor(config[CONF_RAW_MM])
        cg.add(var.set_raw_mm_sensor(raw))
        cg.add(var.set_raw_mm_policy(config[CONF_RAW_MM_POLICY]))
