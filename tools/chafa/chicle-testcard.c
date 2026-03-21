/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2026 derVedro
 *
 * This file is part of Chafa, a program that shows pictures on text terminals.
 *
 * Chafa is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Chafa is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Chafa.  If not, see <http://www.gnu.org/licenses/>. */

#include "config.h"
#include <stdlib.h>
#include <chafa.h>
#include "chicle-testcard.h"
#include "chicle-named-colors.h"
#include "chicle-path-queue.h"
#include "chicle-options.h"

#define TC_HEIGHT           600
#define TC_WIDTH            800
#define TC_CHANNELS         4
#define HF_FIRST            0.75
#define HF_SECOND           0.075
#define HF_THIRD            0.0875
#define HF_LAST             0.0875
#define WF_WB               0.6
#define WF_SPEC             0.1
#define WF_BW               0.5714
#define MOAI_BLOB_SIZE      3262
// #define MOAI_HEIGHT         440
#define MOAI_WIDTH          160
#define MOAI_Y_OFFSET       40
#define MOAI_COLOR_OUTLINE  0,  0,  0,   255
#define MOAI_COLOR_INSIDE   96, 96, 127, 255
#define MOAI_GBOX_V_MIN     14
#define MOAI_GBOX_V_MAX     230
#define MOAI_GBOX_H_MIN     43
#define MOAI_GBOX_H_MAX     84
#define MOAI_G_T_START      0.38f
#define MOAI_G_T_END        1.0f

#define ARRAY_SIZE(arr)     (sizeof(arr) / sizeof(arr[0]))
#define LERPI(a, b, t)      ((a) + (gint)((t) * ((b) - (a))))
#define LERPF(a, b, t)      ((a) + (t) * ((b) - (a)))
#define HF(factor)          (gint) (TC_HEIGHT * (factor))
#define WF(factor)          (gint) (TC_WIDTH * (factor))


typedef struct
{
    size_t x;
    size_t y;
    size_t width;
    size_t height;
}
BBox;

static const unsigned char moai_rle_blob[MOAI_BLOB_SIZE];

static void
draw_bars_internal(const TestcardData *img, const BBox dims, const guint8 *colors, const size_t col_count)
{
    if (col_count == 0) return;

    gfloat stripe_width = (gfloat) dims.width / col_count;

    for (size_t y = dims.y; y < dims.y + dims.height; y++) {
        guint base_row = y * img->width;
        for (size_t x = dims.x; x < dims.x + dims.width; x++) {
            size_t idx = ((size_t)(x / stripe_width)) % col_count;
            const guint8 *c = colors + idx * TC_CHANNELS;
            size_t pos = (base_row + x) * TC_CHANNELS;
            memcpy(img->buffer + pos, c, TC_CHANNELS);
        }
    }
}


static void
draw_bars_of_namedcolors(const TestcardData *img, const BBox dims, const char **color_names, const size_t col_count)
{
    static const ChicleNamedColor default_color = { { 0, 0, 0 }, NULL };

    // This shouldn't be a cause for concern; col_count is small.
    guint8 colors[col_count * TC_CHANNELS];

    for (size_t i = 0; i < col_count; i++) {
        const ChicleNamedColor *color = chicle_find_color_by_name(color_names[i]);
        if (!color) {
            color = &default_color;
        }
        colors[i * TC_CHANNELS]     = color->color[0];
        colors[i * TC_CHANNELS + 1] = color->color[1];
        colors[i * TC_CHANNELS + 2] = color->color[2];
        colors[i * TC_CHANNELS + 3] = 0xFF;
    }

    draw_bars_internal(img, dims, colors, col_count);
}

static void
draw_gradient_bars(const TestcardData *img, const BBox dims, guint segments, const char *start_color, const char *end_color)
{
    const ChicleNamedColor *start = chicle_find_color_by_name(start_color);
    const ChicleNamedColor *end = chicle_find_color_by_name(end_color);

    // in doubt use width
    if (segments == 0 || segments > dims.width) {
        segments = dims.width;
    }

    guint8 gradient_colors[segments * TC_CHANNELS];
    for (size_t i = 0; i < segments; i++) {
        // avoid div by zero
        gfloat t = (segments > 1) ? (gfloat)i / (segments - 1) : 0.0f;
        gradient_colors[i * TC_CHANNELS]     = LERPI(start->color[0], end->color[0], t);
        gradient_colors[i * TC_CHANNELS + 1] = LERPI(start->color[1], end->color[1], t);
        gradient_colors[i * TC_CHANNELS + 2] = LERPI(start->color[2], end->color[2], t);
        gradient_colors[i * TC_CHANNELS + 3] = 0xFF;
    }

    draw_bars_internal(img, dims, gradient_colors, segments);
}

