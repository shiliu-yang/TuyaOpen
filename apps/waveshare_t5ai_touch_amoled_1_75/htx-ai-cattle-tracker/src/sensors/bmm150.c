#include "dev_config.h"
#include "bmm150.h"
#include "tal_system.h"
#include <math.h>

// I2C Initialization using BMM150 I2C Port 1 (GPIO 24/25)
OPERATE_RET bmm150_i2c_init(void) {
    OPERATE_RET ret = bmm150_i2c_port_init();
    if (ret != OPRT_OK) {
        PR_ERR("Failed to initialize BMM150 I2C Port 1");
        return ret;
    }
    
    PR_INFO("BMM150 I2C initialized successfully on Port 1 (GPIO24/25)");
    return OPRT_OK;
}

// I2C write register using BMM150 I2C Port 1
OPERATE_RET bmm150_i2c_write_register(uint8_t addr, uint8_t reg, uint8_t value) {
    return bmm150_i2c_write_reg(addr, reg, value);
}

// I2C read register using BMM150 I2C Port 1
OPERATE_RET bmm150_i2c_read_register(uint8_t addr, uint8_t reg, uint8_t *buffer, uint8_t length) {
    if (!buffer || length == 0) return OPRT_COM_ERROR;
    
    return bmm150_i2c_read_reg(addr, reg, buffer, length);
}

// I2C Bus Scanner using dev_config.c (following GPS pattern)
OPERATE_RET bmm150_scan_i2c_bus(bmm150_dev_t *dev) {
    if (!dev) return OPRT_COM_ERROR;
    
    PR_INFO("Scanning I2C bus for BMM150...");
    
    // Try multiple possible addresses for BMM150 (0x10 when SD0=GND, 0x11 when SD0=3.3V)
    uint8_t addresses[] = {0x10, 0x11};
    bool found = false;
    
    for (int i = 0; i < 2; i++) {
        PR_INFO("Trying address 0x%02X...", addresses[i]);
        
        uint8_t test_data;
        OPERATE_RET ret = bmm150_i2c_read_register(addresses[i], BMM150_CHIP_ID_ADDR, &test_data, 1);
        
        if (ret == OPRT_OK) {
            if (test_data == BMM150_CHIP_ID) {
                dev->i2c_addr = addresses[i];
                PR_INFO("BMM150 found at address 0x%02X", addresses[i]);
                found = true;
                break;
            }
        }
        PR_INFO("No device at address 0x%02X", addresses[i]);
        tal_system_sleep(10);
    }
    
    if (!found) {
        PR_ERR("No BMM150 found on any address");
        PR_INFO("Scanning entire I2C bus for any devices...");
        
        // Scan entire I2C bus
        for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
            uint8_t test_data;
            OPERATE_RET ret = bmm150_i2c_read_register(addr, BMM150_CHIP_ID_ADDR, &test_data, 1);
            if (ret == OPRT_OK) {
                PR_INFO("Device found at address 0x%02X (ID: 0x%02X)", addr, test_data);
            }
            tal_system_sleep(1);
        }
        return OPRT_COM_ERROR;
    }
    
    return OPRT_OK;
}

// Initialize BMM150 with proper power-up sequence (following datasheet)
OPERATE_RET bmm150_init(bmm150_dev_t *dev, uint8_t i2c_addr) {
    if (!dev) return OPRT_COM_ERROR;
    
    // Initialize I2C using dev_config.c (following GPS pattern)
    OPERATE_RET ret = bmm150_i2c_init();
    if (ret != OPRT_OK) {
        PR_ERR("Failed to initialize I2C from dev_config.c");
        return ret;
    }
    
    dev->i2c_addr = i2c_addr;
    
    // Initialize calibration data
    dev->calibration.calibrated = false;
    dev->calibration.x_offset = 0;
    dev->calibration.y_offset = 0;
    dev->calibration.z_offset = 0;
    dev->calibration.calibration_time = 0;
    
    PR_INFO("Initializing BMM150 with proper power-up sequence...");
    
    // Step 1: Power on the sensor (set power control bit)
    PR_INFO("Step 1: Powering on BMM150...");
    ret = bmm150_set_power_control_bit(dev, BMM150_POWER_CNTRL_ENABLE);
    if (ret != OPRT_OK) {
        PR_ERR("Failed to power on BMM150");
        return ret;
    }
    
    // Step 2: Wait for power-up time (datasheet requirement)
    PR_INFO("Step 2: Waiting for power-up...");
    tal_system_sleep(10); // 10ms power-up time as per datasheet
    
    // Step 3: Set to sleep mode first
    PR_INFO("Step 3: Setting sleep mode...");
    ret = bmm150_set_op_mode(dev, BMM150_SLEEP_MODE);
    if (ret != OPRT_OK) {
        PR_ERR("Failed to set sleep mode");
        return ret;
    }
    tal_system_sleep(BMM150_START_UP_TIME);
    
    // Step 4: Now try to read chip ID
    PR_INFO("Step 4: Reading chip ID...");
    uint8_t chip_id;
    ret = bmm150_read_register(dev, BMM150_CHIP_ID_ADDR, &chip_id, 1);
    if (ret != OPRT_OK) {
        PR_ERR("Failed to read chip ID, error: %d", ret);
        PR_ERR("Check I2C connections and address (0x%02X)", i2c_addr);
        return ret;
    }
    
    if (chip_id != BMM150_CHIP_ID) {
        PR_ERR("Invalid chip ID: 0x%02X (expected: 0x%02X)", chip_id, BMM150_CHIP_ID);
        return OPRT_COM_ERROR;
    }
    
    PR_INFO("BMM150 chip ID verified: 0x%02X", chip_id);
    
    // Read trim registers
    ret = bmm150_read_trim_registers(dev);
    if (ret != OPRT_OK) {
        PR_ERR("Failed to read trim registers");
        return ret;
    }
    
    // Set power mode to normal
    ret = bmm150_set_op_mode(dev, BMM150_NORMAL_MODE);
    if (ret != OPRT_OK) {
        PR_ERR("Failed to set normal mode");
        return ret;
    }
    
    // Set preset mode to low power
    ret = bmm150_set_preset_mode(dev, BMM150_PRESETMODE_LOWPOWER);
    if (ret != OPRT_OK) {
        PR_ERR("Failed to set preset mode");
        return ret;
    }
    
    PR_INFO("BMM150 initialized successfully");
    return OPRT_OK;
}

