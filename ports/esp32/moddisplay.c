/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * Development of the code in this file was sponsored by Microbric Pty Ltd
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Paul Sokolovsky
 * Copyright (c) 2016 Damien P. George
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

#include <stdio.h>

#include "rom/gpio.h"
#include "esp_log.h"

#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "driver/spi_master.h"
#include "lcd_interface.h"

#define COLOR_TO_MTK_COLOR_SIMUL(color) ((((color) >> 19) & 0x1f) << 11) \
                                            |((((color) >> 10) & 0x3f) << 5) \
                                            |(((color) >> 3) & 0x1f)
                                            
static uint16_t rgb888to565(uint32_t color888)
{
	uint16_t color565 = COLOR_TO_MTK_COLOR_SIMUL(color888);
	return color565;
}

const mp_obj_type_t display_lcd_type;

typedef struct _machine_hw_spi_obj_t {
    mp_obj_base_t base;
    spi_host_device_t host;
    uint32_t baudrate;
    uint8_t polarity;
    uint8_t phase;
    uint8_t bits;
    uint8_t firstbit;
    int8_t sck;
    int8_t mosi;
    int8_t miso;
    spi_device_handle_t spi;
    enum {
        MACHINE_HW_SPI_STATE_NONE,
        MACHINE_HW_SPI_STATE_INIT,
        MACHINE_HW_SPI_STATE_DEINIT
    } state;
} machine_hw_spi_obj_t;

typedef struct _display_lcd_obj_t{
    mp_obj_base_t base;
    lcd_handle_t *lcd_obj;
    machine_hw_spi_obj_t *spi;
} display_lcd_obj_t;


STATIC void display_lcd_init_internal(
    display_lcd_obj_t      *self,
    int8_t                  host,
    int8_t                  init_bus) 
{
    self->lcd_obj = lcd_create_obj(host, init_bus);
}

//-----------------------------------------------------------------------------------------------
STATIC mp_obj_t display_lcd_setCursor(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_x,            MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y,            MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
    };
    display_lcd_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

  	int16_t x = args[0].u_int;
    int16_t y = args[1].u_int;

    lcd_setCursor(self->lcd_obj, x, y);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(display_lcd_setCursor_obj, 2, display_lcd_setCursor);

//-------------------------------------------------------------------------------------------------
STATIC mp_obj_t display_lcd_getCursor(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args)
{
    mp_obj_t tuple[2];
    display_lcd_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);

    tuple[0] = mp_obj_new_int(lcd_getCursorX(self->lcd_obj));
    tuple[1] = mp_obj_new_int(lcd_getCursorY(self->lcd_obj));

    return mp_obj_new_tuple(2, tuple);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(display_lcd_getCursor_obj, 0, display_lcd_getCursor);

//-----------------------------------------------------------------------------------------------
STATIC mp_obj_t display_lcd_setRotation(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_rotation, MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
    };
    display_lcd_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

  	uint8_t rotation = args[0].u_int;
  	if ((rotation < 0) || (rotation > 3)) rotation = 0;

  	lcd_setRotation(self->lcd_obj, rotation);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(display_lcd_setRotation_obj, 1, display_lcd_setRotation);

//-------------------------------------------------------------------------------------------------
STATIC mp_obj_t display_lcd_setTextColor(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_color,                    MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_bcolor,                   MP_ARG_INT, { .u_int = -1 } },
    };
    display_lcd_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    uint32_t color = ILI9341_WHITE;
    if (args[0].u_int >= 0) {
        color = (args[0].u_int);
    }
    lcd_setTextColor(self->lcd_obj, rgb888to565(color));
    if (args[1].u_int >= 0) {
        lcd_setTextbgColor(self->lcd_obj, color, rgb888to565((args[1].u_int)));
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(display_lcd_setTextColor_obj, 0, display_lcd_setTextColor);

//----------------------------------------------------------------
STATIC mp_obj_t display_lcd_drawPixel(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_x,     MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y,     MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_color,                   MP_ARG_INT, { .u_int = -1 } },
    };
    display_lcd_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);

	mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    uint32_t color = ILI9341_WHITE;
	mp_int_t x = args[0].u_int;
    mp_int_t y = args[1].u_int;
    if (args[2].u_int >= 0) {
        color = (args[2].u_int);
    }
    lcd_drawPixel(self->lcd_obj, x, y, rgb888to565(color));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(display_lcd_drawPixel_obj, 3, display_lcd_drawPixel);

