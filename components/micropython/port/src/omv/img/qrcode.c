/*
 * This file is part of the OpenMV project.
 *
 * Copyright (C) 2010-2012 Daniel Beer <dlbeer@gmail.com>
 * Copyright (c) 2013-2021 Ibrahim Abdelkader <iabdalkader@openmv.io>
 * Copyright (c) 2013-2021 Kwabena W. Agyeman <kwagyeman@openmv.io>
 *
 * This work is licensed under the MIT license, see the file LICENSE for details.
 *
 * QR-code recognition library.
 *
 * Detection is delegated to the k_quirc library
 * (components/micropython/port/src/k_quirc).
 */
#include "imlib.h"
#include "framebuffer.h"
#include "k_quirc.h"
#ifdef IMLIB_ENABLE_QRCODES

// Dual-core support for parallel processing
typedef int (*dual_func_t)(int);
extern volatile dual_func_t dual_func;

// Globals for dual-core RGB565 to grayscale conversion (core 1 path)
static uint8_t *g_gray_dst = NULL;
static uint16_t *g_rgb_src = NULL;
static int g_rgb_y_start = 0;
static int g_rgb_y_end = 0;
static int g_rgb_width = 0;
static int g_rgb_x_start = 0;
static int g_rgb_x_end = 0;

static int rgb565_to_gray_core1(int core)
{
    for (int y = g_rgb_y_start; y < g_rgb_y_end; y++) {
        uint16_t *row_ptr = g_rgb_src + y * g_rgb_width;
        uint8_t *dst_ptr = g_gray_dst + ((y - g_rgb_y_start) * (g_rgb_x_end - g_rgb_x_start));
        for (int x = g_rgb_x_start; x < g_rgb_x_end; x++) {
            *dst_ptr++ = COLOR_RGB565_TO_GRAYSCALE(row_ptr[x]);
        }
    }
    return 0;
}

void imlib_find_qrcodes(list_t *out, image_t *ptr, rectangle_t *roi, bool find_inverted)
{
    k_quirc_t *controller = k_quirc_new();
    k_quirc_resize(controller, roi->w, roi->h);
    uint8_t *grayscale_image = k_quirc_begin(controller, NULL, NULL);

    switch (ptr->bpp) {
        case IMAGE_BPP_BINARY: {
            for (int y = roi->y, yy = roi->y + roi->h; y < yy; y++) {
                uint32_t *row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(ptr, y);
                for (int x = roi->x, xx = roi->x + roi->w; x < xx; x++) {
                    *(grayscale_image++) = COLOR_BINARY_TO_GRAYSCALE(IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, x));
                }
            }
            break;
        }
        case IMAGE_BPP_GRAYSCALE: {
            for (int y = roi->y, yy = roi->y + roi->h; y < yy; y++) {
                uint8_t *row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(ptr, y);
                for (int x = roi->x, xx = roi->x + roi->w; x < xx; x++) {
                    *(grayscale_image++) = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr, x);
                }
            }
            break;
        }
        case IMAGE_BPP_RGB565: {
            // Zero-copy fast path: full-frame scans can use the camera's pre-extracted grayscale buffer
            if (MAIN_FB()->grayscale &&
                roi->x == 0 && roi->y == 0 &&
                roi->w == MAIN_FB()->w && roi->h == MAIN_FB()->h) {
                memcpy(grayscale_image, MAIN_FB()->grayscale, roi->w * roi->h);
            } else {
                int half_h = roi->h / 2;

                g_rgb_src = (uint16_t *)ptr->pixels;
                g_rgb_width = ptr->w;
                g_rgb_x_start = roi->x;
                g_rgb_x_end = roi->x + roi->w;
                g_rgb_y_start = roi->y + half_h;
                g_rgb_y_end = roi->y + roi->h;
                g_gray_dst = grayscale_image + (half_h * roi->w);

                dual_func = rgb565_to_gray_core1;

                for (int y = roi->y, yy = roi->y + half_h; y < yy; y++) {
                    uint16_t *row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(ptr, y);
                    for (int x = roi->x, xx = roi->x + roi->w; x < xx; x++) {
                        *(grayscale_image++) = COLOR_RGB565_TO_GRAYSCALE(IMAGE_GET_RGB565_PIXEL_FAST(row_ptr, x));
                    }
                }

                while (dual_func) { }
            }
            break;
        }
        default: {
            memset(grayscale_image, 0, roi->w * roi->h);
            break;
        }
    }

    k_quirc_end(controller, find_inverted);
    list_init(out, sizeof(find_qrcodes_list_lnk_data_t));

    int num_codes = k_quirc_count(controller);
    for (int i = 0; i < num_codes; i++) {
        k_quirc_result_t result;
        if (k_quirc_decode(controller, i, &result) != K_QUIRC_SUCCESS || !result.valid) {
            continue;
        }

        find_qrcodes_list_lnk_data_t lnk_data;
        rectangle_init(&lnk_data.rect,
                       result.corners[0].x + roi->x,
                       result.corners[0].y + roi->y, 0, 0);
        for (size_t k = 1, l = (sizeof(result.corners) / sizeof(result.corners[0])); k < l; k++) {
            rectangle_t temp;
            rectangle_init(&temp, result.corners[k].x + roi->x, result.corners[k].y + roi->y, 0, 0);
            rectangle_united(&lnk_data.rect, &temp);
        }

        lnk_data.corners[0].x = result.corners[0].x + roi->x; // top-left
        lnk_data.corners[0].y = result.corners[0].y + roi->y;
        lnk_data.corners[1].x = result.corners[1].x + roi->x; // top-right
        lnk_data.corners[1].y = result.corners[1].y + roi->y;
        lnk_data.corners[2].x = result.corners[2].x + roi->x; // bottom-right
        lnk_data.corners[2].y = result.corners[2].y + roi->y;
        lnk_data.corners[3].x = result.corners[3].x + roi->x; // bottom-left
        lnk_data.corners[3].y = result.corners[3].y + roi->y;

        lnk_data.payload_len = result.data.payload_len;
        lnk_data.payload = xalloc(result.data.payload_len);
        memcpy(lnk_data.payload, result.data.payload, result.data.payload_len);

        lnk_data.version = result.data.version;
        lnk_data.ecc_level = result.data.ecc_level;
        lnk_data.mask = result.data.mask;
        lnk_data.data_type = result.data.data_type;
        lnk_data.eci = result.data.eci;

        list_push_back(out, &lnk_data);
    }

    k_quirc_destroy(controller);
}
#endif //IMLIB_ENABLE_QRCODES