// Read magnetometer data using Grove driver algorithm
OPERATE_RET bmm150_read_mag_data(bmm150_dev_t *dev) {
    if (!dev) return OPRT_COM_ERROR;
    
    int16_t msb_data;
    int8_t reg_data[BMM150_XYZR_DATA_LEN] = {0};
    
    OPERATE_RET ret = bmm150_read_register(dev, BMM150_DATA_X_LSB, (uint8_t*)reg_data, BMM150_XYZR_DATA_LEN);
    if (ret != OPRT_OK) {
        return ret;
    }
    
    // Mag X axis data (from Grove driver)
    reg_data[0] = BMM150_GET_BITS(reg_data[0], BMM150_DATA_X);
    msb_data = ((int16_t)((int8_t)reg_data[1])) * 32;
    dev->raw_mag_data.raw_datax = (int16_t)(msb_data | reg_data[0]);
    
    // Mag Y axis data
    reg_data[2] = BMM150_GET_BITS(reg_data[2], BMM150_DATA_Y);
    msb_data = ((int16_t)((int8_t)reg_data[3])) * 32;
    dev->raw_mag_data.raw_datay = (int16_t)(msb_data | reg_data[2]);
    
    // Mag Z axis data
    reg_data[4] = BMM150_GET_BITS(reg_data[4], BMM150_DATA_Z);
    msb_data = ((int16_t)((int8_t)reg_data[5])) * 128;
    dev->raw_mag_data.raw_dataz = (int16_t)(msb_data | reg_data[4]);
    
    // Mag R-HALL data
    reg_data[6] = BMM150_GET_BITS(reg_data[6], BMM150_DATA_RHALL);
    dev->raw_mag_data.raw_data_r = (uint16_t)(((uint16_t)reg_data[7] << 6) | reg_data[6]);
    
    // Compensated data using Grove driver algorithms
    dev->mag_data.x = bmm150_compensate_x(dev, dev->raw_mag_data.raw_datax, dev->raw_mag_data.raw_data_r);
    dev->mag_data.y = bmm150_compensate_y(dev, dev->raw_mag_data.raw_datay, dev->raw_mag_data.raw_data_r);
    dev->mag_data.z = bmm150_compensate_z(dev, dev->raw_mag_data.raw_dataz, dev->raw_mag_data.raw_data_r);
    
    return OPRT_OK;
}

// X-axis compensation algorithm from Grove driver
int16_t bmm150_compensate_x(bmm150_dev_t *dev, int16_t mag_data_x, uint16_t data_rhall) {
    int16_t retval;
    uint16_t process_comp_x0 = 0;
    int32_t process_comp_x1;
    uint16_t process_comp_x2;
    int32_t process_comp_x3;
    int32_t process_comp_x4;
    int32_t process_comp_x5;
    int32_t process_comp_x6;
    int32_t process_comp_x7;
    int32_t process_comp_x8;
    int32_t process_comp_x9;
    int32_t process_comp_x10;
    
    if (mag_data_x != BMM150_XYAXES_FLIP_OVERFLOW_ADCVAL) {
        if (data_rhall != 0) {
            process_comp_x0 = data_rhall;
        } else if (dev->trim_data.dig_xyz1 != 0) {
            process_comp_x0 = dev->trim_data.dig_xyz1;
        } else {
            process_comp_x0 = 0;
        }
        if (process_comp_x0 != 0) {
            process_comp_x1 = ((int32_t)dev->trim_data.dig_xyz1) * 16384;
            process_comp_x2 = ((uint16_t)(process_comp_x1 / process_comp_x0)) - ((uint16_t)0x4000);
            retval = ((int16_t)process_comp_x2);
            process_comp_x3 = (((int32_t)retval) * ((int32_t)retval));
            process_comp_x4 = (((int32_t)dev->trim_data.dig_xy2) * (process_comp_x3 / 128));
            process_comp_x5 = (int32_t)(((int16_t)dev->trim_data.dig_xy1) * 128);
            process_comp_x6 = ((int32_t)retval) * process_comp_x5;
            process_comp_x7 = (((process_comp_x4 + process_comp_x6) / 512) + ((int32_t)0x100000));
            process_comp_x8 = ((int32_t)(((int16_t)dev->trim_data.dig_x2) + ((int16_t)0xA0)));
            process_comp_x9 = ((process_comp_x7 * process_comp_x8) / 4096);
            process_comp_x10 = ((int32_t)mag_data_x) * process_comp_x9;
            retval = ((int16_t)(process_comp_x10 / 8192));
            retval = (retval + (((int16_t)dev->trim_data.dig_x1) * 8)) / 16;
        } else {
            retval = BMM150_OVERFLOW_OUTPUT;
        }
    } else {
        retval = BMM150_OVERFLOW_OUTPUT;
    }
    
    return retval;
}

// Y-axis compensation algorithm from Grove driver
int16_t bmm150_compensate_y(bmm150_dev_t *dev, int16_t mag_data_y, uint16_t data_rhall) {
    int16_t retval;
    uint16_t process_comp_y0 = 0;
    int32_t process_comp_y1;
    uint16_t process_comp_y2;
    int32_t process_comp_y3;
    int32_t process_comp_y4;
    int32_t process_comp_y5;
    int32_t process_comp_y6;
    int32_t process_comp_y7;
    int32_t process_comp_y8;
    int32_t process_comp_y9;
    
    if (mag_data_y != BMM150_XYAXES_FLIP_OVERFLOW_ADCVAL) {
        if (data_rhall != 0) {
            process_comp_y0 = data_rhall;
        } else if (dev->trim_data.dig_xyz1 != 0) {
            process_comp_y0 = dev->trim_data.dig_xyz1;
        } else {
            process_comp_y0 = 0;
        }
        if (process_comp_y0 != 0) {
            process_comp_y1 = (((int32_t)dev->trim_data.dig_xyz1) * 16384) / process_comp_y0;
            process_comp_y2 = ((uint16_t)process_comp_y1) - ((uint16_t)0x4000);
            retval = ((int16_t)process_comp_y2);
            process_comp_y3 = ((int32_t) retval) * ((int32_t)retval);
            process_comp_y4 = ((int32_t)dev->trim_data.dig_xy2) * (process_comp_y3 / 128);
            process_comp_y5 = ((int32_t)(((int16_t)dev->trim_data.dig_xy1) * 128));
            process_comp_y6 = ((process_comp_y4 + (((int32_t)retval) * process_comp_y5)) / 512);
            process_comp_y7 = ((int32_t)(((int16_t)dev->trim_data.dig_y2) + ((int16_t)0xA0)));
            process_comp_y8 = (((process_comp_y6 + ((int32_t)0x100000)) * process_comp_y7) / 4096);
            process_comp_y9 = (((int32_t)mag_data_y) * process_comp_y8);
            retval = (int16_t)(process_comp_y9 / 8192);
            retval = (retval + (((int16_t)dev->trim_data.dig_y1) * 8)) / 16;
        } else {
            retval = BMM150_OVERFLOW_OUTPUT;
        }
    } else {
        retval = BMM150_OVERFLOW_OUTPUT;
    }
    
    return retval;
}