//----------------------------------------------------------------
STATIC mp_obj_t display_lcd_fillScreen(mp_obj_t self_in, mp_obj_t color) {
    display_lcd_obj_t *self = self_in;
    uint32_t c = mp_obj_get_int(color);
    lcd_fillScreen(self->lcd_obj, rgb888to565(c));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(display_lcd_fillScreen_obj, display_lcd_fillScreen);

STATIC mp_obj_t display_lcd_clear(mp_obj_t self_in) {
    display_lcd_obj_t *self = self_in;
    lcd_fillScreen(self->lcd_obj, rgb888to565(ILI9341_BLACK));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(display_lcd_clear_obj, display_lcd_clear);

STATIC mp_obj_t display_lcd_drawLine(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_x,     MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y,     MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_x1,     MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y1,     MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_color,                    MP_ARG_INT, { .u_int = -1 } },
    };
    display_lcd_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);

	mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    uint32_t color = ILI9341_WHITE;
	mp_int_t x0 = args[0].u_int;
    mp_int_t y0 = args[1].u_int;
	mp_int_t x1 = args[2].u_int;
    mp_int_t y1 = args[3].u_int;
    if(args[4].u_int >= 0){
        color = args[4].u_int;
    }
    lcd_drawLine(self->lcd_obj, x0, y0, x1, y1, rgb888to565(color));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(display_lcd_drawLine_obj, 5, display_lcd_drawLine);

STATIC mp_obj_t display_lcd_drawTriangle(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_x,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_x1,     MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y1,     MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_x2,     MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y2,     MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_color,                    MP_ARG_INT, { .u_int = -1 } },
    };
    display_lcd_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    uint32_t color = ILI9341_WHITE;   
	mp_int_t x0 = args[0].u_int;
    mp_int_t y0 = args[1].u_int;
	mp_int_t x1 = args[2].u_int;
    mp_int_t y1 = args[3].u_int;
	mp_int_t x2 = args[4].u_int;
    mp_int_t y2 = args[5].u_int;
    if (args[6].u_int >= 0) 
    {
        color = args[6].u_int;     
    }
    lcd_drawTriangle(self->lcd_obj, x0, y0, x1, y1, x2, y2, rgb888to565(color));    
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(display_lcd_drawTriangle_obj, 6, display_lcd_drawTriangle);

STATIC mp_obj_t display_lcd_fillTriangle(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_x,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_x1,     MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y1,     MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_x2,     MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y2,     MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_fillcolor,                    MP_ARG_INT, { .u_int = -1 } },
    };
    display_lcd_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    uint32_t fillcolor = ILI9341_WHITE;   
	mp_int_t x0 = args[0].u_int;
    mp_int_t y0 = args[1].u_int;
	mp_int_t x1 = args[2].u_int;
    mp_int_t y1 = args[3].u_int;
	mp_int_t x2 = args[4].u_int;
    mp_int_t y2 = args[5].u_int;
    if (args[6].u_int >= 0) 
    {
        fillcolor = args[6].u_int;     
    }
    lcd_fillTriangle(self->lcd_obj, x0, y0, x1, y1, x2, y2, rgb888to565(fillcolor));    
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(display_lcd_fillTriangle_obj, 6, display_lcd_fillTriangle);

STATIC mp_obj_t display_lcd_drawCircle(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_x,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_r,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_color,                    MP_ARG_INT, { .u_int = -1 } },
    };
    display_lcd_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    uint32_t color = ILI9341_WHITE;   
	mp_int_t x = args[0].u_int;
    mp_int_t y = args[1].u_int;
	mp_int_t r = args[2].u_int;
    if (args[3].u_int >= 0) 
    {
        color = args[3].u_int;     
    }
    lcd_drawCircle(self->lcd_obj, x, y, r, rgb888to565(color));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(display_lcd_drawCircle_obj, 3, display_lcd_drawCircle);

