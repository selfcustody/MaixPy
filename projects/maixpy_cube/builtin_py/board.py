try:
    import ujson as json
except ImportError:
    import json

config = json.loads("""
{
    "type": "cube",
    "lcd": {
        "height": 240,
        "width": 240,
        "invert": 1,
        "lcd_type": 0
    },
    "sdcard": {
        "sclk": 27,
        "mosi": 28,
        "miso": 26,
        "cs": 29
    },
    "board_info": {
        "BOOT_KEY": 16,
        "CONNEXT_A": 25,
        "CONNEXT_B": 24,
        "LED_R": 13,
        "LED_G": 12,
        "LED_B": 14,
        "LED_W": 32,
        "MIC0_WS": 19,
        "MIC0_DATA": 20,
        "MIC0_BCK": 18
    },
    "krux": {
        "pins": {
            "BUTTON_A": 10,
            "BUTTON_B": 16,
            "BUTTON_C": 11,
            "LED_W": 32,
            "I2C_SDA": 31,
            "I2C_SCL": 30,
            "BACKLIGHT": 17
        },
        "display": {
            "touch": false,
            "font": [8, 14],
            "font_wide": [14, 14]
        }
    }
}
""")