// Z-axis compensation algorithm from Grove driver
int16_t bmm150_compensate_z(bmm150_dev_t *dev, int16_t mag_data_z, uint16_t data_rhall) {
    int32_t retval;
    int16_t process_comp_z0;
    int32_t process_comp_z1;
    int32_t process_comp_z2;
    int32_t process_comp_z3;
    int16_t process_comp_z4;
    
    if (mag_data_z != BMM150_ZAXIS_HALL_OVERFLOW_ADCVAL) {
        if ((dev->trim_data.dig_z2 != 0) && (dev->trim_data.dig_z1 != 0)
                && (data_rhall != 0) && (dev->trim_data.dig_xyz1 != 0)) {
            process_comp_z0 = ((int16_t)data_rhall) - ((int16_t) dev->trim_data.dig_xyz1);
            process_comp_z1 = (((int32_t)dev->trim_data.dig_z3) * ((int32_t)(process_comp_z0))) / 4;
            process_comp_z2 = (((int32_t)(mag_data_z - dev->trim_data.dig_z4)) * 32768);
            process_comp_z3 = ((int32_t)dev->trim_data.dig_z1) * (((int16_t)data_rhall) * 2);
            process_comp_z4 = (int16_t)((process_comp_z3 + (32768)) / 65536);
            retval = ((process_comp_z2 - process_comp_z1) / (dev->trim_data.dig_z2 + process_comp_z4));
            
            if (retval > BMM150_POSITIVE_SATURATION_Z) {
                retval =  BMM150_POSITIVE_SATURATION_Z;
            } else {
                if (retval < BMM150_NEGATIVE_SATURATION_Z) {
                    retval = BMM150_NEGATIVE_SATURATION_Z;
                }
            }
            retval = retval / 16;
        } else {
            retval = BMM150_OVERFLOW_OUTPUT;
        }
    } else {
        retval = BMM150_OVERFLOW_OUTPUT;
    }
    
    return (int16_t)retval;
}

// Set preset mode using Grove driver algorithm
OPERATE_RET bmm150_set_preset_mode(bmm150_dev_t *dev, uint8_t preset_mode) {
    if (!dev) return OPRT_COM_ERROR;
    
    switch (preset_mode) {
        case BMM150_PRESETMODE_LOWPOWER:
            dev->settings.data_rate = BMM150_DATA_RATE_10HZ;
            dev->settings.xy_rep = BMM150_LOWPOWER_REPXY;
            dev->settings.z_rep = BMM150_LOWPOWER_REPZ;
            break;
        case BMM150_PRESETMODE_REGULAR:
            dev->settings.data_rate = BMM150_DATA_RATE_10HZ;
            dev->settings.xy_rep = BMM150_REGULAR_REPXY;
            dev->settings.z_rep = BMM150_REGULAR_REPZ;
            break;
        case BMM150_PRESETMODE_HIGHACCURACY:
            dev->settings.data_rate = BMM150_DATA_RATE_20HZ;
            dev->settings.xy_rep = BMM150_HIGHACCURACY_REPXY;
            dev->settings.z_rep = BMM150_HIGHACCURACY_REPZ;
            break;
        case BMM150_PRESETMODE_ENHANCED:
            dev->settings.data_rate = BMM150_DATA_RATE_10HZ;
            dev->settings.xy_rep = BMM150_ENHANCED_REPXY;
            dev->settings.z_rep = BMM150_ENHANCED_REPZ;
            break;
        default:
            return OPRT_COM_ERROR;
    }
    
    // Set ODR and repetitions
    OPERATE_RET ret = bmm150_set_odr(dev, dev->settings);
    if (ret != OPRT_OK) return ret;
    
    ret = bmm150_set_xy_rep(dev, dev->settings);
    if (ret != OPRT_OK) return ret;
    
    ret = bmm150_set_z_rep(dev, dev->settings);
    if (ret != OPRT_OK) return ret;
    
    return OPRT_OK;
}

// Set operation mode
OPERATE_RET bmm150_set_op_mode(bmm150_dev_t *dev, uint8_t op_mode) {
    if (!dev) return OPRT_COM_ERROR;
    
    switch (op_mode) {
        case BMM150_NORMAL_MODE:
            bmm150_suspend_to_sleep_mode(dev);
            return bmm150_write_op_mode(dev, op_mode);
        case BMM150_FORCED_MODE:
            bmm150_suspend_to_sleep_mode(dev);
            return bmm150_write_op_mode(dev, op_mode);
        case BMM150_SLEEP_MODE:
            bmm150_suspend_to_sleep_mode(dev);
            return bmm150_write_op_mode(dev, op_mode);
        case BMM150_SUSPEND_MODE:
            return bmm150_set_power_control_bit(dev, BMM150_POWER_CNTRL_DISABLE);
        default:
            return OPRT_COM_ERROR;
    }
}

