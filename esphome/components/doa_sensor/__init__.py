import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

DEPENDENCIES = ["satellite1"]
CODEOWNERS = ["@gnumpi"]

CONF_MODE_SENSOR = "mode_sensor"
CONF_ANGLE_SENSOR = "angle_sensor"
CONF_CONFIDENCE_SENSOR = "confidence_sensor"

DOASensor = cg.esphome_ns.namespace_("DOASensor")

CONFIG_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.declare_id(DOASensor),
    cv.Optional(CONF_MODE_SENSOR): cv.use_id(cg.uint8),
    cv.Optional(CONF_ANGLE_SENSOR): cv.use_id(cg.uint8),
    cv.Optional(CONF_CONFIDENCE_SENSOR): cv.use_id(cg.uint8),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Register sensors
    from esphome.components import sensor, text_sensor
    from esphome.components.satellite1 import doa_sensor

    sensor_var = await sensor.new_sensor(config[CONF_ANGLE_SENSOR])
    cg.add(var.register_sensor(sensor_var))

    conf_sensor_var = await sensor.new_sensor(config[CONF_CONFIDENCE_SENSOR])
    cg.add(var.register_confidence_sensor(conf_sensor_var))

    mode_sensor_var = await text_sensor.new_text_sensor(config[CONF_MODE_SENSOR])
    cg.add(var.register_mode_sensor(mode_sensor))
