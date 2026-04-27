#include <stdint.h>

static struct sensor_config_t {
    uint8_t cmos_pclk;
    uint8_t cmos_xclk;
    uint8_t cmos_href;
    uint8_t cmos_pwdn;
    uint8_t cmos_vsync;
    uint8_t cmos_rst;

    uint8_t reg_width;
    uint8_t i2c_num;
    uint8_t pin_clk;
    uint8_t pin_sda;
    uint8_t gpio_clk;
    uint8_t gpio_sda;
} sensor_config = {
    23, 22, 20, 21, 18, 19,
    8, 2, 25, 24, 0, 0
};