// Read trim registers using Grove driver algorithm
OPERATE_RET bmm150_read_trim_registers(bmm150_dev_t *dev) {
    if (!dev) return OPRT_COM_ERROR;
    
    uint8_t trim_x1y1[2] = {0};
    uint8_t trim_xyz_data[4] = {0};
    uint8_t trim_xy1xy2[10] = {0};
    uint16_t temp_msb = 0;
    
    OPERATE_RET ret = bmm150_read_register(dev, BMM150_DIG_X1, trim_x1y1, 2);
    if (ret != OPRT_OK) return ret;
    
    ret = bmm150_read_register(dev, BMM150_DIG_Z4_LSB, trim_xyz_data, 4);
    if (ret != OPRT_OK) return ret;
    
    ret = bmm150_read_register(dev, BMM150_DIG_Z2_LSB, trim_xy1xy2, 10);
    if (ret != OPRT_OK) return ret;
    
    // Parse trim data
    dev->trim_data.dig_x1 = (int8_t)trim_x1y1[0];
    dev->trim_data.dig_y1 = (int8_t)trim_x1y1[1];
    dev->trim_data.dig_x2 = (int8_t)trim_xyz_data[2];
    dev->trim_data.dig_y2 = (int8_t)trim_xyz_data[3];
    temp_msb = ((uint16_t)trim_xy1xy2[3]) << 8;
    dev->trim_data.dig_z1 = (uint16_t)(temp_msb | trim_xy1xy2[2]);
    temp_msb = ((uint16_t)trim_xy1xy2[1]) << 8;
    dev->trim_data.dig_z2 = (int16_t)(temp_msb | trim_xy1xy2[0]);
    temp_msb = ((uint16_t)trim_xy1xy2[7]) << 8;
    dev->trim_data.dig_z3 = (int16_t)(temp_msb | trim_xy1xy2[6]);
    temp_msb = ((uint16_t)trim_xyz_data[1]) << 8;
    dev->trim_data.dig_z4 = (int16_t)(temp_msb | trim_xyz_data[0]);
    dev->trim_data.dig_xy1 = trim_xy1xy2[9];
    dev->trim_data.dig_xy2 = (int8_t)trim_xy1xy2[8];
    temp_msb = ((uint16_t)(trim_xy1xy2[5] & 0x7F)) << 8;
    dev->trim_data.dig_xyz1 = (uint16_t)(temp_msb | trim_xy1xy2[4]);
    
    return OPRT_OK;
}

// Helper functions
OPERATE_RET bmm150_write_op_mode(bmm150_dev_t *dev, uint8_t op_mode) {
    if (!dev) return OPRT_COM_ERROR;
    
    uint8_t reg_data = 0;
    OPERATE_RET ret = bmm150_read_register(dev, BMM150_OP_MODE_ADDR, &reg_data, 1);
    if (ret != OPRT_OK) return ret;
    
    reg_data = BMM150_SET_BITS(reg_data, BMM150_OP_MODE, op_mode);
    return bmm150_write_register(dev, BMM150_OP_MODE_ADDR, reg_data);
}

OPERATE_RET bmm150_set_power_control_bit(bmm150_dev_t *dev, uint8_t pwrcntrl_bit) {
    if (!dev) return OPRT_COM_ERROR;
    
    uint8_t reg_data = 0;
    OPERATE_RET ret = bmm150_read_register(dev, BMM150_POWER_CONTROL_ADDR, &reg_data, 1);
    if (ret != OPRT_OK) return ret;
    
    reg_data = BMM150_SET_BITS_POS_0(reg_data, BMM150_PWR_CNTRL, pwrcntrl_bit);
    return bmm150_write_register(dev, BMM150_POWER_CONTROL_ADDR, reg_data);
}

void bmm150_suspend_to_sleep_mode(bmm150_dev_t *dev) {
    bmm150_set_power_control_bit(dev, BMM150_POWER_CNTRL_ENABLE);
    tal_system_sleep(3);
}

OPERATE_RET bmm150_set_odr(bmm150_dev_t *dev, bmm150_settings_t settings) {
    if (!dev) return OPRT_COM_ERROR;
    
    uint8_t reg_data = 0;
    OPERATE_RET ret = bmm150_read_register(dev, BMM150_OP_MODE_ADDR, &reg_data, 1);
    if (ret != OPRT_OK) return ret;
    
    reg_data = BMM150_SET_BITS(reg_data, BMM150_ODR, settings.data_rate);
    return bmm150_write_register(dev, BMM150_OP_MODE_ADDR, reg_data);
}

OPERATE_RET bmm150_set_xy_rep(bmm150_dev_t *dev, bmm150_settings_t settings) {
    if (!dev) return OPRT_COM_ERROR;
    return bmm150_write_register(dev, BMM150_REP_XY_ADDR, settings.xy_rep);
}

OPERATE_RET bmm150_set_z_rep(bmm150_dev_t *dev, bmm150_settings_t settings) {
    if (!dev) return OPRT_COM_ERROR;
    return bmm150_write_register(dev, BMM150_REP_Z_ADDR, settings.z_rep);
}

OPERATE_RET bmm150_soft_reset(bmm150_dev_t *dev) {
    if (!dev) return OPRT_COM_ERROR;
    
    uint8_t reg_data = 0;
    OPERATE_RET ret = bmm150_read_register(dev, BMM150_POWER_CONTROL_ADDR, &reg_data, 1);
    if (ret != OPRT_OK) return ret;
    
    reg_data = reg_data | BMM150_SET_SOFT_RESET;
    ret = bmm150_write_register(dev, BMM150_POWER_CONTROL_ADDR, reg_data);
    if (ret != OPRT_OK) return ret;
    
    tal_system_sleep(BMM150_SOFT_RESET_DELAY);
    return OPRT_OK;
}

