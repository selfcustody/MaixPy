/*
* Copyright 2019 Sipeed Co.,Ltd.

* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <assert.h>
#include <string.h>
#include "py/runtime.h"

#if MICROPY_PY_UHASHLIB_MAIX
#include "sha256.h"

#define SHA256_BASE_ADDR    (0x502C0000U)
#define SHA256_BLOCK_LEN   64L

typedef struct _mp_obj_hash_t {
    mp_obj_base_t base;
    size_t total_len;
    char state[0];
} mp_obj_hash_t;

STATIC const mp_obj_type_t uhashlib_sha256_type;

volatile sha256_t* const sha256_hw = (volatile sha256_t*)SHA256_BASE_ADDR;

void sha256_quick_init(sha256_context_t *context, size_t input_len)
{
    sha256_hw->sha_num_reg.sha_data_cnt = (uint32_t)((input_len + SHA256_BLOCK_LEN + 8) / SHA256_BLOCK_LEN);
    sha256_hw->sha_function_reg_0.sha_en = 0x1;
    context->total_len = 0L;
    context->buffer_len = 0L;
}

#if MICROPY_PY_UHASHLIB_SHA256_MAIX
STATIC mp_obj_t uhashlib_sha256_update(mp_obj_t self_in, mp_obj_t arg);

STATIC mp_obj_t uhashlib_sha256_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 1, false);
    mp_obj_hash_t *o = m_new_obj_var(mp_obj_hash_t, char, sizeof(sha256_context_t));
    o->base.type = type;
    o->total_len = 0;
    sha256_init((sha256_context_t*)o->state, 64);
    if (n_args == 1) {
        uhashlib_sha256_update(MP_OBJ_FROM_PTR(o), args[0]);
    }
    return MP_OBJ_FROM_PTR(o);
}

STATIC mp_obj_t uhashlib_sha256_update(mp_obj_t self_in, mp_obj_t arg) {
    mp_obj_hash_t *self = MP_OBJ_TO_PTR(self_in);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(arg, &bufinfo, MP_BUFFER_READ);
    self->total_len += bufinfo.len;
    sha256_update_length(self->total_len);
    sha256_update((sha256_context_t*)self->state, bufinfo.buf, bufinfo.len);
    return mp_const_none;
}

STATIC mp_obj_t uhashlib_sha256_digest(mp_obj_t self_in) {
    mp_obj_hash_t *self = MP_OBJ_TO_PTR(self_in);
    vstr_t vstr;
    vstr_init_len(&vstr, 32);
    sha256_final((sha256_context_t*)self->state, (byte*)vstr.buf);
    return mp_obj_new_str_from_vstr(&mp_type_bytes, &vstr);
}

STATIC mp_obj_t uhashlib_sha256_hard(mp_obj_t self_in,mp_obj_t arg) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(arg, &bufinfo, MP_BUFFER_READ);
    vstr_t vstr;
    vstr_init_len(&vstr, 32);
    sha256_hard_calculate(bufinfo.buf, bufinfo.len, (byte*)vstr.buf);
    return mp_obj_new_str_from_vstr(&mp_type_bytes, &vstr);
}

STATIC mp_obj_t mod_uhashlib_pbkdf2_hmac_sha256(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_password, ARG_salt, ARG_iterations, ARG_dklen };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_password,  MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_salt,      MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_iterations,MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = 0} },
        { MP_QSTR_dklen,     MP_ARG_INT, {.u_int = 32} },
    };

    // uint64_t start_us = sysctl_get_time_us();

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    // Get buffers for password and salt.
    mp_buffer_info_t password_buf, salt_buf;
    mp_get_buffer_raise(args[ARG_password].u_obj, &password_buf, MP_BUFFER_READ);
    mp_get_buffer_raise(args[ARG_salt].u_obj, &salt_buf, MP_BUFFER_READ);

    uint32_t iterations = args[ARG_iterations].u_int;
    uint32_t dklen = args[ARG_dklen].u_int;
    if (iterations < 1) {
        mp_raise_ValueError("iterations must be >= 1");
    }
    if (dklen < 1) {
        mp_raise_ValueError("dklen must be >= 1");
    }
    if (iterations > 2000000) {
        mp_raise_ValueError("iterations too large (max 2000000)");
    }
    if (dklen > 1024) {
        mp_raise_ValueError("dklen too large (max 1024)");
    }
    if (salt_buf.len > 1024) {
        mp_raise_ValueError("salt too large (max 1024 bytes)");
    }
    
    // Process the password into the key (64 bytes)
    uint8_t key[64];
    if (password_buf.len > 64) {
        // Hash the password using incremental functions for hardware acceleration
        sha256_context_t key_ctx;
        sha256_init(&key_ctx, password_buf.len);
        sha256_update(&key_ctx, password_buf.buf, password_buf.len);
        uint8_t hashed_key[32];
        sha256_final(&key_ctx, hashed_key);
        memcpy(key, hashed_key, 32);
        memset(key + 32, 0, 32);
    } else {
        memcpy(key, password_buf.buf, password_buf.len);
        memset(key + password_buf.len, 0, 64 - password_buf.len);
    }

    // Precompute HMAC pads
    uint8_t inner_pad[64], outer_pad[64];
    for (size_t i = 0; i < 64; i++) {
        inner_pad[i] = key[i] ^ 0x36;
        outer_pad[i] = key[i] ^ 0x5C;
    }

    int hlen = 32;
    int l = (dklen + hlen - 1) / hlen;
    vstr_t dk_vstr;
    vstr_init_len(&dk_vstr, l * hlen);
    uint8_t *dk = (uint8_t *)dk_vstr.buf;

    for (int i = 1; i <= l; i++) {
        uint8_t *initial_message = m_new(uint8_t, salt_buf.len + 4);
        memcpy(initial_message, salt_buf.buf, salt_buf.len);
        initial_message[salt_buf.len] = (i >> 24) & 0xFF;
        initial_message[salt_buf.len + 1] = (i >> 16) & 0xFF;
        initial_message[salt_buf.len + 2] = (i >> 8) & 0xFF;
        initial_message[salt_buf.len + 3] = i & 0xFF;

        // Compute U_1 = HMAC(inner_pad || salt_plus_i)
        uint8_t u_current[32];
        {
            // Inner hash
            sha256_context_t inner_ctx, outer_ctx;
            size_t inner_total_len = 64 + salt_buf.len + 4;
            sha256_init(&inner_ctx, inner_total_len);
            sha256_update(&inner_ctx, inner_pad, 64);
            sha256_update(&inner_ctx, initial_message, salt_buf.len + 4);
            uint8_t inner_hash[32];
            sha256_final(&inner_ctx, inner_hash);

            // Outer hash
            sha256_init(&outer_ctx, 64 + 32);
            sha256_update(&outer_ctx, outer_pad, 64);
            sha256_update(&outer_ctx, inner_hash, 32);
            sha256_final(&outer_ctx, u_current);
        }

        // Initialize accum with U_1
        uint8_t accum[32];
        memcpy(accum, u_current, 32);

        // Iterations loop (j starts at 2)
        for (int j = 2; j <= iterations; j++) {
            // Compute U_j = HMAC(u_prev)

            sha256_context_t inner_ctx;
            sha256_quick_init(&inner_ctx, 64 + 32);
            sha256_update(&inner_ctx, inner_pad, 64);
            sha256_update(&inner_ctx, u_current, 32);
            uint8_t inner_hash_j[32];
            sha256_final(&inner_ctx, inner_hash_j);

            sha256_context_t outer_ctx;
            sha256_quick_init(&outer_ctx, 64 + 32);
            sha256_update(&outer_ctx, outer_pad, 64);
            sha256_update(&outer_ctx, inner_hash_j, 32);
            sha256_final(&outer_ctx, u_current);

            // XOR accum with u_current (64-bit optimized)
            uint64_t *accum64 = (uint64_t*)accum;
            uint64_t *u64 = (uint64_t*)u_current;
            accum64[0] ^= u64[0];
            accum64[1] ^= u64[1];
            accum64[2] ^= u64[2];
            accum64[3] ^= u64[3];
        }

        // Copy accum to derived key
        int offset = (i - 1) * 32;
        int copy_len = (i == l) ? dklen - offset : 32;
        memcpy(dk + offset, accum, copy_len);
        m_del(uint8_t, initial_message, salt_buf.len + 4);
    }
    // uint64_t end_us = sysctl_get_time_us();
    // mp_printf(&mp_plat_print, "PBKDF2 took %lu us\n", (unsigned long)(end_us - start_us));
    
    return mp_obj_new_bytes(dk, dklen);
}

STATIC MP_DEFINE_CONST_FUN_OBJ_2(uhashlib_sha256_update_obj, uhashlib_sha256_update);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(uhashlib_sha256_digest_obj, uhashlib_sha256_digest);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(uhashlib_sha256_hard_obj, uhashlib_sha256_hard);
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(pbkdf2_hmac_sha256_obj, 3, mod_uhashlib_pbkdf2_hmac_sha256);

STATIC const mp_rom_map_elem_t uhashlib_sha256_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_update), MP_ROM_PTR(&uhashlib_sha256_update_obj) },
    { MP_ROM_QSTR(MP_QSTR_digest), MP_ROM_PTR(&uhashlib_sha256_digest_obj) },
    { MP_ROM_QSTR(MP_QSTR_calculate_hard), MP_ROM_PTR(&uhashlib_sha256_hard_obj) },
};
STATIC MP_DEFINE_CONST_DICT(uhashlib_sha256_locals_dict, uhashlib_sha256_locals_dict_table);

STATIC const mp_obj_type_t uhashlib_sha256_type = {
    { &mp_type_type },
    .name = MP_QSTR_sha256,
    .make_new = uhashlib_sha256_make_new,
    .locals_dict = (mp_obj_dict_t*)&uhashlib_sha256_locals_dict,
};
#endif


STATIC const mp_rom_map_elem_t mp_module_uhashlib_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_uhashlib) },
    { MP_ROM_QSTR(MP_QSTR_sha256), MP_ROM_PTR(&uhashlib_sha256_type) },
    { MP_ROM_QSTR(MP_QSTR_pbkdf2_hmac_sha256), MP_ROM_PTR(&pbkdf2_hmac_sha256_obj) },
};

STATIC MP_DEFINE_CONST_DICT(mp_module_uhashlib_globals, mp_module_uhashlib_globals_table);

const mp_obj_module_t mp_module_uhashlib_maix = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_uhashlib_globals,
};

#endif //MICROPY_PY_UHASHLIB_K210
