#include "py/runtime.h"
#include "py/builtin.h"
#include "py/obj.h"
#include "py/objstr.h"
#include <string.h>

static const char B43CHARS[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ$*+-./:";
static int8_t base43_index[256];

typedef struct {
    uint8_t *digits;
    size_t length;
} BigInt;

static void init_base43_index(void) {
    memset(base43_index, -1, sizeof(base43_index));
    for (int i = 0; i < 43; i++) {
        char c = B43CHARS[i];
        base43_index[(unsigned char)c] = i;
    }
}

// Big Integer functions
static BigInt bigint_from_bytes(const uint8_t *bytes, size_t len) {
    BigInt num;
    if (len == 0 || bytes == NULL) {
        num.digits = m_new(uint8_t, 1);
        num.digits[0] = 0;
        num.length = 1;
    } else {
        num.digits = m_new(uint8_t, len);
        memcpy(num.digits, bytes, len);
        num.length = len;
    }
    return num;
}

static void bigint_free(BigInt *num) {
    m_del(uint8_t, num->digits, num->length);
}

static BigInt bigint_divmod(BigInt *num, uint8_t divisor, uint8_t *remainder) {
    uint64_t tmp = 0;
    size_t new_len = 0;
    uint8_t *result = m_new(uint8_t, num->length);
    
    for (size_t i = 0; i < num->length; i++) {
        tmp = (tmp << 8) | num->digits[i];
        if (tmp >= divisor || new_len > 0) {
            result[new_len++] = tmp / divisor;
            tmp %= divisor;
        }
    }
    
    if (new_len == 0) {
        new_len = 1;
        result[0] = 0;
    }
    
    BigInt quotient;
    quotient.digits = result;
    quotient.length = new_len;
    *remainder = tmp;
    return quotient;
}


static void bigint_multiply_add(BigInt *num, uint8_t multiplier, uint8_t addend) {
    uint16_t carry = addend;
    
    for (size_t i = 0; i < num->length; i++) {
        uint16_t product = (uint16_t)num->digits[i] * multiplier + carry;
        num->digits[i] = product & 0xFF;
        carry = product >> 8;
    }
    
    while (carry > 0) {
        num->digits = m_renew(uint8_t, num->digits, num->length, num->length + 1);
        num->digits[num->length] = carry & 0xFF;
        num->length++;
        carry >>= 8;
    }
}

static bool bigint_is_zero(const BigInt *num) {
    return num->length == 1 && num->digits[0] == 0;
}

// Base43 Encoding
STATIC mp_obj_t base43_encode(mp_obj_t data_obj, mp_obj_t add_padding_obj) {
    mp_buffer_info_t data_buf;
    mp_get_buffer_raise(data_obj, &data_buf, MP_BUFFER_READ);
    uint8_t *data = data_buf.buf;
    size_t data_len = data_buf.len;
    bool add_padding = mp_obj_is_true(add_padding_obj);

    BigInt num = bigint_from_bytes(data, data_len);
    char *encoded = m_new(char, data_len * 2 + 2);
    size_t encoded_len = 0;

    // Handle zero case first
    if (bigint_is_zero(&num)) {
        encoded[encoded_len++] = B43CHARS[0];
    } else {
        // Process until number becomes zero
        while (!bigint_is_zero(&num)) {
            uint8_t rem;
            BigInt quotient = bigint_divmod(&num, 43, &rem);
            bigint_free(&num);
            encoded[encoded_len++] = B43CHARS[rem];
            num = quotient;
        }
        bigint_free(&num);
    }

    // Reverse the encoded string
    for (size_t i = 0; i < encoded_len / 2; i++) {
        char tmp = encoded[i];
        encoded[i] = encoded[encoded_len - 1 - i];
        encoded[encoded_len - 1 - i] = tmp;
    }

    // Padding (non-standard)
    if (add_padding) {
        size_t target_len = ((encoded_len + 3) / 4) * 4;
        while (encoded_len < target_len) {
            encoded[encoded_len++] = '=';
        }
    }

    mp_obj_t result = mp_obj_new_str(encoded, encoded_len);
    m_del(char, encoded, data_len * 2 + 2);
    return result;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_2(base43_encode_obj, base43_encode);

// Base43 Decoding
STATIC mp_obj_t base43_decode(mp_obj_t encoded_str_obj) {
    size_t encoded_len;
    const char *encoded_str = mp_obj_str_get_data(encoded_str_obj, &encoded_len);

    static bool initialized = false;
    if (!initialized) {
        init_base43_index();
        initialized = true;
    }

    // Remove padding
    while (encoded_len > 0 && encoded_str[encoded_len - 1] == '=') {
        encoded_len--;
    }

    BigInt num = bigint_from_bytes(NULL, 0);

    for (size_t i = 0; i < encoded_len; i++) {
        char c = encoded_str[i];
        int8_t val = base43_index[(unsigned char)c];
        if (val == -1) {
            bigint_free(&num);
            mp_raise_ValueError("Invalid Base43 character");
        }
        
        bigint_multiply_add(&num, 43, val);
    }

    // Remove leading zeros and reverse for correct byte order
    while (num.length > 1 && num.digits[num.length - 1] == 0) {
        num.length--;
    }
    
    // Reverse bytes to match encoding byte order
    for (size_t i = 0; i < num.length / 2; i++) {
        uint8_t tmp = num.digits[i];
        num.digits[i] = num.digits[num.length - 1 - i];
        num.digits[num.length - 1 - i] = tmp;
    }

    mp_obj_t result = mp_obj_new_bytes(num.digits, num.length);
    bigint_free(&num);
    return result;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_1(base43_decode_obj, base43_decode);

STATIC const mp_rom_map_elem_t base43_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_base43) },
    { MP_ROM_QSTR(MP_QSTR_decode), MP_ROM_PTR(&base43_decode_obj) },
    { MP_ROM_QSTR(MP_QSTR_encode), MP_ROM_PTR(&base43_encode_obj) },
};
STATIC MP_DEFINE_CONST_DICT(base43_module_globals, base43_module_globals_table);

const mp_obj_module_t base43_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&base43_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_base43, base43_user_cmodule, MODULE_BASE43_ENABLED);