// Test Functions using dev_config.c (following GPS pattern)
OPERATE_RET bmm150_test_i2c_port(void) {
    PR_INFO("=== I2C Port Test using dev_config.c ===");
    
    // Initialize I2C using dev_config.c
    OPERATE_RET ret = bmm150_i2c_init();
    if (ret != OPRT_OK) {
        PR_ERR("I2C initialization failed");
        return ret;
    }
    
    PR_INFO("I2C initialized successfully using dev_config.c");
    

    // Test both possible BMM150 addresses
    PR_INFO("Testing BMM150 addresses...");
    uint8_t addresses[] = {0x10, 0x11};
    bool found_device = false;
    
    for (int i = 0; i < 2; i++) {
        PR_INFO("Testing address 0x%02X...", addresses[i]);
        
        // First try to write to power control register (this should get ACK if device is present)
        PR_INFO("  Attempting to write to power control register...");
        ret = bmm150_i2c_write_register(addresses[i], BMM150_POWER_CONTROL_ADDR, BMM150_POWER_CNTRL_ENABLE);
        
        if (ret == OPRT_OK) {
            PR_INFO("  ✓ Power control write successful - device is responding!");
            found_device = true;
            
            // Wait for power-up
            tal_system_sleep(20);
            
            // Now try to read chip ID
            PR_INFO("  Attempting to read chip ID from register 0x%02X...", BMM150_CHIP_ID_ADDR);
            uint8_t chip_id;
            ret = bmm150_i2c_read_register(addresses[i], BMM150_CHIP_ID_ADDR, &chip_id, 1);
            
            if (ret == OPRT_OK) {
                PR_INFO("  ✓ Chip ID read successful: 0x%02X", chip_id);
                
                if (chip_id == BMM150_CHIP_ID) {
                    PR_INFO("  ✓ BMM150 found at address 0x%02X!", addresses[i]);
                    return OPRT_OK; // Found the correct device
                } else {
                    PR_INFO("  ⚠ Wrong chip ID: 0x%02X (expected: 0x%02X)", chip_id, BMM150_CHIP_ID);
                    PR_INFO("  This suggests the device is responding but not a BMM150");
                    PR_INFO("  Check if SD0 pin is properly connected (should be GND for 0x10, VCC for 0x11)");
                }
            } else {
                PR_INFO("  ✗ Chip ID read failed (error: %d)", ret);
            }
            
            // Try reading from a different register to test if device is responding correctly
            PR_INFO("  Testing read from power control register (0x%02X)...", BMM150_POWER_CONTROL_ADDR);
            uint8_t power_control;
            ret = bmm150_i2c_read_register(addresses[i], BMM150_POWER_CONTROL_ADDR, &power_control, 1);
            if (ret == OPRT_OK) {
                PR_INFO("  ✓ Power control read: 0x%02X", power_control);
            } else {
                PR_INFO("  ✗ Power control read failed (error: %d)", ret);
            }
        } else {
            PR_INFO("  ✗ Power control write failed (error: %d)", ret);
            PR_INFO("  This means the device is not responding to address 0x%02X", addresses[i]);
        }
        
        // Small delay between tests
        tal_system_sleep(100); // Longer delay to see both addresses clearly
    }
    
    if (!found_device) {
        PR_ERR("No I2C devices found on either address");
        PR_ERR("Check connections:");
        PR_ERR("- VCC to 3.3V");
        PR_ERR("- GND to GND"); 
        PR_ERR("- SCL to I2C_SCL_PIN with 4.7kΩ pull-up");
        PR_ERR("- SDA to I2C_SDA_PIN with 4.7kΩ pull-up");
        PR_ERR("- SD0 to GND (for address 0x10) or VDD (for address 0x11)");
        PR_ERR("- CSB to VDD (for I2C mode)");
    }
    
    PR_INFO("I2C Port Test: Complete");
    return found_device ? OPRT_OK : OPRT_COM_ERROR;
}

OPERATE_RET bmm150_test_chip_id(uint8_t addr) {
    PR_INFO("=== Chip ID Test for Address 0x%02X ===", addr);
    
    // Since we're getting NACK, the BMM150 is likely in power-off mode
    // Try to power it on first by writing to power control register
    PR_INFO("Attempting to power on BMM150...");
    
    // Try to write to power control register (0x4B) to enable power
    PR_INFO("Writing to power control register (0x4B)...");
    OPERATE_RET ret = bmm150_i2c_write_register(addr, BMM150_POWER_CONTROL_ADDR, BMM150_POWER_CNTRL_ENABLE);
    
    if (ret != OPRT_OK) {
        PR_INFO("Power control write failed (error: %d)", ret);
        PR_INFO("This suggests the device is not responding to any commands");
        PR_INFO("Check hardware connections and power supply");
        return ret;
    }
    
    PR_INFO("Power control write successful, waiting for power-up...");
    tal_system_sleep(20); // Wait for power-up
    
    // Now try to read chip ID
    PR_INFO("Attempting to read chip ID after power-on...");
    uint8_t chip_id;
    ret = bmm150_i2c_read_register(addr, BMM150_CHIP_ID_ADDR, &chip_id, 1);
    
    if (ret == OPRT_OK) {
        PR_INFO("✓ Chip ID read successful: 0x%02X", chip_id);
        
        if (chip_id == BMM150_CHIP_ID) {
            PR_INFO("✓ BMM150 chip ID verified: 0x%02X", chip_id);
            return OPRT_OK;
        } else {
            PR_INFO("⚠ Wrong chip ID: 0x%02X (expected: 0x%02X)", chip_id, BMM150_CHIP_ID);
            PR_INFO("  This might be a different device at address 0x%02X", addr);
            return OPRT_COM_ERROR;
        }
    } else {
        PR_ERR("Failed to read chip ID from address 0x%02X (error: %d)", addr, ret);
        PR_ERR("The device is still not responding after power-on attempt");
        PR_ERR("Possible issues:");
        PR_ERR("1. Hardware not connected properly");
        PR_ERR("2. Wrong I2C address (check SD0 pin)");
        PR_ERR("3. Power supply issues (check VCC and GND)");
        PR_ERR("4. CSB pin not connected to VCC");
        PR_ERR("5. Device may be damaged");
        return ret;
    }
}

// BMM150 I2C functions using dev_config.c (following GPS pattern)
OPERATE_RET bmm150_write_register(bmm150_dev_t *dev, uint8_t reg, uint8_t value) {
    if (!dev) return OPRT_COM_ERROR;
    return bmm150_i2c_write_register(dev->i2c_addr, reg, value);
}

OPERATE_RET bmm150_read_register(bmm150_dev_t *dev, uint8_t reg, uint8_t *buffer, uint8_t length) {
    if (!dev || !buffer || length == 0) return OPRT_COM_ERROR;
    return bmm150_i2c_read_register(dev->i2c_addr, reg, buffer, length);
}

// ============================================================================
// CALIBRATION FUNCTIONS (Figure-8 Calibration Algorithm)
// ============================================================================

/**
 * @brief Start figure-8 calibration process
 * @param dev BMM150 device structure
 * @param timeout_ms Calibration timeout in milliseconds
 * @return OPERATE_RET
 */
