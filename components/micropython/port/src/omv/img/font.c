
#include "include/font.h"
#include "include/font_device.h"


// static inline void font_init(uint8_t width, uint8_t high, uint8_t index, uint8_t source_type, void *font_offset)
// {
//   struct font tmp = { width, high, index, source_type, font_offset};
//   font_config = tmp;
// }

#include "vfs_wrapper.h"
#include "nlr.h"

void font_free()
{
  switch (font_config.index)
  {
    case UTF8:
    case Unicode:
        if (font_config.source == FileIn)
        {
            file_close(font_config.this);
        }
        if (font_config_wide.source == FileIn)
        {
            file_close(font_config_wide.this);
        }
    case GBK:
    case GB2312:
    case ASCII:
    default:
      // font_init(8, 12, ASCII, BuildIn, ascii);
      break;
  }
}

void font_load(uint8_t index, uint8_t width, uint8_t high, uint8_t source_type, void *src_addr)
{
    switch (index)
    {
    case UTF8:
        // if (src_addr == NULL)
        // {
        //     font_init(8, 12, ASCII, BuildIn, ascii);
        //     break;
        // }
        // font_init(width, high, UTF8, source_type, src_addr);
    break;
    default:
    case Unicode:
    case GBK:
    case GB2312:
    case ASCII:
        // font_init(8, 12, ASCII, BuildIn, ascii);
    break;
    }
}

int font_height()
{
	return font_config_wide.high;
}

// Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
#define UTF8_ACCEPT 0
#define UTF8_REJECT 1

static const uint8_t utf8d[] = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 00..1f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 20..3f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 40..5f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 60..7f
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, // 80..9f
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, // a0..bf
  8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, // c0..df
  0xa,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x4,0x3,0x3, // e0..ef
  0xb,0x6,0x6,0x6,0x5,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8, // f0..ff
  0x0,0x1,0x2,0x3,0x5,0x8,0x7,0x1,0x1,0x1,0x4,0x6,0x1,0x1,0x1,0x1, // s0..s0
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,1, // s1..s2
  1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1, // s3..s4
  1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1, // s5..s6
  1,3,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // s7..s8
};

uint32_t font_utf8_decode_codepoint(uint32_t* state, uint32_t* codep, uint32_t byte)
{
  uint32_t type = utf8d[byte];

  *codep = (*state != UTF8_ACCEPT) ?
    (byte & 0x3fu) | (*codep << 6) :
    (0xff >> type) & (byte);

  *state = utf8d[256 + *state*16 + type];
  return *state;
}

int string_width_px(mp_obj_t str)
{
  int count = 0;
  const uint8_t *s = mp_obj_str_get_str(str);
  uint32_t codepoint;
  uint32_t state = 0;
  for (; *s; ++s)
    if (!font_utf8_decode_codepoint(&state, &codepoint, *s))
      if (codepoint >= WIDEFONT_CODEPOINT_MIN && codepoint <= WIDEFONT_CODEPOINT_MAX)
        count += font_config_wide.width;
      else
        count += font_config.width;
  return count;
}

int string_has_wide_glyph(mp_obj_t str)
{
  const uint8_t *s = mp_obj_str_get_str(str);
  uint32_t codepoint;
  uint32_t state = 0;
  for (; *s; ++s)
    if (!font_utf8_decode_codepoint(&state, &codepoint, *s))
      if (codepoint >= WIDEFONT_CODEPOINT_MIN && codepoint <= WIDEFONT_CODEPOINT_MAX)
        return 1;
  return 0;
}

void imlib_draw_font(image_t *img, int x_off, int y_off, int c, uint8_t font_h, uint8_t font_w, const uint8_t *font)
{
    for (uint8_t y = 0, yy = font_h; y < yy; y++) {
        uint32_t tmp = font[y];
        for (uint8_t i = 1; i < font_w / 8; i++) {
            tmp <<= 8;
            tmp |= font[y + (i * font_h)];
        }
        for (uint8_t x = 0, xx = font_w; x < xx; x++) {
            if (tmp & (1 << (font_w - 1 - x))) {
                imlib_set_pixel(img, (x_off + x), (y_off + y), c);
            }
        }
    }
}

void imlib_draw_string(image_t *img, int x_off, int y_off, mp_obj_t str, int c) {

  // standard font attributes
  const uint8_t font_byte_width = (font_config.width + 7)/8;
  const uint8_t font_len = font_byte_width * font_config.high;
  uint16_t total_codepoints = (((uint8_t *)font_config.this)[0] << 8) | ((uint8_t *)font_config.this)[1];

  // wide font attributes
  const uint8_t font_byte_width_wide = (font_config_wide.width + 7)/8;
  const uint8_t font_len_wide = font_byte_width_wide * font_config_wide.high;
  uint16_t total_codepoints_wide = (((uint8_t *)font_config_wide.this)[0] << 8) | ((uint8_t *)font_config_wide.this)[1];

  const uint8_t *s = mp_obj_str_get_str(str);
  uint32_t codepoint;
  uint32_t state = 0;
  uint16_t cur_codepoint;
  int l;
  int r;
  int m;

  for (; *s; ++s) {
    if (!font_utf8_decode_codepoint(&state, &codepoint, *s)) {
      if (codepoint >= WIDEFONT_CODEPOINT_MIN && codepoint <= WIDEFONT_CODEPOINT_MAX) {
        // Wide glyph codepoints
        uint8_t buffer[font_len_wide];
        l = 0;
        r = total_codepoints_wide - 1;
        while (l <= r) {
            m = l + (r - l) / 2;
            uint32_t byte_index = 2 + m * (2 + font_len_wide);
            cur_codepoint = (((uint8_t *)font_config_wide.this)[byte_index] << 8) | ((uint8_t *)font_config_wide.this)[byte_index + 1];
            if (cur_codepoint == codepoint) {
                memcpy(buffer, &font_config_wide.this[byte_index + 2], font_len_wide);
                break;
            }
            if (cur_codepoint < codepoint)
                l = m + 1;
            else
                r = m - 1;
        }
        const uint8_t *font = buffer;
        imlib_draw_font(img, x_off, y_off, c, font_config_wide.high, font_byte_width_wide * 8, font);
        x_off += font_config_wide.width;
      } else {
        // Standard
        uint8_t buffer[font_len];
        l = 0;
        r = total_codepoints - 1;
        while (l <= r) {
            m = l + (r - l) / 2;
            uint32_t byte_index = 2 + m * (2 + font_len);
            cur_codepoint = (((uint8_t *)font_config.this)[byte_index] << 8) | ((uint8_t *)font_config.this)[byte_index + 1];
            if (cur_codepoint == codepoint) {
                memcpy(buffer, &font_config.this[byte_index + 2], font_len);
                break;
            }
            if (cur_codepoint < codepoint)
                l = m + 1;
            else
                r = m - 1;
        }
        const uint8_t *font = buffer;
        imlib_draw_font(img, x_off, y_off, c, font_config.high, font_byte_width * 8, font);
        x_off += font_config.width;
      }
    }
  }
}
