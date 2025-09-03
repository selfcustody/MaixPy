try:
    import ujson as json
except ImportError:
    import json

config = json.loads("""
{
    "type": "tzt",
    "sensor": {
        "pin_sda": 40,
        "cmos_href": 44,
        "cmos_pclk": 47,
        "cmos_xclk": 46,
        "cmos_pwdn": 45,
        "cmos_vsync": 42,
        "reg_width": 16,
        "i2c_num": 2,
        "pin_clk": 41,
        "cmos_rst": 43
    },
    "sdcard": {
        "cs": 32,
        "cs_gpio": 29,
        "sclk": 34,
        "mosi": 33,
        "miso": 35
    },
    "lcd": {
        "offset_y2": 0,
        "dir": 96,
        "dcx": 37,
        "invert": 1,
        "lcd_type": 0,
        "offset_y1": 0,
        "ss": 38,
        "offset_x1": 0,
        "offset_x2": 0,
        "rst": 39,
        "clk": 36,
        "height": 240,
        "width": 320
    },
    "board_info": {
        "BOOT_KEY": 16,
        "CONNEXT_A": 8,
        "CONNEXT_B": 7,
        "I2C_SDA": 40,
        "I2C_SCL": 41,
        "SPI_SCLK": 34,
        "SPI_MOSI": 33,
        "SPI_MISO": 35,
        "SPI_CS": 32
    },
    "krux": {
        "pins": {
            "BUTTON_C": 11,
            "BUTTON_B": 12,
            "BUTTON_A": 16,
            "BUTTON_X": 13,
            "TOUCH_IRQ": 22,
            "I2C_SDA": 25,
            "I2C_SCL": 24
        },
        "display": {
            "touch": true,
            "font": [8, 16],
            "font_wide": [16, 16]
        }
    }
}
""")
