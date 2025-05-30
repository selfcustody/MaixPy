#include "py/runtime.h"
#include "py/builtin.h"
#include "py/obj.h"
#include "py/objstr.h"
#include <string.h>
#include "py/mperrno.h"

#define BASE32_MASK 0x1F

static const char B32CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
static int8_t base32_index[256];

static void init_base32_index(void) {
    memset(base32_index, -1, sizeof(base32_index));
    for (int i = 0; i < 32; i++) {
        char c = B32CHARS[i];
        base32_index[(unsigned char)c] = i;
    }
}

STATIC mp_obj_t base32_decode(mp_obj_t encoded_str_obj) {
    size_t encoded_len;
    const char *encoded_str = mp_obj_str_get_data(encoded_str_obj, &encoded_len);

    // Strip padding
    size_t stripped_len = encoded_len;
    while (stripped_len > 0 && encoded_str[stripped_len - 1] == '=') {
        stripped_len--;
    }

    static bool initialized = false;
    if (!initialized) {
        init_base32_index();
        initialized = true;
    }

    size_t max_decoded_size = (stripped_len * 5 + 7) / 8;
    uint8_t *decoded_buf = m_new0(uint8_t, max_decoded_size);
    
    if (decoded_buf == NULL) {
        mp_raise_OSError(MP_ENOMEM);
    }

    size_t decoded_len = 0;
    uint32_t buffer = 0;
    int bits_left = 0;

    for (size_t i = 0; i < stripped_len; i++) {
        unsigned char c = encoded_str[i];
        int8_t index = base32_index[c];
        if (index == -1) {
            // Clean up allocated memory before raising exception
            m_del(uint8_t, decoded_buf, max_decoded_size);
            mp_raise_ValueError("Invalid Base32 character");
        }

        buffer = (buffer << 5) | index;
        bits_left += 5;

        while (bits_left >= 8) {
            bits_left -= 8;
            decoded_buf[decoded_len++] = (buffer >> bits_left) & 0xFF;
            buffer &= (1 << bits_left) - 1;
        }
    }

    mp_obj_t result = mp_obj_new_bytes(decoded_buf, decoded_len);
    
    // Clean up allocated memory
    m_del(uint8_t, decoded_buf, max_decoded_size);
    
    return result;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(base32_decode_obj, base32_decode);

STATIC mp_obj_t base32_encode(mp_obj_t data_obj, mp_obj_t add_padding_obj) {
    mp_buffer_info_t data_buf;
    mp_get_buffer_raise(data_obj, &data_buf, MP_BUFFER_READ);
    uint8_t *data = data_buf.buf;
    size_t data_len = data_buf.len;

    bool add_padding = mp_obj_is_true(add_padding_obj);

    uint32_t buffer = 0;
    int bits_left = 0;

    size_t base_encoded_size = (data_len * 8 + 4) / 5;
    size_t max_encoded_size;
    
    if (add_padding) {
        // Round up to nearest multiple of 8 for padding
        max_encoded_size = ((base_encoded_size + 7) / 8) * 8;
    } else {
        max_encoded_size = base_encoded_size;
    }
    
    char *encoded_buf = m_new0(char, max_encoded_size + 1);
    
    if (encoded_buf == NULL) {
        mp_raise_OSError(MP_ENOMEM);
    }

    size_t encoded_len = 0;

    for (size_t i = 0; i < data_len; i++) {
        buffer = (buffer << 8) | data[i];
        bits_left += 8;

        while (bits_left >= 5) {
            bits_left -= 5;
            uint8_t index = (buffer >> bits_left) & BASE32_MASK;
            encoded_buf[encoded_len++] = B32CHARS[index];
            buffer &= (1 << bits_left) - 1;
        }
    }

    if (bits_left > 0) {
        buffer <<= (5 - bits_left);
        encoded_buf[encoded_len++] = B32CHARS[buffer & BASE32_MASK];
    }

    if (add_padding) {
        size_t padding_length = (8 - (encoded_len % 8)) % 8;
        for (size_t i = 0; i < padding_length; i++) {
            encoded_buf[encoded_len++] = '=';
        }
    }

    mp_obj_t result = mp_obj_new_str(encoded_buf, encoded_len);
    
    // Clean up allocated memory
    m_del(char, encoded_buf, max_encoded_size + 1);
    
    return result;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(base32_encode_obj, base32_encode);

STATIC const mp_rom_map_elem_t base32_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_base32) },
    { MP_ROM_QSTR(MP_QSTR_decode), MP_ROM_PTR(&base32_decode_obj) },
    { MP_ROM_QSTR(MP_QSTR_encode), MP_ROM_PTR(&base32_encode_obj) },
};
STATIC MP_DEFINE_CONST_DICT(base32_module_globals, base32_module_globals_table);

const mp_obj_module_t base32_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&base32_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_base32, base32_user_cmodule, MODULE_BASE32_ENABLED);