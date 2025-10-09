# BMM150 Magnetometer Data Processing Guide

## Overview
This document details the complete data processing pipeline for the BMM150 3-axis digital magnetometer, including raw data acquisition, compensation algorithms, and heading calculation.

## Hardware Configuration
- **I2C Address**: 0x10 (SD0 pin connected to GND)
- **I2C Speed**: 100kHz
- **Data Rate**: 10Hz (100ms intervals)
- **Power Mode**: Normal mode with low power preset

## Data Processing Pipeline

### 1. Raw Data Acquisition

#### Register Reading Sequence
```
Start → Write Register Address (0x42) → Read 8 Bytes → Process Data
```

#### Data Registers (8-byte burst read from 0x42)
| Register | Description | Bits | Data |
|----------|-------------|------|------|
| 0x42 | X LSB | 7:3 | X-axis data (5 bits) |
| 0x43 | X MSB | 7:0 | X-axis data (8 bits) |
| 0x44 | Y LSB | 7:3 | Y-axis data (5 bits) |
| 0x45 | Y MSB | 7:0 | Y-axis data (8 bits) |
| 0x46 | Z LSB | 7:1 | Z-axis data (7 bits) |
| 0x47 | Z MSB | 7:0 | Z-axis data (8 bits) |
| 0x48 | R LSB | 7:2 | Hall resistance (6 bits) |
| 0x49 | R MSB | 7:0 | Hall resistance (8 bits) |

#### Raw Data Assembly
```c
// X-axis: 13-bit signed value
raw_datax = (MSB << 5) | (LSB & 0x1F)

// Y-axis: 13-bit signed value  
raw_datay = (MSB << 5) | (LSB & 0x1F)

// Z-axis: 15-bit signed value
raw_dataz = (MSB << 7) | (LSB & 0x7F)

// Hall resistance: 14-bit unsigned value
raw_data_r = (MSB << 6) | (LSB & 0x3F)
```

### 2. Compensation Process

The BMM150 requires sophisticated compensation algorithms to convert raw ADC values to accurate magnetic field measurements in microtesla (μT).

#### Required Calibration Data (Trim Registers)
```c
typedef struct {
    int8_t dig_x1, dig_y1;           // X/Y axis sensitivity
    int8_t dig_x2, dig_y2;           // X/Y axis sensitivity  
    uint16_t dig_z1;                 // Z-axis sensitivity
    int16_t dig_z2, dig_z3, dig_z4;  // Z-axis sensitivity
    uint8_t dig_xy1;                 // XY cross-sensitivity
    int8_t dig_xy2;                  // XY cross-sensitivity
    uint16_t dig_xyz1;               // XYZ cross-sensitivity
} bmm150_trim_registers_t;
```

#### X-Axis Compensation Algorithm
```c
int16_t compensate_x(int16_t mag_data_x, uint16_t data_rhall) {
    // 1. Overflow check
    if (mag_data_x == BMM150_XYAXES_FLIP_OVERFLOW_ADCVAL) {
        return BMM150_OVERFLOW_OUTPUT;
    }
    
    // 2. Hall resistance validation
    if (data_rhall == 0) {
        data_rhall = trim_data.dig_xyz1;
    }
    
    // 3. Compensation equations (simplified)
    process_comp_x1 = trim_data.dig_xyz1 * 16384;
    process_comp_x2 = (process_comp_x1 / data_rhall) - 0x4000;
    
    // 4. Non-linear compensation
    process_comp_x3 = retval * retval;
    process_comp_x4 = trim_data.dig_xy2 * (process_comp_x3 / 128);
    process_comp_x5 = trim_data.dig_xy1 * 128;
    process_comp_x6 = retval * process_comp_x5;
    
    // 5. Final compensation
    process_comp_x7 = ((process_comp_x4 + process_comp_x6) / 512) + 0x100000;
    process_comp_x8 = (trim_data.dig_x2 + 0xA0);
    process_comp_x9 = (process_comp_x7 * process_comp_x8) / 4096;
    process_comp_x10 = mag_data_x * process_comp_x9;
    
    retval = (process_comp_x10 / 8192);
    retval = (retval + (trim_data.dig_x1 * 8)) / 16;
    
    return retval;
}
```

#### Y-Axis Compensation Algorithm
Similar to X-axis but with Y-specific trim values:
```c
int16_t compensate_y(int16_t mag_data_y, uint16_t data_rhall) {
    // Similar process with dig_y1, dig_y2, dig_xy1, dig_xy2
}
```