STATIC mp_obj_t display_lcd_fillCircle(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_x,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_r,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_fillcolor,                    MP_ARG_INT, { .u_int = -1 } },
    };
    display_lcd_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    uint32_t fillcolor = ILI9341_WHITE;   
	mp_int_t x = args[0].u_int;
    mp_int_t y = args[1].u_int;
	mp_int_t r = args[2].u_int;
    if (args[3].u_int >= 0) 
    {
        fillcolor = args[3].u_int;     
    }
    lcd_fillCircle(self->lcd_obj, x, y, r, rgb888to565(fillcolor));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(display_lcd_fillCircle_obj, 3, display_lcd_fillCircle);

STATIC mp_obj_t display_lcd_drawRect(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_x,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_w,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_h,   MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_color,                    MP_ARG_INT, { .u_int = -1 } },
    };
    display_lcd_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    uint32_t color = ILI9341_WHITE;   
	mp_int_t x = args[0].u_int;
    mp_int_t y = args[1].u_int;
	mp_int_t w = args[2].u_int;
    mp_int_t h = args[3].u_int;
    if (args[4].u_int >= 0) 
    {
        color = args[4].u_int;     
    }
    lcd_drawRect(self->lcd_obj, x, y, w, h, rgb888to565(color));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(display_lcd_drawRect_obj, 4, display_lcd_drawRect);

STATIC mp_obj_t display_lcd_fillRect(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_x,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_w,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_h,   MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_fillcolor,                    MP_ARG_INT, { .u_int = -1 } },
    };
    display_lcd_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    uint32_t fillcolor = ILI9341_WHITE;   
	mp_int_t x = args[0].u_int;
    mp_int_t y = args[1].u_int;
	mp_int_t w = args[2].u_int;
    mp_int_t h = args[3].u_int;
    if (args[4].u_int >= 0) 
    {
        fillcolor = args[4].u_int;     
    }
    lcd_fillRect(self->lcd_obj, x, y, w, h, rgb888to565(fillcolor));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(display_lcd_fillRect_obj, 4, display_lcd_fillRect);

STATIC mp_obj_t display_lcd_drawRoundRect(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_x,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_w,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_h,   MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_r,   MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_color,                    MP_ARG_INT, { .u_int = -1 } },
    };
    display_lcd_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    uint32_t color = ILI9341_WHITE;   
	mp_int_t x = args[0].u_int;
    mp_int_t y = args[1].u_int;
	mp_int_t w = args[2].u_int;
    mp_int_t h = args[3].u_int;
    mp_int_t r = args[4].u_int;
    if (args[5].u_int >= 0) 
    {
        color = args[5].u_int;     
    }
    lcd_drawRoundRect(self->lcd_obj, x, y, w, h, r, rgb888to565(color));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(display_lcd_drawRoundRect_obj, 5, display_lcd_drawRoundRect);

STATIC mp_obj_t display_lcd_fillRoundRect(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_x,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_w,      MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_h,   MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0}},
        { MP_QSTR_r,   MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0}},
        { MP_QSTR_fillcolor,                    MP_ARG_INT, { .u_int = -1 } },
    };
    display_lcd_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    uint32_t fillcolor = ILI9341_WHITE;   
	mp_int_t x = args[0].u_int;
    mp_int_t y = args[1].u_int;
	mp_int_t w = args[2].u_int;
    mp_int_t h = args[3].u_int;
    mp_int_t r = args[4].u_int;
    if (args[5].u_int >= 0) 
    {
        fillcolor = args[5].u_int;     
    }
    lcd_fillRoundRect(self->lcd_obj, x, y, w, h, r, rgb888to565(fillcolor));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(display_lcd_fillRoundRect_obj, 5, display_lcd_fillRoundRect);

