/*
 * This file is part of the MicroPython K210 project, https://github.com/loboris/MicroPython_K210_LoBo
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 LoBo (https://github.com/loboris)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "py/mpconfig.h"

#if MICROPY_PY_UCRYPTOLIB_MAIX

#include <string.h>
#include "aes.h"
#include "py/objstr.h"
#include "py/runtime.h"

enum {
    UCRYPTOLIB_MODE_MIN = 0,
    UCRYPTOLIB_MODE_GCM,
    UCRYPTOLIB_MODE_CBC,
    UCRYPTOLIB_MODE_ECB,
    UCRYPTOLIB_MODE_CTR,
    UCRYPTOLIB_MODE_MAX,
};

#define AES_KEYLEN_128  0
#define AES_KEYLEN_192  1
#define AES_KEYLEN_256  2

typedef struct _context
{
    /* The buffer holding the encryption or decryption key. */
    uint8_t *input_key;
    /* The initialization vector. must be 96 bit for gcm, 128 bit for cbc*/
    uint8_t iv[16];
} context_t;

typedef struct _mp_obj_aes_t {
    mp_obj_base_t base;
    context_t ctx;
    uint8_t mode;
    uint8_t key_len;
    uint8_t gcm_tag[4];
} mp_obj_aes_t;

//------------------------------------------------------------------------------------------------------------------
STATIC mp_obj_t ucryptolib_aes_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum {
        ARG_key,
        ARG_mode,
        ARG_iv,
        ARG_nonce,
        ARG_initial_value,
        ARG_mac_len,
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_key, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_mode, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_iv, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_nonce, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_initial_value, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_mac_len, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 4} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    // Validate key
    mp_buffer_info_t keyinfo;
    mp_get_buffer_raise(args[ARG_key].u_obj, &keyinfo, MP_BUFFER_READ);
    if (keyinfo.len != 16 && keyinfo.len != 24 && keyinfo.len != 32) {
        mp_raise_ValueError("key must be 16/24/32 bytes");
    }

    // Validate mode
    uint8_t mode = args[ARG_mode].u_int;
    if (mode <= UCRYPTOLIB_MODE_MIN || mode >= UCRYPTOLIB_MODE_MAX) {
        mp_raise_ValueError("invalid mode");
    }

    // Validate mac_len for GCM mode
    if (mode == UCRYPTOLIB_MODE_GCM) {
        int mac_len = args[ARG_mac_len].u_int;
        if (mac_len != 4) {
            mp_raise_ValueError("only mac_len=4 is supported");
        }
    } else if (args[ARG_mac_len].u_int != 4) {
        mp_raise_ValueError("mac_len is only valid for GCM mode");
    }

    mp_obj_aes_t *o = m_new_obj(mp_obj_aes_t);
    o->base.type = type;
    o->mode = mode;
    o->key_len = (keyinfo.len == 32) ? AES_KEYLEN_256 : 
                  (keyinfo.len == 24) ? AES_KEYLEN_192 : AES_KEYLEN_128;
    o->ctx.input_key = keyinfo.buf;

    // Handle IV/nonce
    memset(o->ctx.iv, 0, sizeof(o->ctx.iv));
    if (mode == UCRYPTOLIB_MODE_CTR) {
        if (args[ARG_nonce].u_obj != mp_const_none) {
            // Use nonce + initial_value from keywords
            mp_buffer_info_t nonceinfo;
            mp_get_buffer_raise(args[ARG_nonce].u_obj, &nonceinfo, MP_BUFFER_READ);
            if (nonceinfo.len != 12) {
                mp_raise_ValueError("nonce must be 12 bytes");
            }
            memcpy(o->ctx.iv, nonceinfo.buf, 12);
            uint32_t counter = args[ARG_initial_value].u_int;
            o->ctx.iv[12] = (counter >> 24) & 0xFF;
            o->ctx.iv[13] = (counter >> 16) & 0xFF;
            o->ctx.iv[14] = (counter >> 8) & 0xFF;
            o->ctx.iv[15] = counter & 0xFF;
        } else {
            // Fallback to IV parameter (16 bytes)
            if (args[ARG_iv].u_obj != mp_const_none) {
                mp_buffer_info_t ivinfo;
                mp_get_buffer_raise(args[ARG_iv].u_obj, &ivinfo, MP_BUFFER_READ);
                if (ivinfo.len != 16) {
                    mp_raise_ValueError("IV must be 16 bytes for CTR");
                }
                memcpy(o->ctx.iv, ivinfo.buf, 16);
            } else {
                mp_raise_ValueError("CTR requires nonce or IV");
            }
        }
    } else {
        // Original IV handling for other modes
        if (args[ARG_iv].u_obj != mp_const_none) {
            mp_buffer_info_t ivinfo;
            mp_get_buffer_raise(args[ARG_iv].u_obj, &ivinfo, MP_BUFFER_READ);
            uint8_t expected_len = (mode == UCRYPTOLIB_MODE_GCM) ? 12 : 
                                   (mode == UCRYPTOLIB_MODE_CBC) ? 16 : 0;
            if (ivinfo.len != expected_len) {
                mp_raise_ValueError("wrong IV length for mode");
            }
            memcpy(o->ctx.iv, ivinfo.buf, ivinfo.len);
        }
    }

    return MP_OBJ_FROM_PTR(o);
}

