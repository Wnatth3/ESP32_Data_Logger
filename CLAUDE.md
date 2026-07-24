# CLAUDE.md

This file provides guidance to Claude Code when working with this ESP32 IoT data logger project.

## Common Commands
1. **Build**: `/esp32-build` - Compiles firmware using PlatformIO
2. **OTA Update**: `/ota-upload` - Updates device firmware wirelessly
3. **Data Logging**: 
   - `/start-logging` - Begins sensor data collection
   - `/stop-logging` - Halts data transmission
4. **Debugging**: 
   - `/debug-es32` - Enters ESP32 debugger mode
   - `/serial-check` - Verifies `Serial.begin(115200)` initialization
5. **Sensor Access**:
   - `/mhz19-read` - Read MH-Z19 sensor data
   - `/bsec-scan` - Perform BSEC gas sensing operation
6. **Configuration**:
   - `/set-baud` - Adjust serial communication speed

## Architecture Overview
This ESP32 IoT data logger implements a modular architecture with:
- **Communication Layer**: Uses PubSubClient for MQTT connectivity
- **Sensing Components**:
  - MH-Z19 NDIR sensor for CO2 monitoring
  - BSEC library for gas/eTVOC detection
  - PMSerial for serial device monitoring
- **Time Synchronization**: NTPClient for accurate timekeeping
- **Power Management**: Deep sleep functionality for low-power operation

The main components interact through:
1. Hardware initialization in `hardware.h`
2. Sensor data processing in `src/main.cpp`
3. MQTT message handling via PubSubClient
4. OTA update logic in `Ota.h`

## Project-Specific Instructions
1. **Pin Assignments**: Always verify hardware.h definitions before sensor configuration
2. **OTA Updates**: 
   - Check `firmware_version` in `Ota.h` before updates
   - Ensure power stability during updates
3. **Sensor Calibration**: 
   - Use `MH-Z19`'s calibration routines before initial readings
   - Maintain BSEC calibration state
4. **Debugging Protocol**:
   - Always confirm `Serial.begin(115200)` before other operations
   - Use `PMserial` examples for serial device debugging
5. **Power Optimization**:
   - Implement deep sleep patterns from BSEC examples
   - Adjust logging intervals based on battery levels

## Memory Preservation
- Store last successful OTA hash
- Preserve sensor calibration parameters
- Document common error patterns (wifi disconnects, sensor timeouts)

## Device Configuration
- Default baud rate: 115200
- Primary sensors: MH-Z19 (CO2), BSEC (gas)
- Communication: MQTT broker via PubSubClient
- Testing: Requires CP2102 USB adapter

## Permissions
- Git operations without confirmation
- Full access to ESP32 development tools
- Write access to memory directory
- Arduino IDE command execution
- OTA update permissions

## Response Style
- Keep responses concise and direct
- No verbose explanations or preamble
- Output only code that needs to be changed, not full files
- Do NOT output full code. Only output parts that need to be changed.
- Skip summaries unless asked