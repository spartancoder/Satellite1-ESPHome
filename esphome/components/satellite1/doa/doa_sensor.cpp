#include "doa_sensor.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <ctype.h>
#include <cstring>

namespace esphome {
namespace satellite1 {

static const char *TAG = "DOASensor";
static const uint8_t DOA_RESPONSE_LENGTH = 8;

void DOASensor::setup() {
  ESP_LOGI(TAG, "Setting up DOA sensor");

  // Check if XMOS is connected and firmware supports DOA
  if (this->parent_->state != SAT_XMOS_CONNECTED_STATE) {
    ESP_LOGW(TAG, "XMOS not connected, DOA will initialize when connected");
  }

  ESP_LOGI(TAG, "DOA sensor setup complete");
}

void DOASensor::loop() {
  uint32_t now = millis();

  // Check if it's time to update
  if (now - this->last_update_ >= this->UPDATE_INTERVAL_MS) {
    // Only update if XMOS is connected
    if (this->parent_->state == SAT_XMOS_CONNECTED_STATE) {
      this->update_doa_data_();
    } else {
      // Reset failure counter when disconnected
      this->consecutive_failures_ = 0;
    }
    this->last_update_ = now;
  }
}

void DOASensor::update_doa_data_() {
  uint8_t response[DOA_RESPONSE_LENGTH];

  if (!this->parent_->transfer(DC_RESOURCE::DOA_SERVICER,
                                DC_DOA_CMD::GET_DOA_DATA,
                                response, DOA_RESPONSE_LENGTH)) {
    this->consecutive_failures_++;

    if (this->consecutive_failures_ >= this->MAX_FAILURES) {
      ESP_LOGW(TAG, "Multiple DOA read failures, backing off for %lu ms",
               this->FAILURE_BACKOFF_MS);
      this->last_update_ = millis() - this->UPDATE_INTERVAL_MS + this->FAILURE_BACKOFF_MS;
      this->consecutive_failures_ = 0;
    }
    return;
  }

  // Reset failure counter on success
  this->consecutive_failures_ = 0;

  if (!this->parse_doa_response_(response, DOA_RESPONSE_LENGTH)) {
    ESP_LOGW(TAG, "Failed to parse DOA response");
    return;
  }

  // Publish mode if changed
  if (this->current_mode_ != this->last_published_mode_) {
    if (this->mode_sensor_ != nullptr) {
      this->mode_sensor_->publish_state(this->mode_to_string_(this->current_mode_));
    }
    this->last_published_mode_ = this->current_mode_;
  }

  // Publish angle if changed significantly
  if (this->should_publish_angle_(this->current_angle_)) {
    if (this->angle_sensor_ != nullptr) {
      this->angle_sensor_->publish_state(this->current_angle_);
    }
    this->last_published_angle_ = this->current_angle_;
  }

  // Publish confidence if changed significantly
  if (this->should_publish_confidence_(this->current_confidence_)) {
    if (this->confidence_sensor_ != nullptr) {
      this->confidence_sensor_->publish_state(this->current_confidence_);
    }
    this->last_published_confidence_ = this->current_confidence_;
  }
}

bool DOASensor::parse_doa_response_(uint8_t *payload, uint8_t len) {
  if (len != DOA_RESPONSE_LENGTH) {
    ESP_LOGW(TAG, "Invalid DOA response length: %d (expected %d)", len, DOA_RESPONSE_LENGTH);
    return false;
  }

  // Parse mode (offset 0)
  this->current_mode_ = payload[0];
  if (this->current_mode_ > 2) {
    ESP_LOGW(TAG, "Invalid mode value: %d", this->current_mode_);
    return false;
  }

  // Parse angle (offset 1-4, int32_t) - use memcpy to avoid alignment issues
  int32_t angle;
  std::memcpy(&angle, &payload[1], sizeof(angle));
  this->current_angle_ = angle;

  // Normalize angle to 0-359 using modulo (efficient for extreme values)
  this->current_angle_ = ((this->current_angle_ % 360) + 360) % 360;

  // Parse confidence (offset 5-7, uint24_t big-endian)
  uint32_t conf = (payload[5] | (payload[6] << 8) | (payload[7] << 16));
  this->current_confidence_ = static_cast<float>(conf);

  // Clamp confidence to 0-100
  if (this->current_confidence_ > 100.0f) {
    this->current_confidence_ = 100.0f;
  }

  ESP_LOGD(TAG, "DOA: mode=%d (%s), angle=%ld, confidence=%.1f",
           this->current_mode_, this->mode_to_string_(this->current_mode_),
           this->current_angle_, this->current_confidence_);

  return true;
}

const char* DOASensor::mode_to_string_(uint8_t mode) {
  switch (mode) {
    case 0: return "AUTO";
    case 1: return "FIXED";
    case 2: return "BROAD";
    default: return "UNKNOWN";
  }
}

uint8_t DOASensor::string_to_mode_(const std::string &mode) {
  // Case-insensitive comparison
  std::string upper_mode = mode;
  std::transform(upper_mode.begin(), upper_mode.end(), upper_mode.begin(), ::toupper);

  if (upper_mode == "AUTO") return 0;
  if (upper_mode == "FIXED") return 1;
  if (upper_mode == "BROAD") return 2;
  ESP_LOGW(TAG, "Unknown mode string: %s", mode.c_str());
  return 0;  // Default to AUTO
}

bool DOASensor::should_publish_angle_(int32_t new_angle) {
  // Publish on first read or significant change (>2 degrees)
  if (this->last_published_angle_ < 0) return true;

  // Handle wrap-around at 360/0
  int32_t diff = abs(new_angle - this->last_published_angle_);
  if (diff > 180) {
    diff = 360 - diff;
  }

  return diff > 2;
}

bool DOASensor::should_publish_confidence_(float new_confidence) {
  // Publish on first read or significant change (>5)
  if (this->last_published_confidence_ < 0) return true;
  return fabs(new_confidence - this->last_published_confidence_) > 5.0f;
}

void DOASensor::set_beam_angle(float angle) {
  int32_t angle_int = static_cast<int32_t>(angle);

  // Normalize to 0-359 using modulo
  angle_int = ((angle_int % 360) + 360) % 360;

  ESP_LOGI(TAG, "Setting beam angle to %ld degrees", angle_int);

  uint8_t payload[4];
  std::memcpy(payload, &angle_int, 4);

  if (!this->parent_->transfer(DC_RESOURCE::DOA_SERVICER,
                                DC_DOA_CMD::SET_BEAM_ANGLE,
                                payload, 4)) {
    ESP_LOGW(TAG, "Failed to set beam angle");
    return;
  }

  ESP_LOGI(TAG, "Beam angle set successfully (mode switched to FIXED)");

  // Force update immediately to reflect change
  this->update_doa_data_();
}

void DOASensor::set_mvdr_mode(const std::string &mode) {
  uint8_t mode_val = this->string_to_mode_(mode);

  ESP_LOGI(TAG, "Setting MVDR mode to %s (%d)", mode.c_str(), mode_val);

  uint8_t payload[1] = {mode_val};

  if (!this->parent_->transfer(DC_RESOURCE::DOA_SERVICER,
                                DC_DOA_CMD::SET_MVDR_MODE,
                                payload, 1)) {
    ESP_LOGW(TAG, "Failed to set MVDR mode");
    return;
  }

  ESP_LOGI(TAG, "MVDR mode set successfully");

  // Force update immediately to reflect change
  this->update_doa_data_();
}

void DOASensor::dump_config() {
  ESP_LOGCONFIG(TAG, "DOA Sensor:");
  ESP_LOGCONFIG(TAG, "  Update interval: %lu ms", this->UPDATE_INTERVAL_MS);
  ESP_LOGCONFIG(TAG, "  Mode sensor: %s", this->mode_sensor_ ? "configured" : "not configured");
  ESP_LOGCONFIG(TAG, "  Angle sensor: %s", this->angle_sensor_ ? "configured" : "not configured");
  ESP_LOGCONFIG(TAG, "  Confidence sensor: %s", this->confidence_sensor_ ? "configured" : "not configured");
  ESP_LOGCONFIG(TAG, "  Angle number: %s", this->angle_number_ ? "configured" : "not configured");
  ESP_LOGCONFIG(TAG, "  Mode select: %s", this->mode_select_ ? "configured" : "not configured");
}

}  // namespace satellite1
}  // namespace esphome
