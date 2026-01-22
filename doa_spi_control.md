# DOA (Direction of Arrival) SPI Control Interface

## Overview

The MVDR firmware exposes DOA (Direction of Arrival) beamforming data and controls via an SPI-based device control interface. This allows external controllers (e.g., ESP32-S3) to query beam direction and adjust MVDR parameters in real-time.

## Resource ID

**DOA Servicer Resource ID:** `120`

All DOA commands use Resource ID 120 in the device control protocol.

## Command Reference

### Read Commands

#### GET_BEAM_ANGLE (110)
Reads the current beam direction.

**Request:**
- Command ID: 110
- Payload Length: 0

**Response:**
- Payload Length: 4 bytes
- Data: `int32_t angle` - Current beam angle in degrees (0-359)

**Example:**
```c
uint8_t cmd = 110;
device_control_write_resource(120, &cmd, 1, response_buffer, 4);
int32_t angle = *((int32_t*)response_buffer);
```

#### GET_MVDR_MODE (112)
Reads the current MVDR operating mode.

**Request:**
- Command ID: 112
- Payload Length: 0

**Response:**
- Payload Length: 1 byte
- Data: `uint8_t mode`
  - `0` = AUTO (adaptive tracking)
  - `1` = FIXED (fixed direction)
  - `2` = BROADSIDE (omnidirectional)

#### GET_BEAM_CONFIDENCE (114)
Reads the beam confidence metric.

**Request:**
- Command ID: 114
- Payload Length: 0

**Response:**
- Payload Length: 4 bytes
- Data: `float confidence` - Confidence value (0.0 to 100.0)

**Note:** Higher confidence indicates better conditioning of the covariance matrix.

#### GET_DOA_DATA (115)
Reads all DOA data in a single call (optimized).

**Request:**
- Command ID: 115
- Payload Length: 0

**Response:**
- Payload Length: 8 bytes
- Data Format:
  - Offset 0: `uint8_t mode` - MVDR mode
  - Offset 1-4: `int32_t angle` - Beam angle in degrees
  - Offset 5-7: `uint24_t confidence` - Confidence (truncated to 3 bytes)

**Data Layout:**
```
Offset | Size | Type    | Description
-------|------|---------|------------
0      | 1    | uint8_t | Mode (0=AUTO, 1=FIXED, 2=BROAD)
1      | 4    | int32_t | Angle (degrees, 0-359)
5      | 3    | uint24_t | Confidence (0-100, big-endian)
```

### Write Commands

#### SET_BEAM_ANGLE (111)
Sets the beam direction (FIXED mode only).

**Request:**
- Command ID: 111
- Payload Length: 4 bytes
- Data: `int32_t angle` - Target angle in degrees (0-359)

**Response:**
- Payload Length: 0

**Notes:**
- Automatically switches MVDR to FIXED mode
- Angle is normalized to 0-359 range
- Takes effect immediately

**Example:**
```c
int32_t target_angle = 90;  // East
uint8_t payload[4];
memcpy(payload, &target_angle, 4);
device_control_write_resource(120, &cmd, 1, payload, 4, NULL, 0);
```

#### SET_MVDR_MODE (113)
Sets the MVDR operating mode.

**Request:**
- Command ID: 113
- Payload Length: 1 byte
- Data: `uint8_t mode`
  - `0` = AUTO (adaptive tracking with VNR guidance)
  - `1` = FIXED (fixed beam direction)
  - `2` = BROADSIDE (equal weights, omnidirectional)

**Response:**
- Payload Length: 0

**Mode Descriptions:**

**AUTO Mode:**
- Automatically tracks voice source
- Uses VNR (Voice-to-Noise Ratio) to detect speech
- Updates beam direction based on cross-correlation DOA estimation
- Falls back to broadside after silence timeout (5 seconds)

**FIXED Mode:**
- Beam locked to specific angle
- Angle set via SET_BEAM_ANGLE command
- Useful for known speaker position

**BROADSIDE Mode:**
- Equal weighting of all microphones
- Omnidirectional pattern
- Used as fallback when no voice detected

## Data Types

### MVDR Mode Enum
```c
typedef enum {
    DOA_MVDR_MODE_AUTO = 0,       // Adaptive tracking
    DOA_MVDR_MODE_FIXED = 1,      // Fixed direction
    DOA_MVDR_MODE_BROADSIDE = 2   // Omnidirectional
} doa_mvdr_mode_t;
```

### DOA Data Structure
```c
typedef struct {
    doa_mvdr_mode_t mode;         // Current MVDR mode
    float angle;                  // Current beam angle (degrees)
    float confidence;             // Beam confidence (0-100)
} __attribute__((packed)) doa_data_t;
```

