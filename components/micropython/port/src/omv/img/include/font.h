/*
 * This file is part of the OpenMV project.
 * Copyright (c) 2013/2014 Ibrahim Abdelkader <i.abdalkader@gmail.com>
 * This work is licensed under the MIT license, see the file LICENSE for details.
 *
 * Font data.
 *
 */
#ifndef __FONT_H__
#define __FONT_H__
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "fmath.h"
#include "imdefs.h"
#include "imlib.h"

// #define JAPANESE_CODEPOINT_MIN 0x3000
// #define JAPANESE_CODEPOINT_MAX 0x30FF
// #define CHINESE_CODEPOINT_MIN 0x4E00
// #define CHINESE_CODEPOINT_MAX 0x9FFF
// #define KOREAN_CODEPOINT_MIN 0xAC00
// #define KOREAN_CODEPOINT_MAX 0xD7A3
// #define FULLHALFWIDTH_CODEPOINT_MIN 0xFF00
// #define FULLHALFWIDTH_CODEPOINT_MAX 0xFFEF
#define WIDEFONT_CODEPOINT_MIN 0x3000
#define WIDEFONT_CODEPOINT_MAX 0xFFEF

enum FontIndex {
    ASCII,
    Unicode,
    UTF8,
    GBK,
    GB2312,
};

enum FontSource {
    BuildIn,
    FileIn,
    StringIO,
    ArrayIn,
};

void font_load(uint8_t index, uint8_t width, uint8_t high, uint8_t source_type, void *src_addr);
void font_free();
int string_width_px(mp_obj_t str);
int string_has_wide_glyph(mp_obj_t str);
int font_height();

#endif // __FONT_H__
