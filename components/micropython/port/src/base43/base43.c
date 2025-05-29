#include "py/runtime.h"
#include "py/builtin.h"
#include "py/obj.h"
#include "py/objstr.h"
#include <string.h>

static const char B43CHARS[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ$*+-./:";
static int8_t base43_index[256];
static bool base43_index_initialized = false;

static void init_base43_index(void) {
    if (base43_index_initialized) return;
    memset(base43_index, -1, sizeof(base43_index));
    for (int i = 0; i < 43; i++) {
        char c = B43CHARS[i];
        base43_index[(unsigned char)c] = i;
    }
    base43_index_initialized = true;
}

typedef struct {
    uint8_t *digits;
    size_t length;
} BigInt;

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

static void bigint_multiply_add(BigInt *num, uint8_t multiplier, uint8_t addend) {
    uint32_t carry = addend;
    uint8_t *digits = num->digits;
    size_t len = num->length;
    
    for (size_t i = len; i > 0; i--) {
        size_t idx = i - 1;
        carry += (uint32_t)digits[idx] * multiplier;
        digits[idx] = carry & 0xFF;
        carry >>= 8;
    }
    
    if (carry) {
        num->digits = m_renew(uint8_t, num->digits, len, len + 1);
        memmove(num->digits + 1, num->digits, len);
        num->digits[0] = carry;
        num->length++;
    }
}

static bool bigint_is_zero(const BigInt *num) {
    for (size_t i = 0; i < num->length; i++) {
        if (num->digits[i] != 0) {
            return false;
        }
    }
    return true;
}

static uint8_t bigint_divide_by_43(BigInt *num) {
    uint32_t remainder = 0;
    uint8_t *digits = num->digits;
    size_t len = num->length;
    
    // Divide in-place from most significant to least significant
    for (size_t i = 0; i < len; i++) {
        uint32_t temp = (remainder << 8) | digits[i];
        digits[i] = temp / 43;
        remainder = temp % 43;
    }
    
    // Remove leading zeros
    size_t leading_zeros = 0;
    while (leading_zeros < len - 1 && digits[leading_zeros] == 0) {
        leading_zeros++;
    }
    
    if (leading_zeros > 0) {
        num->length = len - leading_zeros;
        memmove(digits, digits + leading_zeros, num->length);
    }
    
    return (uint8_t)remainder;
}

// Base43 Encoding
STATIC mp_obj_t base43_encode(mp_obj_t data_obj, mp_obj_t add_padding_obj) {
    mp_buffer_info_t data_buf;
    mp_get_buffer_raise(data_obj, &data_buf, MP_BUFFER_READ);
    uint8_t *data = data_buf.buf;
    size_t data_len = data_buf.len;
    bool add_padding = mp_obj_is_true(add_padding_obj);

    // Handle zero-length input
    if (data_len == 0) {
        return mp_obj_new_str("", 0);
    }

    // Ideal calculation for data size: ceil(data_len * 8 * log(2) / log(43))
    // log(2)/log(43) ≈ 0.1845, so data_len * 8 * 0.185 ≈ data_len * 1.48
    size_t max_out_len = data_len + (data_len >> 1) + 2; // data_len * 1.5 + 2
    if (add_padding) {
        max_out_len = ((max_out_len + 3) / 4) * 4;  // Round to next multiple of 4
    }

    // Allocate output buffer
    char *output_buf = m_new(char, max_out_len);
    size_t out_len = 0;

    // Special case for all zeros
    bool all_zero = true;
    for (size_t i = 0; i < data_len; i++) {
        if (data[i] != 0) {
            all_zero = false;
            break;
        }
    }
    if (all_zero) {
        output_buf[out_len++] = B43CHARS[0];
    } else {
        // Use BigInt approach for correctness
        BigInt num = bigint_from_bytes(data, data_len);
        
        while (!bigint_is_zero(&num)) {
            if (out_len >= max_out_len) {
                bigint_free(&num);
                m_del(char, output_buf, max_out_len);
                mp_raise_ValueError("Output buffer overflow");
            }
            
            uint8_t rem = bigint_divide_by_43(&num);
            output_buf[out_len] = B43CHARS[rem];
            out_len++;
        }
        bigint_free(&num);
        
        // Reverse the digits
        for (size_t i = 0; i < out_len / 2; i++) {
            char tmp = output_buf[i];
            output_buf[i] = output_buf[out_len - 1 - i];
            output_buf[out_len - 1 - i] = tmp;
        }
    }

    // Add padding if requested
    if (add_padding) {
        size_t target_len = ((out_len + 3) / 4) * 4;
        if (target_len > max_out_len) {
            m_del(char, output_buf, max_out_len);
            mp_raise_ValueError("Padding overflow");
        }
        for (size_t i = out_len; i < target_len; i++) {
            output_buf[i] = '=';
        }
        out_len = target_len;
    }

    mp_obj_t result = mp_obj_new_str(output_buf, out_len);
    m_del(char, output_buf, max_out_len);
    return result;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_2(base43_encode_obj, base43_encode);

// Base43 Decoding (optimized and safe)
STATIC mp_obj_t base43_decode(mp_obj_t encoded_str_obj) {
    size_t encoded_len;
    const char *encoded_str = mp_obj_str_get_data(encoded_str_obj, &encoded_len);

    init_base43_index();

    // Remove padding
    while (encoded_len > 0 && encoded_str[encoded_len - 1] == '=') {
        encoded_len--;
    }

    // Handle empty string
    if (encoded_len == 0) {
        return mp_obj_new_bytes(NULL, 0);
    }

    // Special case for "0"
    if (encoded_len == 1 && encoded_str[0] == '0') {
        uint8_t zero = 0;
        return mp_obj_new_bytes(&zero, 1);
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

    // Strip leading zeros
    size_t start = 0;
    while (start < num.length - 1 && num.digits[start] == 0) {
        start++;
    }

    mp_obj_t result = mp_obj_new_bytes(num.digits + start, num.length - start);
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