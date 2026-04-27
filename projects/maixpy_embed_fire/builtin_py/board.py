try:
    import ujson as json
except ImportError:
    import json

config = json.loads("""
{
    "type": "embed_fire",
    "lcd": {
        "dcx": 27,
        "ss": 28,
        "rst": 29,
        "clk": 26,
        "height": 240,
        "width": 320,
        "invert": 0,
        "dir": 96,
        "lcd_type": 0
    },
    "sensor": {
        "pin_sda": 24,
        "cmos_href": 20,
        "cmos_pclk": 23,
        "cmos_xclk": 22,
        "cmos_pwdn": 21,
        "cmos_vsync": 18,
        "reg_width": 8,
        "i2c_num": 2,
        "pin_clk": 25,
        "cmos_rst": 19
    },
    "sdcard": {
        "sclk": 8,
        "mosi": 7,
        "miso": 9,
        "cs": 6
    },
    "board_info": {
        "BOOT_KEY": 16,
        "LED_G": 17,
        "CONNEXT_A": 1,
        "CONNEXT_B": 0,
        "I2C_SDA": 12,
        "I2C_SCL": 13,
        "SPI_SCLK": 8,
        "SPI_MOSI": 7,
        "SPI_MISO": 9,
        "SPI_CS": 6
    },
    "krux": {
        "pins": {
            "BUTTON_B": 16,
            "TOUCH_IRQ": 15,
            "I2C_SDA": 12,
            "I2C_SCL": 13
        },
        "display": {
            "touch": true,
            "font": [8, 16],
            "font_wide": [16, 16]
        }
    }
}
""")
