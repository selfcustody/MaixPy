try:
    import ujson as json
except ImportError:
    import json

config = json.loads("""
{
    "type": "wonder_mv",
    "lcd": {
        "dcx": 8,
        "ss": 6,
        "rst": 21,
        "clk": 7,
        "height": 240,
        "width": 320,
        "invert": 1,
        "dir": 96,
        "lcd_type": 0
    },
    "sensor": {
        "pin_sda": 9,
        "cmos_href": 17,
        "cmos_pclk": 15,
        "cmos_xclk": 14,
        "cmos_pwdn": 13,
        "cmos_vsync": 12,
        "reg_width": 16,
        "i2c_num": 2,
        "pin_clk": 10,
        "cmos_rst": 11
    },
    "sdcard": {
        "sclk": 29,
        "mosi": 30,
        "miso": 31,
        "cs": 32
    },
    "board_info": {
        "BOOT_KEY": 16,
        "CONNEXT_A": 27,
        "CONNEXT_B": 28,
        "I2C_SDA": 19,
        "I2C_SCL": 18,
        "SPI_SCLK": 29,
        "SPI_MOSI": 30,
        "SPI_MISO": 31,
        "SPI_CS": 32
    },
    "krux": {
        "pins": {
            "BUTTON_A": 26,
            "BUTTON_B": 47,
            "LED_W": 25,
            "TOUCH_IRQ": 41,
            "I2C_SDA": 19,
            "I2C_SCL": 18,
            "BACKLIGHT": 23
        },
        "display": {
            "touch": true,
            "font": [8, 16],
            "font_wide": [16, 16]
        }
    }
}
""")