## Usage Examples

### Example 1: Query Current Beam Direction (ESP32-S3)

```cpp
#include "device_control.h"

void get_beam_direction() {
    uint8_t cmd = 110;  // GET_BEAM_ANGLE
    uint8_t response[4];

    int ret = device_control_write_resource(
        120,              // Resource ID
        &cmd, 1,          // Command
        response, 4       // Response buffer
    );

    if (ret == 0) {
        int32_t angle = *((int32_t*)response);
        Serial.printf("Beam direction: %d degrees\n", angle);
    }
}
```

### Example 2: Get Full DOA Data

```cpp
void get_doa_data() {
    uint8_t cmd = 115;  // GET_DOA_DATA
    uint8_t response[8];

    int ret = device_control_write_resource(120, &cmd, 1, response, 8);

    if (ret == 0) {
        uint8_t mode = response[0];
        int32_t angle = *((int32_t*)&response[1]);

        // Unpack confidence (3 bytes, big-endian)
        uint32_t confidence = (response[5] | (response[6] << 8) | (response[7] << 16));

        const char* mode_str = (mode == 0) ? "AUTO" :
                               (mode == 1) ? "FIXED" : "BROAD";

        Serial.printf("DOA: mode=%s, angle=%d, conf=%u\n",
                     mode_str, angle, confidence);
    }
}
```

### Example 3: Set Fixed Beam Direction

```cpp
void set_beam_direction(int32_t angle) {
    uint8_t cmd = 111;  // SET_BEAM_ANGLE
    uint8_t payload[4];

    memcpy(payload, &angle, 4);

    int ret = device_control_write_resource(
        120,              // Resource ID
        &cmd, 1,          // Command
        payload, 4,       // Payload
        NULL, 0           // No response
    );

    if (ret == 0) {
        Serial.printf("Beam set to %d degrees\n", angle);
    }
}
```

### Example 4: Switch to Auto Tracking

```cpp
void enable_auto_tracking() {
    uint8_t cmd = 113;  // SET_MVDR_MODE
    uint8_t payload[1] = {0};  // AUTO mode

    int ret = device_control_write_resource(
        120, &cmd, 1, payload, 1, NULL, 0
    );

    if (ret == 0) {
        Serial.println("Auto tracking enabled");
    }
}
```

## Performance Considerations

### Query Frequency
- **Recommended:** 10-50 Hz (every 20-100ms)
- **Maximum:** 100 Hz (every 10ms)
- Higher frequencies may interfere with audio processing

### Data Freshness
- DOA data is updated each audio frame (240 samples @ 16kHz = 15ms)
- Querying faster than frame rate returns cached data

### SPI Configuration
- **Clock Frequency:** Up to 10 MHz
- **Mode:** 0 (CPOL=0, CPHA=0)
- **Bit Order:** MSB first

## Integration Notes

### With ESP32-S3

The ESP32-S3 should:
1. Poll DOA data at 10-20 Hz for UI updates
2. Expose DOA data via:
   - MQTT topics (home assistant integration)
   - REST API (mobile apps)
   - WebSocket (real-time web UI)

### Example MQTT Integration

```cpp
void loop() {
    static unsigned long last_update = 0;

    if (millis() - last_update > 100) {  // 10 Hz
        uint8_t cmd = 115;  // GET_DOA_DATA
        uint8_t response[8];

        if (device_control_write_resource(120, &cmd, 1, response, 8) == 0) {
            int32_t angle = *((int32_t*)&response[1]);

            // Publish to MQTT
            char msg[32];
            snprintf(msg, sizeof(msg), "{\"angle\":%d}", angle);
            mqtt_client.publish("satellite1/doa", msg);

            last_update = millis();
        }
    }
}
```

## Error Handling

### Return Codes
- `0` (CONTROL_SUCCESS) - Command executed successfully
- `!0` (CONTROL_ERROR) - Error occurred

### Common Errors
- **MVDR state not set** - Firmware not initialized properly
- **Bad command** - Invalid command ID
- **Resource unavailable** - Device control not running

## Debugging

### Enable Debug Output

Set in `doa_servicer.c`:
```c
#define DEBUG_PRINT_ENABLE_DOA_SERVICER 1
```

Debug messages will appear on XMOS debug output (xscope or UART).

## Firmware Version

- **Minimum Required:** v1.2.3-dev.1 (MVDR support)
- **Resource ID Added:** v1.2.4 (DOA servicer)

## Related Documentation

- [MVDR Beamforming Overview](./mvdr_beamforming.md)
- [SPI Device Control Protocol](./spi_device_control.md)
- [Firmware Build Instructions](./build_notes.md)
