#include "py/obj.h"
#include "py/runtime.h"
#include "py/mperrno.h"
#include "mpconfigboard.h"
#include "stdint.h"
#include "stdbool.h"
#include "stdlib.h"
#include "sipeed_mem.h"
#include "w25qxx.h"

// sys_free(): return the number of bytes of available sys heap RAM
STATIC mp_obj_t py_heap_free(void) {
    return MP_OBJ_NEW_SMALL_INT(get_free_heap_size2());
}
MP_DEFINE_CONST_FUN_OBJ_0(py_heap_free_obj, py_heap_free);

// STATIC mp_obj_t py_malloc(mp_obj_t arg) {
//     void malloc_stats(void);
//     malloc_stats();
//     void* p = malloc(mp_obj_get_int(arg));
//     return mp_obj_new_int((mp_int_t)p);
// }

// STATIC MP_DEFINE_CONST_FUN_OBJ_1(py_malloc_obj, py_malloc);

// STATIC mp_obj_t py_free(mp_obj_t arg) {
//     free(mp_obj_get_int(arg));
//     return mp_const_none;
// }

// STATIC MP_DEFINE_CONST_FUN_OBJ_1(py_free_obj, py_free);


// STATIC mp_obj_t py_flash_write(mp_obj_t addr, mp_obj_t data_in) {
//     mp_buffer_info_t bufinfo;
//     mp_get_buffer_raise(data_in, &bufinfo, MP_BUFFER_READ);
//     w25qxx_status_t status = w25qxx_write_data_dma(mp_obj_get_int(addr), bufinfo.buf, (uint32_t)bufinfo.len);
//     return mp_obj_new_int(status); // (status != W25QXX_OK)
// }

// STATIC MP_DEFINE_CONST_FUN_OBJ_2(py_flash_write_obj, py_flash_write);

STATIC mp_obj_t py_flash_read(mp_obj_t addr, mp_obj_t len_in) {
    size_t length = mp_obj_get_int(len_in);
    byte* data = m_new(byte, length);
    w25qxx_status_t status = w25qxx_read_data_dma(mp_obj_get_int(addr), data, (uint32_t)length, W25QXX_QUAD_FAST);
    if(status != W25QXX_OK)
    {
        mp_raise_OSError(MP_EIO);
    }
    return mp_obj_new_bytes(data, length);
}

STATIC MP_DEFINE_CONST_FUN_OBJ_2(py_flash_read_obj, py_flash_read);

static const mp_map_elem_t locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),        MP_OBJ_NEW_QSTR(MP_QSTR_utils) },
    { MP_ROM_QSTR(MP_QSTR_heap_free),    (mp_obj_t)(&py_heap_free_obj) },
    // { MP_ROM_QSTR(MP_QSTR_malloc),    (mp_obj_t)(&py_malloc_obj) },
    // { MP_ROM_QSTR(MP_QSTR_free),    (mp_obj_t)(&py_free_obj) },
    { MP_ROM_QSTR(MP_QSTR_flash_read),    (mp_obj_t)(&py_flash_read_obj) },
    // { MP_ROM_QSTR(MP_QSTR_flash_write),    (mp_obj_t)(&py_flash_write_obj) },
};
STATIC MP_DEFINE_CONST_DICT(locals_dict, locals_dict_table);

const mp_obj_type_t Maix_utils_type = {
    .base = { &mp_type_type },
    .name = MP_QSTR_utils,
    .locals_dict = (mp_obj_dict_t*)&locals_dict
};