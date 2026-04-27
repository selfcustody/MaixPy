try:
    import ujson as json
except ImportError:
    import json

config = json.loads("""
{
    "type": "wonder_k",
    "lcd": {
        "dcx": 37,
        "ss": 38,
        "rst": 36,
        "clk": 39,
        "height": 240,
        "width": 320,
        "invert": 0,
        "dir": 160,
        "lcd_type": 0
    },
    "sensor": {
        "pin_sda": 40,
        "cmos_href": 45,
        "cmos_pclk": 47,
        "cmos_xclk": 46,
        "cmos_pwdn": 44,
        "cmos_vsync": 43,
        "reg_width": 16,
        "i2c_num": 2,
        "pin_clk": 41,
        "cmos_rst": 42
    },
    "sdcard": {
        "sclk": 32,
        "mosi": 33,
        "miso": 35,
        "cs": 30
    },
    "board_info": {
        "BOOT_KEY": 16,
        "CONNEXT_A": 6,
        "CONNEXT_B": 8,
        "I2C_SDA": 27,
        "I2C_SCL": 24,
        "SPI_SCLK": 32,
        "SPI_MOSI": 33,
        "SPI_MISO": 35,
        "SPI_CS": 30
    },
    "krux": {
        "pins": {
            "BUTTON_A": 25,
            "BUTTON_B": 31,
            "TOUCH_IRQ": 26,
            "TOUCH_RESET": 29,
            "I2C_SDA": 27,
            "I2C_SCL": 24,
            "BACKLIGHT": 22
        },
        "display": {
            "touch": true,
            "font": [8, 16],
            "font_wide": [16, 16]
        }
    }
}
""")
