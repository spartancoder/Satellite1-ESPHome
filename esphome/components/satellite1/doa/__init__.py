import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, text_sensor
from esphome.const import (
    CONF_ID,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_SIGNAL,
    UNIT_DEGREES,
    UNIT_PERCENT,
)

from ..satellite1 import (
    Satellite1,
    Satellite1SPIService,
    namespace as satellite1_ns,
)


DEPENDENCIES = ["satellite1"]
CODEOWNERS = ["@gnumpi"]

CONF_MODE_SENSOR = "mode_sensor"
CONF_ANGLE_SENSOR = "angle_sensor"
CONF_CONFIDENCE_SENSOR = "confidence_sensor"

DOASensor = satellite1_ns.class_("DOASensor", Satellite1SPIService, cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.declare_id(DOASensor),
    cv.Optional(CONF_MODE_SENSOR): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_ANGLE_SENSOR): sensor.sensor_schema(
        unit_of_measurement=UNIT_DEGREES,
        icon="mdi:compass",
        accuracy_decimals=0,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    cv.Optional(CONF_CONFIDENCE_SENSOR): sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        icon=ICON_SIGNAL,
        accuracy_decimals=1,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Register sensors
    if CONF_MODE_SENSOR in config:
        sens = await text_sensor.new_text_sensor(config[CONF_MODE_SENSOR])
        cg.add(var.set_mode_sensor(sens))

    if CONF_ANGLE_SENSOR in config:
        sens = await sensor.new_sensor(config[CONF_ANGLE_SENSOR])
        cg.add(var.set_angle_sensor(sens))

    if CONF_CONFIDENCE_SENSOR in config:
        sens = await sensor.new_sensor(config[CONF_CONFIDENCE_SENSOR])
        cg.add(var.set_confidence_sensor(sens))