static void
put_moai_rle(const TestcardData *img)
{
    guint y_offset = MOAI_Y_OFFSET;
    guint x_offset = img->width / 2 - MOAI_WIDTH;
    guint8 color_a [4] = {MOAI_COLOR_OUTLINE};
    guint8 color_b [4] = {MOAI_COLOR_INSIDE};
    gboolean outside = TRUE;
    gboolean color_toggle = TRUE;
    BBox m_grad_box = {MOAI_GBOX_H_MIN, MOAI_GBOX_V_MIN, MOAI_GBOX_H_MAX - MOAI_GBOX_H_MIN, MOAI_GBOX_V_MAX - MOAI_GBOX_V_MIN};
    guint8 oddy = img->width % 2 + 1;
    guint m_i = 0;
    guint x = x_offset;
    for (guint i = 0; i < ARRAY_SIZE(moai_rle_blob); i++) {
        color_toggle = !color_toggle;

        for (guint val=0; val < moai_rle_blob[i]; val++) {

            if (m_i % MOAI_WIDTH == 0) {
                y_offset++;
                outside = TRUE;
                x = x_offset;
            }

            guint l_pos = (y_offset * img->width + x) * TC_CHANNELS;
            guint r_pos = ((y_offset + 1) * img->width - x - oddy) * TC_CHANNELS;

            if (color_toggle) {
                outside = FALSE;
                for (guint c = 0; c < TC_CHANNELS; c++) {
                    img->buffer[l_pos + c] = color_a[c];
                    img->buffer[r_pos + c] = color_a[c];
                }
            } else if (!outside) {
                guint m_x_l = x - x_offset;
                guint m_x_r = img->width - x - oddy - x_offset;
                guint m_y = y_offset - MOAI_Y_OFFSET;

                gfloat y_norm = CLAMP((gfloat)(m_y - m_grad_box.y) / (m_grad_box.height), 0.0f, 1.0f);
                gfloat t_l = CLAMP((gfloat)(m_x_l - m_grad_box.x) / (m_grad_box.width), 0.0f, 1.0f);
                gfloat t_r = CLAMP((gfloat)(m_x_r - m_grad_box.x) / (m_grad_box.width), 0.0f, 1.0f);

                gfloat lerp_factor_l = LERPF(MOAI_G_T_START, MOAI_G_T_END, t_l * y_norm);
                gfloat lerp_factor_r = LERPF(MOAI_G_T_START, MOAI_G_T_END, t_r * y_norm);

                for (guint c = 0; c < 3; c++) {
                    img->buffer[l_pos + c] = LERPI(img->buffer[l_pos + c], color_b[c], lerp_factor_l);
                    img->buffer[r_pos + c] = LERPI(img->buffer[r_pos + c], color_b[c], lerp_factor_r);
                }
            }
            m_i++;
            x++;
        }
    }
}