STATIC mp_obj_t display_lcd_drawString(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    const mp_arg_t allowed_args[] = {
        { MP_QSTR_text,         MP_ARG_REQUIRED | MP_ARG_OBJ, { .u_obj = mp_const_none } },
        { MP_QSTR_x,                              MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_y,                              MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_color,                          MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_rotate,                         MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_transparent,  MP_ARG_KW_ONLY  | MP_ARG_OBJ, { .u_obj = mp_const_none } },
        { MP_QSTR_fixedwidth,   MP_ARG_KW_ONLY  | MP_ARG_OBJ, { .u_obj = mp_const_none } },
        { MP_QSTR_wrap,         MP_ARG_KW_ONLY  | MP_ARG_OBJ, { .u_obj = mp_const_none } },
    };
    display_lcd_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_int_t x = lcd_getCursorX(self->lcd_obj);
    mp_int_t y = lcd_getCursorY(self->lcd_obj);
    if(args[1].u_int >= 0){
        x = args[1].u_int;
    }
    if(args[2].u_int >= 0){
        y = args[2].u_int;
    }
    char *st = (char *)mp_obj_str_get_str(args[0].u_obj);

    if (args[3].u_int >= 0) {
    	lcd_setTextColor(self->lcd_obj, rgb888to565((args[3].u_int)));
    }
    // else{
    // 	lcd_setTextColor(self->lcd_obj, rgb888to565(ILI9341_WHITE));
    // }
    // if (args[4].u_int >= 0) font_rotate = args[4].u_int;
    // if (mp_obj_is_integer(args[5].u_obj)) font_transparent = args[5].u_int;
    // if (mp_obj_is_integer(args[6].u_obj)) font_forceFixed = args[6].u_int;
    // if (mp_obj_is_integer(args[7].u_obj)) text_wrap = args[7].u_int;

    lcd_drawString(self->lcd_obj, st, x, y);
    

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(display_lcd_drawString_obj, 1, display_lcd_drawString);

//================================================================
mp_obj_t display_lcd_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum { ARG_spi};
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_spi,      MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    display_lcd_obj_t *self = m_new_obj(display_lcd_obj_t);
    self->base.type = &display_lcd_type;

    machine_hw_spi_obj_t* spi = (machine_hw_spi_obj_t*)(args[ARG_spi].u_obj);
    if (spi) {
        display_lcd_init_internal(self, spi->host, false);
    } else {
        display_lcd_init_internal(self, 1, true);
    }
    // TODO: rotation: 0~3 need be changed to 0~360
    lcd_setRotation(self->lcd_obj, 1);

    return MP_OBJ_FROM_PTR(self);
}

