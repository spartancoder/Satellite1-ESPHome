#pragma once

#include "esphome/components/satellite1/satellite1.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
#include "esphome/core/component.h"

namespace esphome {
namespace satellite1 {

class DOASensor : public Satellite1SPIService, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  // Sensor registration
  void set_mode_sensor(text_sensor::TextSensor *sensor) { mode_sensor_ = sensor; }
  void set_angle_sensor(sensor::Sensor *sensor) { angle_sensor_ = sensor; }
  void set_confidence_sensor(sensor::Sensor *sensor) { confidence_sensor_ = sensor; }
  void set_angle_number(number::Number *number) { angle_number_ = number; }
  void set_mode_select(select::Select *select) { mode_select_ = select; }

  // Control methods called from ESPHome number/select
  void set_beam_angle(float angle);
  void set_mvdr_mode(const std::string &mode);

 protected:
  // Data acquisition
  void update_doa_data_();
  bool parse_doa_response_(uint8_t *payload, uint8_t len);

  // Helper to convert mode enum to string
  const char* mode_to_string_(uint8_t mode);
  uint8_t string_to_mode_(const std::string &mode);

  // Change detection
  bool should_publish_angle_(int32_t new_angle);
  bool should_publish_confidence_(float new_confidence);

  // Cached values
  uint8_t current_mode_{0};
  int32_t current_angle_{0};
  float current_confidence_{0.0f};

  // Last published values (for change detection)
  int32_t last_published_angle_{-1};
  float last_published_confidence_{-1.0f};
  uint8_t last_published_mode_{255};

  // Sensor pointers
  text_sensor::TextSensor *mode_sensor_{nullptr};
  sensor::Sensor *angle_sensor_{nullptr};
  sensor::Sensor *confidence_sensor_{nullptr};
  number::Number *angle_number_{nullptr};
  select::Select *mode_select_{nullptr};

  // Timing
  uint32_t last_update_{0};
  static const uint32_t UPDATE_INTERVAL_MS = 100;  // 10 Hz polling
  uint8_t consecutive_failures_{0};
  static const uint8_t MAX_FAILURES = 3;
  static const uint32_t FAILURE_BACKOFF_MS = 5000;
};

}  // namespace satellite1
}  // namespace esphome