TestcardData *
chicle_test_card_generate(void)
{
    TestcardData *data = g_new0(TestcardData, 1);
    data->width = TC_WIDTH;
    data->height = TC_HEIGHT;
    data->rowstride = data->width * TC_CHANNELS;
    data->pixel_type = CHAFA_PIXEL_RGBA8_UNASSOCIATED;
    data->buffer = g_malloc(data->rowstride * data->height);

    const char *colors_first_block[]  = {"white", "yellow", "cyan", "lime", "fuchsia", "red", "blue"};
    const char *colors_second_block[] = {"blue", "fuchsia", "yellow", "red", "cyan", "black", "white"};

    BBox first_block  = {
            .width  = TC_WIDTH,
            .height = HF(HF_FIRST)
    };
    BBox second_block = {
            .y      = HF(HF_FIRST),
            .width  = TC_WIDTH,
            .height = HF(HF_SECOND)
    };
    BBox third_block_a =  {
            .y      = HF(HF_FIRST+HF_SECOND),
            .width  = WF(WF_WB),
            .height = HF(HF_THIRD)
    };
    BBox third_block_b_1 =  {
            .x      = WF(WF_WB),
            .y      = HF(HF_FIRST+HF_SECOND),
            .width  = WF(WF_SPEC),
            .height = HF(HF_THIRD)
    };
    BBox third_block_b_2 =  {
            .x      = WF(WF_WB + WF_SPEC),
            .y      = HF(HF_FIRST+HF_SECOND),
            .width  = WF(WF_SPEC),
            .height = HF(HF_THIRD)
    };
    BBox third_block_b_3 =  {
            .x      = WF(WF_WB + WF_SPEC * 2),
            .y      = HF(HF_FIRST+HF_SECOND),
            .width  = WF(WF_SPEC),
            .height = HF(HF_THIRD)
    };
    BBox third_block_b_4 =  {
            .x      = WF(WF_WB + WF_SPEC * 3),
            .y      = HF(HF_FIRST + HF_SECOND),
            .width  = WF(WF_SPEC),
            .height = HF(HF_THIRD)
    };
    BBox last_block =  {
            .y      = HF(HF_FIRST + HF_SECOND + HF_THIRD),
            .width  = WF(WF_BW),
            .height = HF(HF_LAST)
    };

    draw_bars_of_namedcolors(data, first_block,  colors_first_block, ARRAY_SIZE(colors_first_block));
    draw_bars_of_namedcolors(data, second_block, colors_second_block, ARRAY_SIZE(colors_second_block));
    draw_gradient_bars(data, third_block_a,   0,  "white",  "black");
    draw_gradient_bars(data, third_block_b_1, 0,  "red",    "yellow");
    draw_gradient_bars(data, third_block_b_2, 0,  "yellow", "cyan");
    draw_gradient_bars(data, third_block_b_3, 0,  "cyan",   "blue");
    draw_gradient_bars(data, third_block_b_4, 0,  "blue",   "red" );
    draw_gradient_bars(data, last_block,      12, "black",  "white");
    put_moai_rle(data);

    return data;
}

void
chicle_test_card_free(TestcardData *data)
{
    if (!data)
        return;
    if (data->buffer)
        g_free(data->buffer);
    g_free(data);
}