//----------------------------------------------------------------------------
STATIC mp_obj_t AES_run(size_t n_args, const mp_obj_t *args, bool encrypt)
{
    mp_obj_aes_t *self = MP_OBJ_TO_PTR(args[0]);

    // get input
    mp_obj_t in_buf = args[1];
    mp_obj_t out_buf = MP_OBJ_NULL;
    if (n_args > 2) {
        // separate output is used
        out_buf = args[2];
    }

    // create output buffer
    mp_buffer_info_t in_bufinfo;
    mp_get_buffer_raise(in_buf, &in_bufinfo, MP_BUFFER_READ);

    if ((self->mode != UCRYPTOLIB_MODE_GCM && self->mode != UCRYPTOLIB_MODE_CTR) && ((in_bufinfo.len % 16) != 0)) {
        mp_raise_ValueError("input length must be multiple of 16");
    }

    vstr_t vstr;
    mp_buffer_info_t out_bufinfo;
    uint8_t *out_buf_ptr;

    if (out_buf != MP_OBJ_NULL) {
        mp_get_buffer_raise(out_buf, &out_bufinfo, MP_BUFFER_WRITE);
        if (out_bufinfo.len < in_bufinfo.len) {
            mp_raise_ValueError("output too small");
        }
        out_buf_ptr = out_bufinfo.buf;
    }
    else {
        vstr_init_len(&vstr, in_bufinfo.len);
        out_buf_ptr = (uint8_t*)vstr.buf;
    }

    if (self->mode == UCRYPTOLIB_MODE_GCM) {
        uint8_t gcm_tag[4];
        gcm_context_t ctx;
        ctx.input_key = self->ctx.input_key;
        ctx.iv = self->ctx.iv;
        ctx.gcm_aad = NULL;
        ctx.gcm_aad_len = 0;
        if (!encrypt) {
            if (self->key_len == AES_KEYLEN_256) aes_gcm256_hard_decrypt(&ctx, in_bufinfo.buf, in_bufinfo.len, out_buf_ptr, gcm_tag);
            else if (self->key_len == AES_KEYLEN_192) aes_gcm192_hard_decrypt(&ctx, in_bufinfo.buf, in_bufinfo.len, out_buf_ptr, gcm_tag);
            else aes_gcm128_hard_decrypt(&ctx, in_bufinfo.buf, in_bufinfo.len, out_buf_ptr, gcm_tag);
        }
        else {
            if (self->key_len == AES_KEYLEN_256) aes_gcm256_hard_encrypt(&ctx, in_bufinfo.buf, in_bufinfo.len, out_buf_ptr, gcm_tag);
            else if (self->key_len == AES_KEYLEN_192) aes_gcm192_hard_encrypt(&ctx, in_bufinfo.buf, in_bufinfo.len, out_buf_ptr, gcm_tag);
            else aes_gcm128_hard_encrypt(&ctx, in_bufinfo.buf, in_bufinfo.len, out_buf_ptr, gcm_tag);
        }
        memcpy(self->gcm_tag, gcm_tag, sizeof(gcm_tag));
    }
    else if (self->mode == UCRYPTOLIB_MODE_CBC) {
        cbc_context_t ctx;
        ctx.input_key = self->ctx.input_key;
        ctx.iv = self->ctx.iv;
        if (!encrypt) {
            if (self->key_len == AES_KEYLEN_256) aes_cbc256_hard_decrypt(&ctx, in_bufinfo.buf, in_bufinfo.len, out_buf_ptr);
            else if (self->key_len == AES_KEYLEN_192) aes_cbc192_hard_decrypt(&ctx, in_bufinfo.buf, in_bufinfo.len, out_buf_ptr);
            else aes_cbc128_hard_decrypt(&ctx, in_bufinfo.buf, in_bufinfo.len, out_buf_ptr);
        }
        else {
            if (self->key_len == AES_KEYLEN_256) aes_cbc256_hard_encrypt(&ctx, in_bufinfo.buf, in_bufinfo.len, out_buf_ptr);
            else if (self->key_len == AES_KEYLEN_192) aes_cbc192_hard_encrypt(&ctx, in_bufinfo.buf, in_bufinfo.len, out_buf_ptr);
            else aes_cbc128_hard_encrypt(&ctx, in_bufinfo.buf, in_bufinfo.len, out_buf_ptr);
        }
    }
    else if (self->mode == UCRYPTOLIB_MODE_ECB) {
        if (!encrypt) {
            if (self->key_len == AES_KEYLEN_256) aes_ecb256_hard_decrypt(self->ctx.input_key, in_bufinfo.buf, in_bufinfo.len, out_buf_ptr);
            else if (self->key_len == AES_KEYLEN_192) aes_ecb192_hard_decrypt(self->ctx.input_key, in_bufinfo.buf, in_bufinfo.len, out_buf_ptr);
            else aes_ecb128_hard_decrypt(self->ctx.input_key, in_bufinfo.buf, in_bufinfo.len, out_buf_ptr);
        }
        else {
            if (self->key_len == AES_KEYLEN_256) aes_ecb256_hard_encrypt(self->ctx.input_key, in_bufinfo.buf, in_bufinfo.len, out_buf_ptr);
            else if (self->key_len == AES_KEYLEN_192) aes_ecb192_hard_encrypt(self->ctx.input_key, in_bufinfo.buf, in_bufinfo.len, out_buf_ptr);
            else aes_ecb128_hard_encrypt(self->ctx.input_key, in_bufinfo.buf, in_bufinfo.len, out_buf_ptr);
        }
    }
    else if (self->mode == UCRYPTOLIB_MODE_CTR) {
    uint8_t nonce[12];
    uint32_t initial_counter;
    memcpy(nonce, self->ctx.iv, 12);
    initial_counter = (self->ctx.iv[12] << 24) | (self->ctx.iv[13] << 16) | (self->ctx.iv[14] << 8) | self->ctx.iv[15];

    size_t total_blocks = in_bufinfo.len / 16;
    size_t remaining = in_bufinfo.len % 16;
    uint32_t current_counter = initial_counter;

    // Process full blocks
    uint8_t counter_block[16];
    uint8_t encrypted_counter[16];
    for (size_t i = 0; i < total_blocks; i++) {
        memcpy(counter_block, nonce, 12);
        counter_block[12] = (current_counter >> 24) & 0xFF;
        counter_block[13] = (current_counter >> 16) & 0xFF;
        counter_block[14] = (current_counter >> 8) & 0xFF;
        counter_block[15] = current_counter & 0xFF;

        // Always use ECB encryption for CTR mode
        switch (self->key_len) {
            case AES_KEYLEN_256:
                aes_ecb256_hard_encrypt(self->ctx.input_key, counter_block, 16, encrypted_counter);
                break;
            case AES_KEYLEN_192:
                aes_ecb192_hard_encrypt(self->ctx.input_key, counter_block, 16, encrypted_counter);
                break;
            default:
                aes_ecb128_hard_encrypt(self->ctx.input_key, counter_block, 16, encrypted_counter);
        }

        // Optimized XOR with input using 64-bit words
        uint64_t *in64 = (uint64_t*)&((uint8_t*)in_bufinfo.buf)[i*16];
        uint64_t *enc64 = (uint64_t*)encrypted_counter;
        uint64_t *out64 = (uint64_t*)&out_buf_ptr[i*16];
        out64[0] = in64[0] ^ enc64[0];
        out64[1] = in64[1] ^ enc64[1];

        current_counter++;
    }

    // Handle partial block
    if (remaining > 0) {
        uint8_t counter_block[16];
        memcpy(counter_block, nonce, 12);
        counter_block[12] = (current_counter >> 24) & 0xFF;
        counter_block[13] = (current_counter >> 16) & 0xFF;
        counter_block[14] = (current_counter >> 8) & 0xFF;
        counter_block[15] = current_counter & 0xFF;

        uint8_t encrypted_counter[16];
        switch (self->key_len) {
            case AES_KEYLEN_256:
                aes_ecb256_hard_encrypt(self->ctx.input_key, counter_block, 16, encrypted_counter);
                break;
            case AES_KEYLEN_192:
                aes_ecb192_hard_encrypt(self->ctx.input_key, counter_block, 16, encrypted_counter);
                break;
            default:
                aes_ecb128_hard_encrypt(self->ctx.input_key, counter_block, 16, encrypted_counter);
        }

        for (size_t j = 0; j < remaining; j++) {
            out_buf_ptr[total_blocks*16 + j] = ((uint8_t*)in_bufinfo.buf)[total_blocks*16 + j] ^ encrypted_counter[j];
        }
    }
}

    if (out_buf != MP_OBJ_NULL) {
        return out_buf;
    }
    return mp_obj_new_str_from_vstr(&mp_type_bytes, &vstr);
}