OPERATE_RET bmm150_start_calibration(bmm150_dev_t *dev, uint32_t timeout_ms) {
    if (!dev) return OPRT_COM_ERROR;
    
    PR_INFO("Starting figure-8 calibration for %d seconds...", timeout_ms / 1000);
    PR_INFO("Move the sensor in figure-8 patterns during calibration!");
    
    // Initialize calibration data
    dev->calibration.calibrated = false;
    dev->calibration.calibration_time = tal_system_get_millisecond();
    
    // Read initial values to set min/max
    OPERATE_RET ret = bmm150_read_mag_data(dev);
    if (ret != OPRT_OK) {
        PR_ERR("Failed to read initial data for calibration");
        return ret;
    }
    
    // Initialize min/max values
    dev->calibration.x_min = dev->raw_mag_data.raw_datax;
    dev->calibration.x_max = dev->raw_mag_data.raw_datax;
    dev->calibration.y_min = dev->raw_mag_data.raw_datay;
    dev->calibration.y_max = dev->raw_mag_data.raw_datay;
    dev->calibration.z_min = dev->raw_mag_data.raw_dataz;
    dev->calibration.z_max = dev->raw_mag_data.raw_dataz;
    
    return OPRT_OK;
}

/**
 * @brief Update calibration with current reading
 * @param dev BMM150 device structure
 * @return OPERATE_RET
 */
OPERATE_RET bmm150_update_calibration(bmm150_dev_t *dev) {
    if (!dev) return OPRT_COM_ERROR;
    
    // Update X-axis min/max
    if (dev->raw_mag_data.raw_datax < dev->calibration.x_min) {
        dev->calibration.x_min = dev->raw_mag_data.raw_datax;
    } else if (dev->raw_mag_data.raw_datax > dev->calibration.x_max) {
        dev->calibration.x_max = dev->raw_mag_data.raw_datax;
    }
    
    // Update Y-axis min/max
    if (dev->raw_mag_data.raw_datay < dev->calibration.y_min) {
        dev->calibration.y_min = dev->raw_mag_data.raw_datay;
    } else if (dev->raw_mag_data.raw_datay > dev->calibration.y_max) {
        dev->calibration.y_max = dev->raw_mag_data.raw_datay;
    }
    
    // Update Z-axis min/max
    if (dev->raw_mag_data.raw_dataz < dev->calibration.z_min) {
        dev->calibration.z_min = dev->raw_mag_data.raw_dataz;
    } else if (dev->raw_mag_data.raw_dataz > dev->calibration.z_max) {
        dev->calibration.z_max = dev->raw_mag_data.raw_dataz;
    }
    
    return OPRT_OK;
}

/**
 * @brief Apply calibration offsets to magnetometer data
 * @param dev BMM150 device structure
 * @return OPERATE_RET
 */
OPERATE_RET bmm150_apply_calibration(bmm150_dev_t *dev) {
    if (!dev) return OPRT_COM_ERROR;
    
    if (!dev->calibration.calibrated) {
        return OPRT_OK; // No calibration applied
    }
    
    // Apply hard iron offset correction
    dev->mag_data.x = dev->mag_data.x - dev->calibration.x_offset;
    dev->mag_data.y = dev->mag_data.y - dev->calibration.y_offset;
    dev->mag_data.z = dev->mag_data.z - dev->calibration.z_offset;
    
    return OPRT_OK;
}

/**
 * @brief Check if sensor needs calibration
 * @param dev BMM150 device structure
 * @return true if calibration needed, false otherwise
 */
bool bmm150_needs_calibration(bmm150_dev_t *dev) {
    if (!dev) return true;
    
    // Check if never calibrated
    if (!dev->calibration.calibrated) {
        return true;
    }
    
    // Check if calibration is too old (24 hours)
    uint32_t current_time = tal_system_get_millisecond();
    if ((current_time - dev->calibration.calibration_time) > (24 * 60 * 60 * 1000)) {
        return true;
    }
    
    return false;
}

/**
 * @brief Detect magnetic drift
 * @param dev BMM150 device structure
 * @param drift_angle Output drift angle in degrees
 * @return OPERATE_RET
 */
OPERATE_RET bmm150_detect_drift(bmm150_dev_t *dev, float *drift_angle) {
    if (!dev || !drift_angle) return OPRT_COM_ERROR;
    
    // Calculate magnetic field strength
    float field_strength = sqrt(dev->mag_data.x * dev->mag_data.x + 
                               dev->mag_data.y * dev->mag_data.y + 
                               dev->mag_data.z * dev->mag_data.z);
    
    // Normal magnetic field strength is around 25-65 μT
    if (field_strength < 15.0f || field_strength > 100.0f) {
        *drift_angle = 999.0f; // Invalid reading
        return OPRT_COM_ERROR;
    }
    
    // Calculate heading stability (simplified drift detection)
    static float last_heading = 0.0f;
    static uint32_t last_time = 0;
    
    float current_heading = atan2(dev->mag_data.y, dev->mag_data.x);
    if (current_heading < 0) current_heading += 2 * 3.14159f;
    current_heading = current_heading * 180.0f / 3.14159f;
    
    uint32_t current_time = tal_system_get_millisecond();
    if (last_time > 0) {
        float heading_diff = fabs(current_heading - last_heading);
        if (heading_diff > 180.0f) heading_diff = 360.0f - heading_diff;
        
        *drift_angle = heading_diff;
    } else {
        *drift_angle = 0.0f;
    }
    
    last_heading = current_heading;
    last_time = current_time;
    
    return OPRT_OK;
}

// ============================================================================
// CLI CALIBRATION FUNCTIONS
// ============================================================================

/**
 * @brief CLI command to start calibration
 * @param dev BMM150 device structure
 * @param timeout_ms Calibration timeout in milliseconds
 * @return OPERATE_RET
 */
