#ifndef BMM150_H
#define BMM150_H

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "dev_config.h"

// Software I2C Pin Definitions - using P14 and P15
#define BMM150_I2C_SCL_PIN      TUYA_GPIO_NUM_14
#define BMM150_I2C_SDA_PIN      TUYA_GPIO_NUM_15

// Software I2C Timing Constants
#define BMM150_I2C_DELAY_US     10
#define BMM150_I2C_TIMEOUT_MS  100

// BMM150 I2C Address
#define BMM150_ADDRESS 0x10

// Live Calibration Configuration
#define ENABLE_LIVE_CALIBRATION 1  // Set to 1 to enable live calibration, 0 to disable

// Live Calibration Parameters
#define LIVE_CAL_ALPHA 0.1f         // Forgetting factor (0.05-0.2)
#define LIVE_CAL_TURBULENCE_THRESHOLD 50.0f  // Magnetic field strength threshold for turbulence detection
#define LIVE_CAL_MIN_SAMPLES 10     // Minimum samples for calibration update

// BMM150 Register Map (from Grove driver)
#define BMM150_CHIP_ID_ADDR          0x40
#define BMM150_DATA_X_LSB            0x42
#define BMM150_DATA_X_MSB            0x43
#define BMM150_DATA_Y_LSB            0x44
#define BMM150_DATA_Y_MSB            0x45
#define BMM150_DATA_Z_LSB            0x46
#define BMM150_DATA_Z_MSB            0x47
#define BMM150_DATA_READY_STATUS     0x48
#define BMM150_INTERRUPT_STATUS      0x4A
#define BMM150_POWER_CONTROL_ADDR    0x4B
#define BMM150_OP_MODE_ADDR          0x4C
#define BMM150_INT_CONFIG_ADDR       0x4D
#define BMM150_AXES_ENABLE_ADDR      0x4E
#define BMM150_LOW_THRESHOLD_ADDR    0x4F
#define BMM150_HIGH_THRESHOLD_ADDR   0x50
#define BMM150_REP_XY_ADDR           0x51
#define BMM150_REP_Z_ADDR            0x52

// Trim Registers
#define BMM150_DIG_X1                0x5D
#define BMM150_DIG_Y1                0x5E
#define BMM150_DIG_Z4_LSB            0x62
#define BMM150_DIG_Z4_MSB            0x63
#define BMM150_DIG_X2                0x64
#define BMM150_DIG_Y2                0x65
#define BMM150_DIG_Z2_LSB            0x68
#define BMM150_DIG_Z2_MSB            0x69
#define BMM150_DIG_Z1_LSB            0x6A
#define BMM150_DIG_Z1_MSB            0x6B
#define BMM150_DIG_XYZ1_LSB          0x6C
#define BMM150_DIG_XYZ1_MSB          0x6D
#define BMM150_DIG_Z3_LSB            0x6E
#define BMM150_DIG_Z3_MSB            0x6F
#define BMM150_DIG_XY2               0x70
#define BMM150_DIG_XY1               0x71

// BMM150 Chip ID
#define BMM150_CHIP_ID               0x32

// BMM150 Power Modes
#define BMM150_SLEEP_MODE            0x03
#define BMM150_NORMAL_MODE           0x00
#define BMM150_FORCED_MODE           0x01
#define BMM150_SUSPEND_MODE          0x04

// BMM150 Data Rates
#define BMM150_DATA_RATE_2HZ         0x01
#define BMM150_DATA_RATE_6HZ         0x02
#define BMM150_DATA_RATE_8HZ         0x03
#define BMM150_DATA_RATE_10HZ        0x00
#define BMM150_DATA_RATE_15HZ        0x04
#define BMM150_DATA_RATE_20HZ        0x05
#define BMM150_DATA_RATE_25HZ        0x06
#define BMM150_DATA_RATE_30HZ        0x07

// Preset Modes
#define BMM150_PRESETMODE_LOWPOWER   0x01
#define BMM150_PRESETMODE_REGULAR    0x02
#define BMM150_PRESETMODE_HIGHACCURACY 0x03
#define BMM150_PRESETMODE_ENHANCED   0x04

// Power Control
#define BMM150_POWER_CNTRL_DISABLE   0x00
#define BMM150_POWER_CNTRL_ENABLE    0x01
#define BMM150_SET_SOFT_RESET        0x82

// Timing Constants
#define BMM150_START_UP_TIME         3
#define BMM150_SOFT_RESET_DELAY      1

// Data Length Constants
#define BMM150_XYZR_DATA_LEN         8