//-------------------------------------------------------------------------
STATIC mp_obj_t ucryptolib_aes_encrypt(size_t n_args, const mp_obj_t *args)
{
    return AES_run(n_args, args, true);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ucryptolib_aes_encrypt_obj, 2, 3, ucryptolib_aes_encrypt);

//-------------------------------------------------------------------------
STATIC mp_obj_t ucryptolib_aes_decrypt(size_t n_args, const mp_obj_t *args)
{
    return AES_run(n_args, args, false);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ucryptolib_aes_decrypt_obj, 2, 3, ucryptolib_aes_decrypt);

STATIC mp_obj_t ucryptolib_aes_get_tag(mp_obj_t self_in) {
    mp_obj_aes_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->mode != UCRYPTOLIB_MODE_GCM) {
        mp_raise_ValueError("tag is only available in GCM mode");
    }
    return mp_obj_new_bytes(self->gcm_tag, sizeof(self->gcm_tag));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(ucryptolib_aes_get_tag_obj, ucryptolib_aes_get_tag);

STATIC mp_obj_t ucryptolib_aes_verify_tag(mp_obj_t self_in, mp_obj_t tag_in) {
    mp_obj_aes_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->mode != UCRYPTOLIB_MODE_GCM) {
        mp_raise_ValueError("tag verification only available in GCM mode");
    }
    mp_buffer_info_t taginfo;
    mp_get_buffer_raise(tag_in, &taginfo, MP_BUFFER_READ);
    if (taginfo.len != sizeof(self->gcm_tag)) {
        mp_raise_ValueError("wrong tag length");
    }
    if (memcmp(self->gcm_tag, taginfo.buf, sizeof(self->gcm_tag)) != 0) {
        mp_raise_ValueError("tag verification failed");
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(ucryptolib_aes_verify_tag_obj, ucryptolib_aes_verify_tag);

/*
//----------------------------------------------------
STATIC mp_obj_t ucryptolib_aes_getIV(mp_obj_t self_in)
{
    mp_obj_aes_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_obj_new_str_of_type(&mp_type_bytes, self->ctx.iv, (self->mode == UCRYPTOLIB_MODE_GCM) ? 12 : 16);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(ucryptolib_aes_getIV_obj, ucryptolib_aes_getIV);
*/

//===================================================================
STATIC const mp_rom_map_elem_t ucryptolib_aes_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_encrypt), MP_ROM_PTR(&ucryptolib_aes_encrypt_obj) },
    { MP_ROM_QSTR(MP_QSTR_decrypt), MP_ROM_PTR(&ucryptolib_aes_decrypt_obj) },
    { MP_ROM_QSTR(MP_QSTR_digest), MP_ROM_PTR(&ucryptolib_aes_get_tag_obj) },
    { MP_ROM_QSTR(MP_QSTR_verify), MP_ROM_PTR(&ucryptolib_aes_verify_tag_obj) },
    //{ MP_ROM_QSTR(MP_QSTR_getIV), MP_ROM_PTR(&ucryptolib_aes_getIV_obj) },
};
STATIC MP_DEFINE_CONST_DICT(ucryptolib_aes_locals_dict, ucryptolib_aes_locals_dict_table);