#### Z-Axis Compensation Algorithm
More complex due to different bit resolution:
```c
int16_t compensate_z(int16_t mag_data_z, uint16_t data_rhall) {
    // 1. Overflow check
    if (mag_data_z == BMM150_ZAXIS_HALL_OVERFLOW_ADCVAL) {
        return BMM150_OVERFLOW_OUTPUT;
    }
    
    // 2. Z-axis specific compensation
    process_comp_z0 = data_rhall - trim_data.dig_xyz1;
    process_comp_z1 = (trim_data.dig_z3 * process_comp_z0) / 4;
    process_comp_z2 = ((mag_data_z - trim_data.dig_z4) * 32768);
    process_comp_z3 = trim_data.dig_z1 * (data_rhall * 2);
    process_comp_z4 = (process_comp_z3 + 32768) / 65536;
    
    retval = (process_comp_z2 - process_comp_z1) / (trim_data.dig_z2 + process_comp_z4);
    
    // 3. Saturation limits (±2 μT)
    if (retval > BMM150_POSITIVE_SATURATION_Z) {
        retval = BMM150_POSITIVE_SATURATION_Z;
    } else if (retval < BMM150_NEGATIVE_SATURATION_Z) {
        retval = BMM150_NEGATIVE_SATURATION_Z;
    }
    
    return retval / 16;  // Convert to μT
}
```

### 3. Heading Calculation

#### Compass Heading Algorithm
```c
// 1. Calculate heading using atan2
float heading = atan2(compensated_x, compensated_y);

// 2. Normalize to 0-2π range
if (heading < 0) {
    heading += 2 * M_PI;
}
if (heading > 2 * M_PI) {
    heading -= 2 * M_PI;
}

// 3. Convert to degrees
float heading_degrees = heading * 180.0f / M_PI;

// 4. Determine compass direction
const char* direction = get_compass_direction(heading_degrees);
```

#### Compass Direction Mapping
| Range | Direction |
|-------|-----------|
| 337.5° - 22.5° | North |
| 22.5° - 67.5° | Northeast |
| 67.5° - 112.5° | East |
| 112.5° - 157.5° | Southeast |
| 157.5° - 202.5° | South |
| 202.5° - 247.5° | Southwest |
| 247.5° - 292.5° | West |
| 292.5° - 337.5° | Northwest |

## Required APIs

### Core Initialization APIs
```c
// 1. I2C Initialization
OPERATE_RET bmm150_i2c_init(void);

// 2. Device Initialization
OPERATE_RET bmm150_init(bmm150_dev_t *dev, uint8_t i2c_addr);

// 3. Trim Register Reading
OPERATE_RET bmm150_read_trim_registers(bmm150_dev_t *dev);
```

### Data Acquisition APIs
```c
// 1. Raw Data Reading
OPERATE_RET bmm150_read_mag_data(bmm150_dev_t *dev);

// 2. Register Access
OPERATE_RET bmm150_read_register(bmm150_dev_t *dev, uint8_t reg, uint8_t *buffer, uint8_t length);
OPERATE_RET bmm150_write_register(bmm150_dev_t *dev, uint8_t reg, uint8_t value);
```

### Compensation APIs
```c
// 1. X-axis compensation
int16_t bmm150_compensate_x(bmm150_dev_t *dev, int16_t mag_data_x, uint16_t data_rhall);

// 2. Y-axis compensation  
int16_t bmm150_compensate_y(bmm150_dev_t *dev, int16_t mag_data_y, uint16_t data_rhall);

// 3. Z-axis compensation
int16_t bmm150_compensate_z(bmm150_dev_t *dev, int16_t mag_data_z, uint16_t data_rhall);
```

### Configuration APIs
```c
// 1. Power mode control
OPERATE_RET bmm150_set_op_mode(bmm150_dev_t *dev, uint8_t op_mode);

// 2. Preset mode selection
OPERATE_RET bmm150_set_preset_mode(bmm150_dev_t *dev, uint8_t preset_mode);

// 3. Power control
OPERATE_RET bmm150_set_power_control_bit(bmm150_dev_t *dev, uint8_t pwrcntrl_bit);
```

## Data Flow Summary

```
Raw ADC Values → Bit Assembly → Compensation → Heading Calculation → Output
     ↓              ↓              ↓              ↓              ↓
  [0x42-0x49]   [13/15-bit]   [μT values]   [0-360°]    [One-liner]
```

## Output Format
```
BMM150: X=123μT Y=456μT Z=789μT | Heading=45.2° (Northeast)
```

## Key Features
- **High Accuracy**: Proper compensation algorithms from Grove driver
- **Real-time**: 10Hz sampling rate (100ms intervals)
- **Robust**: Overflow handling and saturation limits
- **Clean Output**: Single formatted line with all essential data
- **Compass Ready**: Direct heading calculation with direction mapping

## Error Handling
- **Overflow Detection**: Handles sensor saturation conditions
- **I2C Errors**: Proper error codes for communication failures
- **Invalid Data**: Overflow output (-32768) for invalid readings
- **Calibration**: Automatic trim register reading during initialization