OPERATE_RET bmm150_cli_calibrate(bmm150_dev_t *dev, uint32_t timeout_ms) {
    if (!dev) return OPRT_COM_ERROR;
    
    PR_INFO("=== BMM150 Calibration CLI ===");
    PR_INFO("Starting figure-8 calibration for %d seconds...", timeout_ms / 1000);
    PR_INFO("Instructions:");
    PR_INFO("1. Hold the sensor in your hand");
    PR_INFO("2. Move it in figure-8 patterns");
    PR_INFO("3. Rotate it in all directions");
    PR_INFO("4. Keep moving until calibration completes");
    PR_INFO("Press any key to start...");
    
    // Start calibration
    OPERATE_RET ret = bmm150_start_calibration(dev, timeout_ms);
    if (ret != OPRT_OK) {
        PR_ERR("Failed to start calibration: %d", ret);
        return ret;
    }
    
    uint32_t cal_start = tal_system_get_millisecond();
    uint32_t last_progress = 0;
    
    while ((tal_system_get_millisecond() - cal_start) < timeout_ms) {
        ret = bmm150_read_mag_data(dev);
        if (ret == OPRT_OK) {
            bmm150_update_calibration(dev);
        }
        
        // Show progress every 2 seconds
        uint32_t elapsed = tal_system_get_millisecond() - cal_start;
        if (elapsed - last_progress >= 2000) {
            PR_INFO("Calibration progress: %d%% (%d/%d seconds)", 
                    (elapsed * 100) / timeout_ms, elapsed / 1000, timeout_ms / 1000);
            last_progress = elapsed;
        }
        
        tal_system_sleep(100);
    }
    
    // Calculate and apply offsets
    dev->calibration.x_offset = dev->calibration.x_min + (dev->calibration.x_max - dev->calibration.x_min) / 2;
    dev->calibration.y_offset = dev->calibration.y_min + (dev->calibration.y_max - dev->calibration.y_min) / 2;
    dev->calibration.z_offset = dev->calibration.z_min + (dev->calibration.z_max - dev->calibration.z_min) / 2;
    dev->calibration.calibrated = true;
    dev->calibration.calibration_time = tal_system_get_millisecond();
    
    PR_INFO("=== Calibration Complete ===");
    PR_INFO("X Offset: %d", dev->calibration.x_offset);
    PR_INFO("Y Offset: %d", dev->calibration.y_offset);
    PR_INFO("Z Offset: %d", dev->calibration.z_offset);
    PR_INFO("Calibration saved and applied!");
    
    return OPRT_OK;
}

/**
 * @brief CLI command to show calibration status
 * @param dev BMM150 device structure
 * @return OPERATE_RET
 */
OPERATE_RET bmm150_cli_cal_status(bmm150_dev_t *dev) {
    if (!dev) return OPRT_COM_ERROR;
    
    PR_INFO("=== BMM150 Calibration Status ===");
    PR_INFO("Calibrated: %s", dev->calibration.calibrated ? "YES" : "NO");
    
    if (dev->calibration.calibrated) {
        uint32_t current_time = tal_system_get_millisecond();
        uint32_t age_hours = (current_time - dev->calibration.calibration_time) / (1000 * 60 * 60);
        PR_INFO("Calibration Age: %d hours", age_hours);
        PR_INFO("X Offset: %d", dev->calibration.x_offset);
        PR_INFO("Y Offset: %d", dev->calibration.y_offset);
        PR_INFO("Z Offset: %d", dev->calibration.z_offset);
        PR_INFO("X Range: %d to %d", dev->calibration.x_min, dev->calibration.x_max);
        PR_INFO("Y Range: %d to %d", dev->calibration.y_min, dev->calibration.y_max);
        PR_INFO("Z Range: %d to %d", dev->calibration.z_min, dev->calibration.z_max);
        
        if (bmm150_needs_calibration(dev)) {
            PR_INFO("Status: RECALIBRATION NEEDED (age > 24h)");
        } else {
            PR_INFO("Status: CALIBRATION VALID");
        }
    } else {
        PR_INFO("Status: NOT CALIBRATED - Run calibration first!");
    }
    
    return OPRT_OK;
}

/**
 * @brief CLI command to reset calibration
 * @param dev BMM150 device structure
 * @return OPERATE_RET
 */
OPERATE_RET bmm150_cli_reset_calibration(bmm150_dev_t *dev) {
    if (!dev) return OPRT_COM_ERROR;
    
    PR_INFO("=== Resetting BMM150 Calibration ===");
    
    // Reset calibration data
    dev->calibration.calibrated = false;
    dev->calibration.x_offset = 0;
    dev->calibration.y_offset = 0;
    dev->calibration.z_offset = 0;
    dev->calibration.calibration_time = 0;
    dev->calibration.x_min = dev->calibration.x_max = 0;
    dev->calibration.y_min = dev->calibration.y_max = 0;
    dev->calibration.z_min = dev->calibration.z_max = 0;
    
    PR_INFO("Calibration data reset successfully!");
    PR_INFO("Run calibration command to recalibrate the sensor.");
    
    return OPRT_OK;
}

/**
 * @brief CLI command to show current offsets
 * @param dev BMM150 device structure
 * @return OPERATE_RET
 */
OPERATE_RET bmm150_cli_show_offsets(bmm150_dev_t *dev) {
    if (!dev) return OPRT_COM_ERROR;
    
    PR_INFO("=== BMM150 Current Offsets ===");
    
    if (dev->calibration.calibrated) {
        PR_INFO("X Offset: %d μT", dev->calibration.x_offset);
        PR_INFO("Y Offset: %d μT", dev->calibration.y_offset);
        PR_INFO("Z Offset: %d μT", dev->calibration.z_offset);
        
        // Show current raw readings
        OPERATE_RET ret = bmm150_read_mag_data(dev);
        if (ret == OPRT_OK) {
            PR_INFO("Current Raw Readings:");
            PR_INFO("  X: %d (compensated: %d)", dev->raw_mag_data.raw_datax, dev->mag_data.x);
            PR_INFO("  Y: %d (compensated: %d)", dev->raw_mag_data.raw_datay, dev->mag_data.y);
            PR_INFO("  Z: %d (compensated: %d)", dev->raw_mag_data.raw_dataz, dev->mag_data.z);
        }
    } else {
        PR_INFO("No calibration data available!");
        PR_INFO("Run 'calibrate' command to calibrate the sensor.");
    }
    
    return OPRT_OK;
}

/**
 * @brief CLI command for manual calibration with user input
 * @param dev BMM150 device structure
 * @return OPERATE_RET
 */
OPERATE_RET bmm150_cli_manual_calibration(bmm150_dev_t *dev) {
    if (!dev) return OPRT_COM_ERROR;
    
    PR_INFO("=== BMM150 Manual Calibration ===");
    PR_INFO("This will start a 15-second calibration process.");
    PR_INFO("Move the sensor in figure-8 patterns during calibration.");
    PR_INFO("Starting in 3 seconds...");
    
    tal_system_sleep(1000);
    PR_INFO("2...");
    tal_system_sleep(1000);
    PR_INFO("1...");
    tal_system_sleep(1000);
    PR_INFO("GO! Move the sensor now!");
    
    return bmm150_cli_calibrate(dev, 15000); // 15 second calibration
}