static const unsigned char moai_rle_blob[] = {
    157, 3, 157, 3, 157, 3, 157, 3, 157, 3, 157, 3, 51, 109, 48, 112, 47, 113,
    46, 114, 45, 115, 44, 116, 44, 116, 44, 116, 43, 117, 43, 14, 5, 5, 54, 39,
    43, 12, 7, 5, 90, 3, 43, 12, 7, 5, 90, 3, 43, 12, 7, 5, 90, 3, 42, 12, 8,
    5, 90, 3, 42, 12, 8, 5, 90, 3, 42, 12, 8, 5, 90, 3, 42, 12, 8, 5, 90, 3,
    42, 12, 8, 5, 90, 3, 42, 12, 8, 5, 90, 3, 42, 12, 8, 5, 90, 3, 42, 12, 8,
    5, 90, 3, 42, 12, 8, 5, 90, 3, 42, 12, 8, 5, 90, 3, 42, 11, 9, 5, 90, 3,
    42, 11, 9, 5, 90, 3, 42, 11, 9, 5, 90, 3, 42, 11, 9, 5, 90, 3, 41, 12, 9,
    5, 90, 3, 41, 12, 9, 5, 90, 3, 41, 12, 9, 5, 90, 3, 41, 12, 9, 5, 90, 3,
    41, 12, 9, 5, 90, 3, 41, 12, 9, 5, 90, 3, 41, 11, 10, 5, 90, 3, 41, 11, 10,
    5, 90, 3, 41, 11, 10, 5, 90, 3, 41, 11, 10, 5, 90, 3, 41, 11, 10, 5, 90, 3,
    40, 12, 10, 5, 90, 3, 40, 12, 10, 5, 90, 3, 40, 12, 10, 5, 90, 3, 40, 12,
    10, 5, 90, 3, 40, 12, 10, 5, 90, 3, 40, 11, 11, 5, 90, 3, 40, 11, 11, 5,
    90, 3, 40, 11, 11, 5, 90, 3, 40, 11, 11, 5, 90, 3, 40, 11, 11, 5, 77, 2,
    11, 3, 40, 11, 11, 5, 77, 2, 11, 3, 39, 12, 11, 5, 77, 3, 10, 3, 39, 12,
    11, 5, 77, 3, 10, 3, 39, 12, 11, 5, 76, 4, 10, 3, 39, 12, 11, 5, 76, 4, 10,
    3, 39, 12, 11, 5, 76, 4, 10, 3, 39, 11, 12, 5, 76, 4, 10, 3, 39, 11, 12, 5,
    76, 4, 10, 3, 39, 11, 12, 5, 75, 6, 9, 3, 39, 11, 12, 5, 75, 6, 9, 3, 39,
    11, 12, 5, 75, 5, 10, 3, 38, 12, 12, 5, 75, 5, 10, 3, 38, 12, 12, 5, 74, 6,
    10, 3, 37, 13, 12, 5, 74, 6, 10, 3, 35, 15, 12, 5, 74, 6, 10, 3, 32, 18,
    12, 5, 73, 7, 10, 3, 29, 21, 12, 5, 73, 6, 11, 3, 26, 23, 13, 6, 71, 7, 11,
    3, 24, 25, 13, 39, 37, 8, 11, 3, 22, 27, 11, 61, 14, 11, 11, 3, 20, 29, 10,
    87, 11, 3, 19, 30, 9, 88, 11, 3, 18, 31, 8, 88, 12, 3, 17, 32, 8, 88, 12,
    3, 16, 33, 7, 92, 9, 3, 16, 33, 6, 105, 15, 34, 6, 105, 15, 20, 1, 12, 6,
    106, 14, 17, 5, 12, 6, 106, 14, 14, 8, 12, 5, 107, 14, 13, 9, 12, 5, 107,
    14, 11, 11, 12, 5, 107, 13, 12, 11, 12, 4, 108, 13, 12, 11, 12, 4, 108, 13,
    12, 11, 12, 3, 109, 13, 12, 11, 12, 3, 109, 13, 12, 11, 12, 3, 90, 8, 11,
    13, 12, 11, 12, 3, 89, 17, 3, 13, 12, 11, 12, 3, 89, 17, 3, 13, 12, 10, 12,
    3, 90, 17, 3, 13, 11, 11, 12, 3, 90, 17, 3, 13, 11, 11, 12, 3, 90, 17, 3,
    13, 11, 11, 12, 3, 90, 17, 3, 13, 11, 11, 12, 3, 89, 18, 3, 13, 11, 11, 12,
    3, 89, 18, 3, 13, 11, 11, 12, 3, 89, 18, 3, 13, 11, 11, 12, 3, 89, 18, 3,
    13, 11, 11, 12, 3, 89, 18, 3, 13, 11, 11, 12, 4, 88, 18, 3, 13, 11, 11, 12,
    4, 88, 18, 3, 13, 11, 11, 12, 4, 87, 19, 3, 13, 11, 11, 11, 5, 87, 19, 3,
    13, 11, 10, 12, 5, 87, 19, 3, 12, 12, 10, 12, 5, 87, 19, 3, 12, 12, 10, 12,
    5, 87, 19, 3, 12, 12, 10, 12, 6, 86, 19, 3, 12, 12, 10, 12, 6, 86, 19, 3,
    12, 11, 11, 12, 6, 85, 20, 3, 12, 11, 11, 12, 7, 84, 20, 3, 12, 11, 11, 12,
    8, 83, 20, 3, 12, 11, 11, 12, 9, 82, 20, 3, 12, 11, 11, 12, 11, 80, 20, 3,
    12, 11, 11, 12, 16, 75, 20, 3, 12, 11, 11, 11, 17, 5, 34, 36, 20, 3, 12,
    11, 11, 11, 17, 5, 56, 13, 21, 3, 12, 11, 11, 11, 17, 5, 55, 14, 21, 3, 12,
    11, 10, 12, 17, 5, 55, 14, 21, 3, 12, 11, 10, 12, 17, 5, 55, 14, 21, 3, 12,
    11, 10, 12, 17, 5, 55, 14, 21, 3, 12, 11, 10, 12, 17, 5, 54, 15, 21, 3, 12,
    11, 10, 12, 17, 5, 54, 15, 21, 3, 12, 11, 10, 12, 17, 5, 54, 14, 22, 3, 12,
    11, 10, 12, 17, 5, 54, 14, 22, 3, 12, 10, 11, 12, 17, 5, 54, 14, 22, 3, 11,
    11, 11, 12, 17, 5, 54, 14, 22, 3, 11, 11, 11, 12, 17, 5, 53, 15, 22, 3, 11,
    11, 11, 12, 17, 5, 53, 15, 22, 3, 11, 11, 11, 12, 17, 5, 53, 14, 23, 3, 11,
    11, 11, 11, 18, 5, 53, 14, 23, 3, 11, 11, 10, 12, 18, 5, 53, 14, 23, 3, 11,
    11, 10, 12, 18, 5, 52, 15, 23, 3, 11, 11, 10, 12, 18, 5, 52, 15, 23, 3, 11,
    11, 10, 12, 18, 5, 52, 15, 23, 3, 11, 11, 10, 12, 18, 5, 52, 14, 24, 3, 11,
    11, 10, 12, 18, 5, 52, 14, 24, 3, 11, 11, 10, 12, 18, 5, 51, 15, 24, 3, 11,
    11, 10, 12, 18, 5, 51, 15, 24, 3, 11, 11, 10, 12, 18, 5, 51, 15, 24, 3, 11,
    11, 10, 12, 18, 5, 51, 15, 24, 3, 11, 11, 10, 12, 18, 5, 51, 14, 25, 3, 10,
    11, 11, 12, 18, 5, 50, 15, 25, 3, 10, 11, 11, 12, 18, 5, 50, 15, 25, 3, 10,
    11, 11, 12, 18, 5, 50, 15, 25, 3, 10, 11, 11, 100, 25, 3, 10, 11, 11, 100,
    25, 3, 10, 11, 11, 99, 26, 3, 10, 11, 10, 100, 26, 3, 10, 11, 10, 100, 26,
    3, 10, 11, 10, 12, 19, 5, 48, 16, 2, 2, 22, 3, 10, 11, 10, 12, 19, 5, 48,
    16, 1, 3, 22, 3, 10, 11, 10, 12, 19, 5, 48, 16, 1, 3, 22, 3, 10, 11, 10,
    12, 19, 5, 48, 15, 2, 3, 22, 3, 10, 11, 10, 12, 19, 5, 48, 15, 2, 3, 22, 3,
    10, 11, 10, 12, 19, 5, 48, 15, 1, 4, 22, 3, 10, 11, 10, 12, 19, 5, 47, 16,
    1, 4, 22, 3, 10, 11, 10, 12, 19, 5, 47, 16, 1, 4, 22, 3, 9, 12, 10, 12, 19,
    5, 47, 15, 2, 4, 22, 3, 9, 11, 11, 12, 19, 5, 47, 15, 2, 4, 22, 3, 9, 11,
    11, 12, 19, 5, 46, 16, 2, 4, 22, 3, 9, 11, 11, 12, 19, 5, 46, 16, 1, 5, 22,
    3, 9, 11, 11, 12, 19, 5, 46, 15, 2, 5, 22, 3, 9, 11, 11, 12, 19, 5, 46, 15,
    2, 5, 22, 3, 9, 11, 10, 12, 20, 5, 45, 16, 2, 5, 22, 3, 9, 11, 10, 12, 20,
    5, 45, 16, 2, 5, 22, 3, 9, 11, 10, 12, 20, 5, 45, 16, 2, 5, 22, 3, 9, 11,
    10, 12, 20, 5, 45, 15, 3, 5, 22, 3, 9, 11, 10, 12, 20, 5, 45, 15, 2, 6, 22,
    3, 9, 11, 10, 12, 20, 5, 44, 16, 2, 5, 23, 3, 9, 11, 10, 12, 20, 5, 44, 16,
    2, 5, 23, 3, 9, 11, 10, 12, 20, 5, 44, 15, 3, 5, 23, 3, 9, 11, 10, 12, 20,
    5, 44, 15, 3, 5, 23, 3, 9, 11, 10, 12, 20, 5, 43, 16, 3, 5, 23, 3, 9, 10,
    11, 12, 20, 5, 43, 15, 4, 5, 23, 3, 9, 10, 11, 12, 20, 5, 43, 15, 3, 6, 23,
    3, 8, 11, 11, 12, 20, 5, 43, 15, 3, 6, 23, 3, 8, 11, 11, 12, 20, 5, 42, 15,
    4, 6, 23, 3, 8, 11, 11, 12, 20, 5, 42, 15, 4, 6, 23, 3, 8, 11, 11, 12, 20,
    5, 42, 14, 5, 6, 23, 3, 8, 11, 11, 12, 20, 5, 41, 15, 5, 6, 23, 3, 8, 11,
    11, 12, 20, 5, 41, 14, 6, 5, 24, 3, 8, 11, 11, 12, 20, 5, 40, 15, 6, 5, 24,
    3, 8, 11, 11, 12, 20, 5, 39, 15, 7, 5, 24, 3, 8, 11, 11, 12, 20, 5, 38, 15,
    8, 5, 24, 3, 8, 11, 11, 12, 20, 5, 37, 16, 8, 5, 17, 10, 8, 11, 11, 12, 20,
    5, 36, 16, 9, 4, 18, 10, 8, 11, 11, 12, 20, 5, 35, 16, 12, 1, 19, 10, 8,
    11, 11, 12, 20, 5, 34, 16, 33, 10, 8, 11, 11, 12, 20, 5, 33, 16, 34, 10, 8,
    11, 11, 12, 20, 5, 33, 15, 42, 3, 8, 10, 11, 13, 20, 5, 32, 15, 43, 3, 8,
    10, 11, 13, 20, 5, 31, 15, 44, 3, 7, 11, 11, 13, 20, 5, 30, 16, 44, 3, 7,
    11, 11, 13, 20, 5, 30, 15, 45, 3, 7, 11, 11, 13, 20, 5, 29, 15, 46, 3, 7,
    11, 11, 13, 20, 5, 29, 15, 46, 3, 7, 11, 11, 13, 20, 5, 29, 14, 47, 3, 7,
    11, 11, 13, 20, 5, 28, 15, 47, 3, 7, 11, 11, 13, 20, 5, 28, 14, 48, 3, 7,
    11, 11, 13, 20, 5, 28, 14, 48, 3, 7, 11, 11, 13, 20, 5, 27, 15, 48, 3, 7,
    11, 11, 13, 20, 5, 27, 15, 17, 9, 22, 3, 7, 11, 11, 13, 20, 5, 27, 15, 15,
    14, 19, 3, 7, 11, 11, 13, 20, 5, 27, 15, 13, 18, 17, 3, 7, 11, 11, 13, 20,
    5, 27, 15, 12, 20, 16, 3, 7, 11, 11, 13, 20, 5, 26, 16, 11, 23, 14, 3, 7,
    11, 11, 13, 20, 5, 26, 17, 9, 25, 13, 3, 6, 12, 11, 13, 20, 5, 26, 17, 9,
    27, 11, 3, 6, 11, 12, 13, 20, 5, 26, 18, 7, 30, 9, 3, 6, 11, 12, 13, 20, 5,
    26, 19, 5, 43, 6, 11, 12, 13, 20, 5, 26, 67, 6, 11, 12, 13, 20, 5, 27, 66,
    6, 11, 12, 13, 20, 5, 27, 66, 6, 11, 12, 13, 20, 5, 27, 66, 6, 11, 12, 13,
    20, 5, 27, 66, 6, 11, 12, 13, 20, 5, 27, 66, 6, 11, 12, 13, 20, 5, 28, 65,
    6, 11, 12, 13, 20, 5, 28, 65, 6, 11, 12, 13, 20, 5, 28, 65, 6, 11, 12, 13,
    20, 5, 28, 65, 6, 11, 12, 13, 20, 5, 29, 64, 6, 11, 11, 14, 20, 5, 29, 64,
    6, 11, 11, 14, 20, 5, 29, 64, 6, 11, 11, 14, 20, 5, 30, 63, 6, 11, 11, 14,
    20, 5, 30, 63, 6, 11, 11, 14, 20, 5, 31, 62, 6, 11, 11, 14, 20, 5, 31, 62,
    5, 12, 11, 14, 20, 5, 32, 61, 5, 12, 11, 14, 20, 5, 33, 60, 5, 11, 12, 14,
    20, 5, 33, 60, 5, 11, 12, 14, 20, 5, 34, 59, 5, 11, 12, 14, 20, 5, 35, 58,
    5, 11, 12, 14, 20, 5, 36, 57, 5, 11, 12, 14, 20, 5, 38, 55, 5, 11, 12, 14,
    20, 5, 39, 54, 5, 11, 12, 14, 20, 5, 41, 52, 5, 11, 12, 14, 20, 5, 43, 50,
    5, 11, 12, 14, 20, 5, 48, 10, 3, 32, 5, 12, 11, 14, 20, 5, 65, 28, 5, 12,
    11, 14, 20, 5, 67, 26, 5, 13, 10, 14, 20, 5, 68, 25, 5, 15, 8, 14, 20, 5,
    70, 23, 5, 16, 7, 14, 20, 5, 71, 22, 6, 17, 5, 14, 20, 5, 72, 21, 6, 20, 2,
    14, 20, 5, 74, 19, 7, 35, 20, 5, 75, 18, 7, 35, 20, 5, 77, 16, 8, 34, 20,
    5, 80, 13, 9, 33, 20, 5, 86, 7, 10, 32, 20, 5, 90, 3, 12, 30, 20, 5, 90, 3,
    13, 29, 20, 5, 90, 3, 15, 27, 20, 5, 90, 3, 17, 25, 20, 5, 90, 3, 20, 94,
    43, 3, 22, 92, 43, 3, 25, 89, 43, 3, 27, 87, 43, 3, 27, 87, 43, 3, 28, 14,
    20, 5, 90, 3, 28, 14, 20, 5, 90, 3, 28, 14, 20, 5, 90, 3, 28, 14, 20, 5,
    90, 3, 28, 14, 20, 5, 90, 3, 28, 14, 20, 5, 90, 3, 28, 14, 20, 5, 90, 3,
    28, 14, 20, 5, 90, 3, 28, 14, 20, 5, 90, 3, 28, 14, 20, 5, 79, 14, 28, 14,
    20, 5, 71, 22, 28, 14, 20, 5, 65, 28, 28, 14, 20, 5, 60, 33, 28, 14, 20, 5,
    56, 37, 28, 14, 20, 5, 52, 41, 28, 14, 20, 5, 49, 44, 28, 14, 20, 5, 46,
    47, 28, 14, 20, 5, 43, 50, 27, 15, 20, 5, 40, 53, 27, 15, 20, 5, 37, 56,
    27, 15, 20, 5, 34, 59, 27, 15, 20, 5, 32, 61, 27, 15, 20, 5, 30, 63, 27,
    15, 20, 5, 28, 65, 27, 15, 20, 5, 27, 56, 7, 3, 27, 15, 20, 5, 26, 44, 20,
    3, 27, 15, 20, 5, 25, 37, 28, 3, 27, 15, 20, 5, 24, 30, 36, 3, 27, 15, 20,
    5, 24, 25, 41, 3, 27, 15, 20, 5, 25, 20, 45, 3, 27, 15, 20, 5, 25, 15, 50,
    3, 27, 15, 20, 5, 26, 10, 54, 3, 27, 15, 20, 5, 90, 3, 27, 15, 20, 5, 90,
    3, 26, 16, 20, 5, 90, 3, 26, 17, 19, 5, 90, 3, 26, 17, 19, 5, 90, 3, 26,
    17, 19, 5, 90, 3, 26, 17, 19, 5, 90, 3, 26, 17, 19, 5, 70, 23, 26, 17, 19,
    5, 64, 29, 26, 17, 19, 5, 63, 30, 26, 17, 19, 5, 63, 30, 26, 17, 19, 5, 63,
    30, 26, 17, 19, 5, 64, 29, 26, 17, 19, 5, 68, 25, 25, 18, 19, 5, 74, 19,
    25, 18, 19, 5, 90, 3, 25, 18, 19, 5, 90, 3, 25, 18, 19, 5, 90, 3, 25, 18,
    19, 5, 90, 3, 25, 18, 19, 5, 90, 3, 25, 18, 19, 5, 90, 3, 25, 18, 19, 5,
    90, 3, 25, 19, 18, 5, 90, 3, 25, 19, 18, 5, 90, 3, 25, 19, 18, 5, 90, 3,
    24, 20, 18, 5, 90, 3, 24, 10, 1, 9, 18, 5, 90, 3, 24, 10, 1, 9, 18, 5, 90,
    3, 24, 10, 1, 9, 18, 5, 90, 3, 24, 10, 1, 9, 18, 5, 90, 3, 24, 10, 1, 9,
    18, 5, 90, 3, 24, 10, 1, 9, 18, 5, 90, 3, 23, 11, 1, 9, 18, 5, 90, 3, 23,
    11, 1, 10, 17, 5, 90, 3, 23, 11, 1, 10, 17, 5, 90, 3, 23, 10, 2, 10, 17, 5,
    90, 3, 23, 10, 2, 10, 17, 5, 90, 3, 23, 10, 2, 10, 17, 5, 90, 3, 23, 10, 2,
    10, 17, 5, 90, 3, 22, 11, 2, 10, 17, 5, 90, 3, 22, 11, 3, 9, 17, 5, 90, 3,
    22, 11, 3, 9, 17, 5, 90, 3, 22, 11, 3, 9, 17, 5, 90, 3, 22, 11, 3, 9, 17,
    5, 90, 3, 22, 10, 4, 124, 22, 10, 4, 124, 22, 10, 4, 124, 21, 11, 4, 124,
    21, 11, 4, 124, 21, 11, 4, 124, 21, 11, 4, 10, 16, 5, 90, 3, 21, 11, 4, 10,
    16, 5, 90, 3, 21, 11, 4, 10, 16, 5, 90, 3, 21, 10, 5, 11, 15, 5, 90, 3, 21,
    10, 5, 11, 15, 5, 90, 3, 21, 10, 6, 10, 15, 5, 90, 3, 21, 10, 6, 10, 15, 5,
    90, 3, 20, 11, 6, 10, 15, 5, 90, 3, 20, 11, 6, 10, 15, 5, 90, 3, 20, 11, 6,
    10, 15, 5, 90, 3, 20, 11, 6, 10, 15, 5, 90, 3, 20, 11, 6, 10, 15, 5, 90, 3,
    20, 11, 6, 11, 14, 5, 90, 3, 20, 10, 7, 11, 14, 5, 90, 3, 20, 10, 7, 11,
    14, 5, 90, 3, 19, 11, 7, 11, 14, 5, 90, 3, 19, 11, 7, 11, 14, 5, 90, 3, 19,
    11, 8, 10, 14, 5, 90, 3, 19, 11, 8, 10, 14, 5, 90, 3, 19, 11, 8, 10, 14, 5,
    90, 3, 19, 11, 8, 11, 13, 5, 90, 3, 19, 11, 8, 11, 13, 5, 90, 3, 19, 10, 9,
    11, 13, 5, 90, 3, 19, 10, 9, 11, 13, 5, 90, 3, 18, 11, 9, 11, 13, 5, 90, 3,
    18, 11, 9, 11, 13, 5, 90, 3, 18, 11, 9, 12, 12, 5, 90, 3, 18, 11, 10, 11,
    12, 5, 90, 3, 18, 11, 10, 11, 12, 5, 90, 3, 18, 11, 10, 11, 12, 5, 90, 3,
    18, 11, 10, 11, 12, 5, 90, 3, 18, 10, 11, 12, 11, 5, 90, 3, 18, 10, 11, 12,
    11, 5, 90, 3, 18, 10, 11, 12, 11, 5, 90, 3, 17, 11, 11, 12, 11, 5, 90, 3,
    17, 11, 11, 12, 11, 5, 90, 3, 17, 11, 12, 12, 10, 5, 90, 3, 17, 11, 12, 12,
    10, 5, 90, 3, 17, 11, 12, 12, 10, 5, 90, 3, 17, 11, 12, 13, 9, 5, 90, 3,
    17, 11, 12, 13, 9, 5, 90, 3, 17, 10, 13, 13, 9, 5, 90, 3, 17, 10, 13, 14,
    8, 5, 90, 3, 17, 10, 13, 14, 8, 5, 90, 3, 16, 11, 13, 14, 8, 5, 90, 3, 16,
    11, 13, 15, 7, 5, 90, 3, 16, 11, 13, 15, 7, 5, 90, 3, 16, 11, 14, 15, 6, 5,
    90, 3, 16, 11, 14, 15, 6, 5, 90, 3, 16, 11, 14, 16, 5, 5, 90, 3, 16, 11,
    14, 17, 4, 5, 90, 3, 16, 10, 15, 17, 4, 5, 90, 3, 16, 10, 15, 18, 3, 5, 90,
    3, 16, 10, 15, 19, 2, 5, 90, 3, 16, 10, 15, 20, 1, 5, 90, 3, 16, 10, 15,
    26, 90, 3, 15, 11, 15, 26, 90, 3, 15, 11, 15, 26, 90, 3, 15, 11, 16, 26,
    89, 3, 15, 11, 16, 28, 87, 3, 15, 11, 16, 30, 85, 3, 15, 11, 16, 33, 82, 3,
    15, 11, 16, 35, 80, 3, 15, 10, 17, 39, 76, 3, 15, 10, 17, 43, 72, 3, 15,
    10, 17, 47, 68, 3, 15, 10, 17, 51, 64, 3, 14, 11, 17, 56, 59, 3, 14, 11,
    17, 63, 52, 3, 14, 11, 17, 71, 44, 3, 14, 11, 17, 83, 32, 3, 14, 11, 18,
    106, 4, 7, 14, 11, 18, 117, 14, 11, 18, 117, 14, 11, 18, 117, 14, 11, 18,
    117, 14, 11, 18, 117, 14, 11, 17, 118, 14, 12, 16, 118, 14, 13, 15, 118,
    14, 17, 11, 118, 15, 145, 15, 145, 15, 145, 16, 144, 17, 143, 18, 142, 19,
    141, 21, 139, 23, 137, 26, 134, 33, 127, 49, 111, 73, 87, 99, 61, 133, 27
};