// Overflow Definitions
#define BMM150_XYAXES_FLIP_OVERFLOW_ADCVAL  (-4096)
#define BMM150_ZAXIS_HALL_OVERFLOW_ADCVAL  (-16384)
#define BMM150_OVERFLOW_OUTPUT             (-32768)
#define BMM150_NEGATIVE_SATURATION_Z       (-32767)
#define BMM150_POSITIVE_SATURATION_Z       (32767)

// Bit Manipulation Macros
#define BMM150_SET_BITS(reg_data, bitname, data) \
    ((reg_data & ~(bitname##_MSK)) | \
     ((data << bitname##_POS) & bitname##_MSK))

#define BMM150_GET_BITS(reg_data, bitname)  ((reg_data & (bitname##_MSK)) >> \
        (bitname##_POS))

#define BMM150_SET_BITS_POS_0(reg_data, bitname, data) \
    ((reg_data & ~(bitname##_MSK)) | \
     (data & bitname##_MSK))

#define BMM150_GET_BITS_POS_0(reg_data, bitname)  (reg_data & (bitname##_MSK))

// Bit Masks and Positions
#define BMM150_PWR_CNTRL_MSK         (0x01)
#define BMM150_PWR_CNTRL_POS         (0x00)

#define BMM150_OP_MODE_MSK           (0x06)
#define BMM150_OP_MODE_POS           (0x01)

#define BMM150_ODR_MSK               (0x38)
#define BMM150_ODR_POS               (0x03)

#define BMM150_DATA_X_MSK            (0xF8)
#define BMM150_DATA_X_POS            (0x03)

#define BMM150_DATA_Y_MSK            (0xF8)
#define BMM150_DATA_Y_POS            (0x03)

#define BMM150_DATA_Z_MSK            (0xFE)
#define BMM150_DATA_Z_POS            (0x01)

#define BMM150_DATA_RHALL_MSK        (0xFC)
#define BMM150_DATA_RHALL_POS        (0x02)

// Preset Mode Repetitions
#define BMM150_LOWPOWER_REPXY        (1)
#define BMM150_REGULAR_REPXY         (4)
#define BMM150_ENHANCED_REPXY        (7)
#define BMM150_HIGHACCURACY_REPXY    (23)

#define BMM150_LOWPOWER_REPZ         (2)
#define BMM150_REGULAR_REPZ          (14)
#define BMM150_ENHANCED_REPZ         (26)
#define BMM150_HIGHACCURACY_REPZ     (82)

// Data Structures (from Grove driver)
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} bmm150_mag_data_t;

// Calibration data structure
typedef struct {
    int16_t x_offset;
    int16_t y_offset;
    int16_t z_offset;
    int16_t x_min, x_max;
    int16_t y_min, y_max;
    int16_t z_min, z_max;
    bool calibrated;
    uint32_t calibration_time;
} bmm150_calibration_t;

// Live calibration data structure for real-time interference detection
typedef struct {
    float x_offset_live;      // Live X offset
    float y_offset_live;      // Live Y offset  
    float z_offset_live;      // Live Z offset
    float x_max_live;         // Live X maximum
    float x_min_live;         // Live X minimum
    float y_max_live;         // Live Y maximum
    float y_min_live;         // Live Y minimum
    float z_max_live;         // Live Z maximum
    float z_min_live;         // Live Z minimum
    uint32_t sample_count;    // Number of samples collected
    float field_strength_avg; // Average magnetic field strength
    bool turbulence_detected; // Turbulence detection flag
    uint32_t last_update_time; // Last update timestamp
} bmm150_live_cal_t;

typedef struct {
    int16_t raw_datax;
    int16_t raw_datay;
    int16_t raw_dataz;
    uint16_t raw_data_r;
} bmm150_raw_mag_data_t;

typedef struct {
    int8_t dig_x1;
    int8_t dig_y1;
    int8_t dig_x2;
    int8_t dig_y2;
    uint16_t dig_z1;
    int16_t dig_z2;
    int16_t dig_z3;
    int16_t dig_z4;
    uint8_t dig_xy1;
    int8_t dig_xy2;
    uint16_t dig_xyz1;
} bmm150_trim_registers_t;

typedef struct {
    uint8_t xyz_axes_control;
    uint8_t pwr_cntrl_bit;
    uint8_t pwr_mode;
    uint8_t data_rate;
    uint8_t xy_rep;
    uint8_t z_rep;
    uint8_t preset_mode;
} bmm150_settings_t;

typedef struct {
    uint8_t i2c_addr;
    bmm150_settings_t settings;
    bmm150_raw_mag_data_t raw_mag_data;
    bmm150_mag_data_t mag_data;
    bmm150_trim_registers_t trim_data;
    bmm150_calibration_t calibration;
    bmm150_live_cal_t live_cal;  // Live calibration data
} bmm150_dev_t;

// Function Prototypes
OPERATE_RET bmm150_init(bmm150_dev_t *dev, uint8_t i2c_addr);
OPERATE_RET bmm150_read_mag_data(bmm150_dev_t *dev);
OPERATE_RET bmm150_set_preset_mode(bmm150_dev_t *dev, uint8_t preset_mode);
OPERATE_RET bmm150_set_op_mode(bmm150_dev_t *dev, uint8_t op_mode);
OPERATE_RET bmm150_soft_reset(bmm150_dev_t *dev);
OPERATE_RET bmm150_read_trim_registers(bmm150_dev_t *dev);

// I2C functions using dev_config.c (following GPS codebase pattern)
OPERATE_RET bmm150_i2c_init(void);
OPERATE_RET bmm150_i2c_write_register(uint8_t addr, uint8_t reg, uint8_t value);
OPERATE_RET bmm150_i2c_read_register(uint8_t addr, uint8_t reg, uint8_t *buffer, uint8_t length);

// BMM150 I2C functions
OPERATE_RET bmm150_write_register(bmm150_dev_t *dev, uint8_t reg, uint8_t value);
OPERATE_RET bmm150_read_register(bmm150_dev_t *dev, uint8_t reg, uint8_t *buffer, uint8_t length);

// Compensation functions
int16_t bmm150_compensate_x(bmm150_dev_t *dev, int16_t mag_data_x, uint16_t data_rhall);
int16_t bmm150_compensate_y(bmm150_dev_t *dev, int16_t mag_data_y, uint16_t data_rhall);
int16_t bmm150_compensate_z(bmm150_dev_t *dev, int16_t mag_data_z, uint16_t data_rhall);

// Helper functions
OPERATE_RET bmm150_write_op_mode(bmm150_dev_t *dev, uint8_t op_mode);
OPERATE_RET bmm150_set_power_control_bit(bmm150_dev_t *dev, uint8_t pwrcntrl_bit);
void bmm150_suspend_to_sleep_mode(bmm150_dev_t *dev);
OPERATE_RET bmm150_set_odr(bmm150_dev_t *dev, bmm150_settings_t settings);
OPERATE_RET bmm150_set_xy_rep(bmm150_dev_t *dev, bmm150_settings_t settings);
OPERATE_RET bmm150_set_z_rep(bmm150_dev_t *dev, bmm150_settings_t settings);

// I2C Bus Scanner
OPERATE_RET bmm150_scan_i2c_bus(bmm150_dev_t *dev);

// Test Functions
OPERATE_RET bmm150_test_chip_id(uint8_t addr);
OPERATE_RET bmm150_test_i2c_port(void);

// Calibration Functions
OPERATE_RET bmm150_start_calibration(bmm150_dev_t *dev, uint32_t timeout_ms);
OPERATE_RET bmm150_update_calibration(bmm150_dev_t *dev);
OPERATE_RET bmm150_apply_calibration(bmm150_dev_t *dev);
bool bmm150_needs_calibration(bmm150_dev_t *dev);
OPERATE_RET bmm150_detect_drift(bmm150_dev_t *dev, float *drift_angle);

// CLI Calibration Functions
OPERATE_RET bmm150_cli_calibrate(bmm150_dev_t *dev, uint32_t timeout_ms);
OPERATE_RET bmm150_cli_cal_status(bmm150_dev_t *dev);
OPERATE_RET bmm150_cli_reset_calibration(bmm150_dev_t *dev);
OPERATE_RET bmm150_cli_show_offsets(bmm150_dev_t *dev);
OPERATE_RET bmm150_cli_manual_calibration(bmm150_dev_t *dev);

// Live Calibration Functions (Real-time interference detection and compensation)
OPERATE_RET bmm150_live_cal_init(bmm150_dev_t *dev);
OPERATE_RET bmm150_live_cal_update(bmm150_dev_t *dev);
bool bmm150_live_cal_detect_turbulence(bmm150_dev_t *dev);
OPERATE_RET bmm150_live_cal_apply_offsets(bmm150_dev_t *dev, bmm150_mag_data_t *value);
uint8_t bmm150_live_cal_get_status(bmm150_dev_t *dev);  // Returns 1 for turbulence/calibration needed, 0 for working

#endif // BMM150_H