// ============================================================================
// LIVE CALIBRATION FUNCTIONS (Real-time interference detection and compensation)
// ============================================================================

/**
 * @brief Initialize live calibration system
 * @param dev BMM150 device structure
 * @return OPERATE_RET
 */
OPERATE_RET bmm150_live_cal_init(bmm150_dev_t *dev) {
    if (!dev) return OPRT_COM_ERROR;
    
    // Initialize live calibration data
    dev->live_cal.x_offset_live = 0.0f;
    dev->live_cal.y_offset_live = 0.0f;
    dev->live_cal.z_offset_live = 0.0f;
    dev->live_cal.x_max_live = -32768.0f;
    dev->live_cal.x_min_live = 32767.0f;
    dev->live_cal.y_max_live = -32768.0f;
    dev->live_cal.y_min_live = 32767.0f;
    dev->live_cal.z_max_live = -32768.0f;
    dev->live_cal.z_min_live = 32767.0f;
    dev->live_cal.sample_count = 0;
    dev->live_cal.field_strength_avg = 0.0f;
    dev->live_cal.turbulence_detected = false;
    dev->live_cal.last_update_time = tal_system_get_millisecond();
    
    PR_INFO("Live calibration system initialized");
    return OPRT_OK;
}

/**
 * @brief Update live calibration with current reading
 * @param dev BMM150 device structure
 * @return OPERATE_RET
 */
OPERATE_RET bmm150_live_cal_update(bmm150_dev_t *dev) {
    if (!dev) return OPRT_COM_ERROR;
    
    float x = (float)dev->raw_mag_data.raw_datax;
    float y = (float)dev->raw_mag_data.raw_datay;
    float z = (float)dev->raw_mag_data.raw_dataz;
    
    // Update min/max values
    if (x > dev->live_cal.x_max_live) dev->live_cal.x_max_live = x;
    if (x < dev->live_cal.x_min_live) dev->live_cal.x_min_live = x;
    if (y > dev->live_cal.y_max_live) dev->live_cal.y_max_live = y;
    if (y < dev->live_cal.y_min_live) dev->live_cal.y_min_live = y;
    if (z > dev->live_cal.z_max_live) dev->live_cal.z_max_live = z;
    if (z < dev->live_cal.z_min_live) dev->live_cal.z_min_live = z;
    
    // Calculate magnetic field strength
    float field_strength = sqrt(x*x + y*y + z*z);
    
    // Update average field strength with exponential moving average
    if (dev->live_cal.sample_count == 0) {
        dev->live_cal.field_strength_avg = field_strength;
    } else {
        dev->live_cal.field_strength_avg = 0.9f * dev->live_cal.field_strength_avg + 0.1f * field_strength;
    }
    
    dev->live_cal.sample_count++;
    
    // Update live offsets using dynamic zero drift compensation algorithm
    // offset_x(k) = α * (max(X_k) + min(X_k)) / 2 + (1-α) * offset_x(k-1)
    float alpha = LIVE_CAL_ALPHA;
    float new_x_offset = alpha * (dev->live_cal.x_max_live + dev->live_cal.x_min_live) / 2.0f + 
                        (1.0f - alpha) * dev->live_cal.x_offset_live;
    float new_y_offset = alpha * (dev->live_cal.y_max_live + dev->live_cal.y_min_live) / 2.0f + 
                        (1.0f - alpha) * dev->live_cal.y_offset_live;
    float new_z_offset = alpha * (dev->live_cal.z_max_live + dev->live_cal.z_min_live) / 2.0f + 
                        (1.0f - alpha) * dev->live_cal.z_offset_live;
    
    dev->live_cal.x_offset_live = new_x_offset;
    dev->live_cal.y_offset_live = new_y_offset;
    dev->live_cal.z_offset_live = new_z_offset;
    
    dev->live_cal.last_update_time = tal_system_get_millisecond();
    
    return OPRT_OK;
}

/**
 * @brief Detect magnetic turbulence
 * @param dev BMM150 device structure
 * @return true if turbulence detected, false otherwise
 */
bool bmm150_live_cal_detect_turbulence(bmm150_dev_t *dev) {
    if (!dev) return false;
    
    float x = (float)dev->raw_mag_data.raw_datax;
    float y = (float)dev->raw_mag_data.raw_datay;
    float z = (float)dev->raw_mag_data.raw_dataz;
    float field_strength = sqrt(x*x + y*y + z*z);
    
    // Detect turbulence based on field strength deviation
    float strength_deviation = fabs(field_strength - dev->live_cal.field_strength_avg);
    
    if (strength_deviation > LIVE_CAL_TURBULENCE_THRESHOLD) {
        dev->live_cal.turbulence_detected = true;
        return true;
    } else {
        dev->live_cal.turbulence_detected = false;
        return false;
    }
}

/**
 * @brief Apply live calibration offsets to magnetometer data
 * @param dev BMM150 device structure
 * @param value Output calibrated data
 * @return OPERATE_RET
 */
OPERATE_RET bmm150_live_cal_apply_offsets(bmm150_dev_t *dev, bmm150_mag_data_t *value) {
    if (!dev || !value) return OPRT_COM_ERROR;
    
    // Apply live calibration offsets
    value->x = (int16_t)(dev->raw_mag_data.raw_datax - dev->live_cal.x_offset_live);
    value->y = (int16_t)(dev->raw_mag_data.raw_datay - dev->live_cal.y_offset_live);
    value->z = (int16_t)(dev->raw_mag_data.raw_dataz - dev->live_cal.z_offset_live);
    
    return OPRT_OK;
}

/**
 * @brief Get live calibration status
 * @param dev BMM150 device structure
 * @return 1 for turbulence/calibration needed, 0 for working
 */
uint8_t bmm150_live_cal_get_status(bmm150_dev_t *dev) {
    if (!dev) return 1;
    
    // Return 1 if turbulence detected or insufficient samples
    if (dev->live_cal.turbulence_detected || dev->live_cal.sample_count < LIVE_CAL_MIN_SAMPLES) {
        return 1;
    }
    
    return 0;  // Working normally
}