try:
    import ujson as json
except ImportError:
    import json

# todo: serial console not fully functioning for debugging and/or repl
# yahboom_devkit pretends to be "yahboom" so that krux code can enable common features
config = json.loads("""
{
    "type": "yahboom",
    "lcd": {
        "dcx": 38,
        "ss": 36,
        "rst": 37,
        "clk": 39,
        "height": 240,
        "width": 320,
        "invert": 1,
        "dir": 96,
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
        "CONNEXT_A": 4,
        "CONNEXT_B": 5,
        "I2C_SDA": 10,
        "I2C_SCL": 9,
        "SPI_SCLK": 27,
        "SPI_MOSI": 28,
        "SPI_MISO": 26,
        "SPI_CS": 29
    },
    "krux": {
        "pins": {
            "BUTTON_A": 2,
            "BUTTON_B": 3,
            "BUTTON_C": 1,
            "TOUCH_IRQ": 12,
            "I2C_SDA": 10,
            "I2C_SCL": 9
        },
        "display": {
            "touch": true,
            "font": [8, 16],
            "font_wide": [16, 16]
        }
    }
}
""")