//================================================
STATIC const mp_obj_type_t ucryptolib_aes_type = {
    { &mp_type_type },
    .name = MP_QSTR_aes,
    .make_new = ucryptolib_aes_make_new,
    .locals_dict = (void*)&ucryptolib_aes_locals_dict,
};

//=====================================================================
STATIC const mp_rom_map_elem_t mp_module_ucryptolib_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_ucryptolib) },
    { MP_ROM_QSTR(MP_QSTR_aes), MP_ROM_PTR(&ucryptolib_aes_type) },

    { MP_ROM_QSTR(MP_QSTR_MODE_GCM), MP_ROM_INT(UCRYPTOLIB_MODE_GCM) },
    { MP_ROM_QSTR(MP_QSTR_MODE_CBC), MP_ROM_INT(UCRYPTOLIB_MODE_CBC) },
    { MP_ROM_QSTR(MP_QSTR_MODE_ECB), MP_ROM_INT(UCRYPTOLIB_MODE_ECB) },
    { MP_ROM_QSTR(MP_QSTR_MODE_CTR), MP_ROM_INT(UCRYPTOLIB_MODE_CTR) },
};

STATIC MP_DEFINE_CONST_DICT(mp_module_ucryptolib_globals, mp_module_ucryptolib_globals_table);

const mp_obj_module_t mp_module_ucryptolib = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_ucryptolib_globals,
};

#endif //MICROPY_PY_UCRYPTOLIB