//================================================================
STATIC const mp_rom_map_elem_t display_lcd_locals_dict_table[] = {
    // instance methods
    // { MP_ROM_QSTR(MP_QSTR_init),				MP_ROM_PTR(&display_lcd_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_setCursor),           MP_ROM_PTR(&display_lcd_setCursor_obj) },
    { MP_ROM_QSTR(MP_QSTR_getCursor),           MP_ROM_PTR(&display_lcd_getCursor_obj) },
    { MP_ROM_QSTR(MP_QSTR_setRotation),			MP_ROM_PTR(&display_lcd_setRotation_obj) },
    // { MP_ROM_QSTR(MP_QSTR_setTextSize),         MP_ROM_PTR(&display_lcd_setTextSize_obj) },
    { MP_ROM_QSTR(MP_QSTR_setColor),            MP_ROM_PTR(&display_lcd_setTextColor_obj) },
    { MP_ROM_QSTR(MP_QSTR_setTextColor),        MP_ROM_PTR(&display_lcd_setTextColor_obj) },
    { MP_ROM_QSTR(MP_QSTR_drawPixel),	            MP_ROM_PTR(&display_lcd_drawPixel_obj) },
    { MP_ROM_QSTR(MP_QSTR_fillScreen),	        MP_ROM_PTR(&display_lcd_fillScreen_obj) },
    { MP_ROM_QSTR(MP_QSTR_drawLine),	            MP_ROM_PTR(&display_lcd_drawLine_obj) },
    { MP_ROM_QSTR(MP_QSTR_drawTriangle),	        MP_ROM_PTR(&display_lcd_drawTriangle_obj) },
    { MP_ROM_QSTR(MP_QSTR_fillTriangle),	        MP_ROM_PTR(&display_lcd_fillTriangle_obj) },
    { MP_ROM_QSTR(MP_QSTR_drawCircle),	            MP_ROM_PTR(&display_lcd_drawCircle_obj) },
    { MP_ROM_QSTR(MP_QSTR_fillcircle),	            MP_ROM_PTR(&display_lcd_fillCircle_obj) },
    { MP_ROM_QSTR(MP_QSTR_drawRect),	            MP_ROM_PTR(&display_lcd_drawRect_obj) },
    { MP_ROM_QSTR(MP_QSTR_fillRect),	            MP_ROM_PTR(&display_lcd_fillRect_obj) },
    { MP_ROM_QSTR(MP_QSTR_drawRoundRect),	    MP_ROM_PTR(&display_lcd_drawRoundRect_obj) },
    { MP_ROM_QSTR(MP_QSTR_fillRoundRect),	    MP_ROM_PTR(&display_lcd_fillRoundRect_obj) },
    { MP_ROM_QSTR(MP_QSTR_print),	            MP_ROM_PTR(&display_lcd_drawString_obj) },
    { MP_ROM_QSTR(MP_QSTR_clear),				MP_ROM_PTR(&display_lcd_clear_obj) },

	{ MP_ROM_QSTR(MP_QSTR_BLACK),				MP_ROM_INT(ILI9341_BLACK) },
	{ MP_ROM_QSTR(MP_QSTR_NAVY),				MP_ROM_INT(ILI9341_NAVY) },
	{ MP_ROM_QSTR(MP_QSTR_DARKGREEN),			MP_ROM_INT(ILI9341_DARKGREEN) },
	{ MP_ROM_QSTR(MP_QSTR_DARKCYAN),			MP_ROM_INT(ILI9341_DARKCYAN) },
	{ MP_ROM_QSTR(MP_QSTR_MAROON),				MP_ROM_INT(ILI9341_MAROON) },
	{ MP_ROM_QSTR(MP_QSTR_PURPLE),				MP_ROM_INT(ILI9341_PURPLE) },
	{ MP_ROM_QSTR(MP_QSTR_OLIVE),				MP_ROM_INT(ILI9341_OLIVE) },
	{ MP_ROM_QSTR(MP_QSTR_LIGHTGREY),			MP_ROM_INT(ILI9341_LIGHTGREY) },
	{ MP_ROM_QSTR(MP_QSTR_DARKGREY),			MP_ROM_INT(ILI9341_DARKGREY) },
	{ MP_ROM_QSTR(MP_QSTR_BLUE),				MP_ROM_INT(ILI9341_BLUE) },
	{ MP_ROM_QSTR(MP_QSTR_GREEN),				MP_ROM_INT(ILI9341_GREEN) },
	{ MP_ROM_QSTR(MP_QSTR_CYAN),				MP_ROM_INT(ILI9341_CYAN) },
	{ MP_ROM_QSTR(MP_QSTR_RED),					MP_ROM_INT(ILI9341_RED) },
	{ MP_ROM_QSTR(MP_QSTR_MAGENTA),				MP_ROM_INT(ILI9341_MAGENTA) },
	{ MP_ROM_QSTR(MP_QSTR_YELLOW),				MP_ROM_INT(ILI9341_YELLOW) },
	{ MP_ROM_QSTR(MP_QSTR_WHITE),				MP_ROM_INT(ILI9341_WHITE) },
	{ MP_ROM_QSTR(MP_QSTR_ORANGE),				MP_ROM_INT(ILI9341_ORANGE) },
	{ MP_ROM_QSTR(MP_QSTR_GREENYELLOW),			MP_ROM_INT(ILI9341_GREENYELLOW) },
	{ MP_ROM_QSTR(MP_QSTR_PINK),				MP_ROM_INT(ILI9341_PINK) },

	{ MP_ROM_QSTR(MP_QSTR_COLOR_BITS16),		MP_ROM_INT(16) },
	{ MP_ROM_QSTR(MP_QSTR_COLOR_BITS24),		MP_ROM_INT(24) },

	{ MP_ROM_QSTR(MP_QSTR_HSPI),				MP_ROM_INT(HSPI_HOST) },
	{ MP_ROM_QSTR(MP_QSTR_VSPI),				MP_ROM_INT(VSPI_HOST) },

};
STATIC MP_DEFINE_CONST_DICT(display_lcd_locals_dict, display_lcd_locals_dict_table);

//======================================
const mp_obj_type_t display_lcd_type = {
    { &mp_type_type },
    .name = MP_QSTR_LCD,
    // .print = display_lcd_printinfo,
    .make_new = display_lcd_make_new,
    .locals_dict = (mp_obj_t)&display_lcd_locals_dict,
};

//===============================================================
STATIC const mp_rom_map_elem_t display_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_display) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_LCD), MP_ROM_PTR(&display_lcd_type) },
};

//===============================================================================
STATIC MP_DEFINE_CONST_DICT(display_module_globals, display_module_globals_table);

const mp_obj_module_t mp_module_display = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&display_module_globals,
};


