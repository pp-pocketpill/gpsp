/* gameplaySP
 *
 * Copyright (C) 2006 Exophase <exophase@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <SDL/SDL_ttf.h>
#include "common.h"
#define WANT_FONT_BITS
#include "font.h"
#include "menu.h"
#include "configfile_fk.h"

#ifdef CHIP_BUILD
#include "arm/neon_scaler.h"
#endif



#define AVERAGE(z, x) ((((z) & 0xF7DEF7DE) >> 1) + (((x) & 0xF7DEF7DE) >> 1))
#define AVERAGEHI(AB) ((((AB) & 0xF7DE0000) >> 1) + (((AB) & 0xF7DE) << 15))
#define AVERAGELO(CD) ((((CD) & 0xF7DE) >> 1) + (((CD) & 0xF7DE0000) >> 17))

// Support math
#define Half(A) (((A) >> 1) & 0x7BEF)
#define Quarter(A) (((A) >> 2) & 0x39E7)
// Error correction expressions to piece back the lower bits together
#define RestHalf(A) ((A) & 0x0821)
#define RestQuarter(A) ((A) & 0x1863)

// Error correction expressions for quarters of pixels
#define Corr1_3(A, B)     Quarter(RestQuarter(A) + (RestHalf(B) << 1) + RestQuarter(B))
#define Corr3_1(A, B)     Quarter((RestHalf(A) << 1) + RestQuarter(A) + RestQuarter(B))

// Error correction expressions for halves
#define Corr1_1(A, B)     ((A) & (B) & 0x0821)

// Quarters
#define Weight1_3(A, B)   (Quarter(A) + Half(B) + Quarter(B) + Corr1_3(A, B))
#define Weight3_1(A, B)   (Half(A) + Quarter(A) + Quarter(B) + Corr3_1(A, B))

// Halves
#define Weight1_1(A, B)   (Half(A) + Half(B) + Corr1_1(A, B))


#if 0
#ifdef PSP_BUILD

#include <pspctrl.h>

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>

#include <pspgu.h>
#include <psppower.h>
#include <psprtc.h>

static float *screen_vertex = (float *)0x441FC100;
static u32 *ge_cmd = (u32 *)0x441FC000;
static u16 *psp_gu_vram_base = (u16 *)(0x44000000);
static u32 *ge_cmd_ptr = (u32 *)0x441FC000;
static u32 gecbid;
static u32 video_direct = 0;

static u32 __attribute__((aligned(16))) display_list[32];

#define GBA_SCREEN_WIDTH 240
#define GBA_SCREEN_HEIGHT 160

#define RES_HW_SCREEN_HORIZONTAL  240
#define RES_HW_SCREEN_VERTICAL    240

#define PSP_SCREEN_WIDTH 480
#define PSP_SCREEN_HEIGHT 272
#define PSP_LINE_SIZE 512

#define PSP_ALL_BUTTON_MASK 0xFFFF

#define GE_CMD_FBP    0x9C
#define GE_CMD_FBW    0x9D
#define GE_CMD_TBP0   0xA0
#define GE_CMD_TBW0   0xA8
#define GE_CMD_TSIZE0 0xB8
#define GE_CMD_TFLUSH 0xCB
#define GE_CMD_CLEAR  0xD3
#define GE_CMD_VTYPE  0x12
#define GE_CMD_BASE   0x10
#define GE_CMD_VADDR  0x01
#define GE_CMD_IADDR  0x02
#define GE_CMD_PRIM   0x04
#define GE_CMD_FINISH 0x0F
#define GE_CMD_SIGNAL 0x0C
#define GE_CMD_NOP    0x00

#define GE_CMD(cmd, operand)                                                \
  *ge_cmd_ptr = (((GE_CMD_##cmd) << 24) | (operand));                       \
  ge_cmd_ptr++                                                              \

static u16 *screen_texture = (u16 *)(0x4000000 + (512 * 272 * 2));
static u16 *current_screen_texture = (u16 *)(0x4000000 + (512 * 272 * 2));
static u16 *screen_pixels = (u16 *)(0x4000000 + (512 * 272 * 2));
static u32 screen_pitch = 240;

static void Ge_Finish_Callback(int id, void *arg)
{
}

#define get_screen_pixels()                                                   \
  screen_pixels                                                               \

#define get_screen_pitch()                                                    \
  screen_pitch                                                                \

#elif defined(POLLUX_BUILD)

static u16 rot_buffer[240*4];
static u32 rot_lines_total = 4;
static u32 rot_line_count = 0;
#ifdef WIZ_BUILD
static char rot_msg_buff[64];
#endif

static u32 screen_offset = 0;
static u16 *screen_pixels = NULL;
const u32 screen_pitch = 320;

#define get_screen_pixels()                                                   \
  screen_pixels                                                               \

#define get_screen_pitch()                                                    \
  screen_pitch                                                                \

#elif defined(PND_BUILD) || defined(RPI_BUILD)

static u16 *screen_pixels = NULL;

#define get_screen_pixels()                                                   \
  screen_pixels                                                               \

#define get_screen_pitch()                                                    \
  resolution_width                                                            \

#else
#endif

#ifdef GP2X_BUILD
#include "SDL_gp2x.h"
SDL_Surface *hw_screen;
#endif
#endif
#ifdef CHIP_BUILD
SDL_Surface *hw_screen = NULL;

#define GBA_SCREEN_WIDTH 240
#define GBA_SCREEN_HEIGHT 160

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define ABS(x) (((x) < 0) ? (-x) : (x))
#endif
SDL_Surface *screen = NULL;
SDL_Surface *virtual_hw_screen = NULL;
SDL_Rect screenRect, gbaRect;

const u32 video_scale = 1;

#define get_screen_pixels()                                                   \
  ((u16 *)screen->pixels)                                                     \

#define get_screen_pitch()                                                    \
  (screen->pitch >> 1)                                                         \

int need_screen_cleared;

char hud_msg[HUD_MSG_MAX_SIZE];
int hud_msg_secs_on_screen = 0;
static int hud_ms_at_launch;

static void render_scanline_conditional_tile(u32 start, u32 end, u16 *scanline,
 u32 enable_flags, u32 dispcnt, u32 bldcnt, const tile_layer_render_struct
 *layer_renderers);
static void render_scanline_conditional_bitmap(u32 start, u32 end, u16 *scanline,
 u32 enable_flags, u32 dispcnt, u32 bldcnt, const bitmap_layer_render_struct
 *layer_renderers);

#define no_op                                                                 \

// This old version is not necessary if the palette is either being converted
// transparently or the ABGR 1555 format is being used natively. The direct
// version (without conversion) is much faster.

#define tile_lookup_palette_full(palette, source)                             \
  current_pixel = palette[source];                                            \
  convert_palette(current_pixel)                                              \

#define tile_lookup_palette(palette, source)                                  \
  current_pixel = palette[source];                                            \


#ifdef RENDER_COLOR16_NORMAL

#define tile_expand_base_normal(index)                                        \
  tile_expand_base_color16(index)                                             \

#else

#define tile_expand_base_normal(index)                                        \
  tile_lookup_palette(palette, current_pixel);                                \
  dest_ptr[index] = current_pixel                                             \

#endif

#define tile_expand_transparent_normal(index)                                 \
  tile_expand_base_normal(index)                                              \

#define tile_expand_copy(index)                                               \
  dest_ptr[index] = copy_ptr[index]                                           \


#define advance_dest_ptr_base(delta)                                          \
  dest_ptr += delta                                                           \

#define advance_dest_ptr_transparent(delta)                                   \
  advance_dest_ptr_base(delta)                                                \

#define advance_dest_ptr_copy(delta)                                          \
  advance_dest_ptr_base(delta);                                               \
  copy_ptr += delta                                                           \


#define color_combine_mask_a(layer)                                           \
  ((io_registers[REG_BLDCNT] >> layer) & 0x01)                                \

// For color blending operations, will create a mask that has in bit
// 10 if the layer is target B, and bit 9 if the layer is target A.

#define color_combine_mask(layer)                                             \
  (color_combine_mask_a(layer) |                                              \
   ((io_registers[REG_BLDCNT] >> (layer + 7)) & 0x02)) << 9                   \

// For alpha blending renderers, draw the palette index (9bpp) and
// layer bits rather than the raw RGB. For the base this should write to
// the 32bit location directly.

#define tile_expand_base_alpha(index)                                         \
  dest_ptr[index] = current_pixel | pixel_combine                             \

#define tile_expand_base_bg(index)                                            \
  dest_ptr[index] = bg_combine                                                \


// For layered (transparent) writes this should shift the "stack" and write
// to the bottom. This will preserve the topmost pixel and the most recent
// one.

#define tile_expand_transparent_alpha(index)                                  \
  dest_ptr[index] = (dest_ptr[index] << 16) | current_pixel | pixel_combine   \


// OBJ should only shift if the top isn't already OBJ
#define tile_expand_transparent_alpha_obj(index)                              \
  dest = dest_ptr[index];                                                     \
  if(dest & 0x00000100)                                                       \
  {                                                                           \
    dest_ptr[index] = (dest & 0xFFFF0000) | current_pixel | pixel_combine;    \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    dest_ptr[index] = (dest << 16) | current_pixel | pixel_combine;           \
  }                                                                           \


// For color effects that don't need to preserve the previous layer.
// The color32 version should be used with 32bit wide dest_ptr so as to be
// compatible with alpha combine on top of it.

#define tile_expand_base_color16(index)                                       \
  dest_ptr[index] = current_pixel | pixel_combine                             \

#define tile_expand_transparent_color16(index)                                \
  tile_expand_base_color16(index)                                             \

#define tile_expand_base_color32(index)                                       \
  tile_expand_base_color16(index)                                             \

#define tile_expand_transparent_color32(index)                                \
  tile_expand_base_color16(index)                                             \


// Operations for isolation 8bpp pixels within 32bpp pixel blocks.

#define tile_8bpp_pixel_op_mask(op_param)                                     \
  current_pixel = current_pixels & 0xFF                                       \

#define tile_8bpp_pixel_op_shift_mask(shift)                                  \
  current_pixel = (current_pixels >> shift) & 0xFF                            \

#define tile_8bpp_pixel_op_shift(shift)                                       \
  current_pixel = current_pixels >> shift                                     \

#define tile_8bpp_pixel_op_none(shift)                                        \

// Base should always draw raw in 8bpp mode; color 0 will be drawn where
// color 0 is.

#define tile_8bpp_draw_base_normal(index)                                     \
  tile_expand_base_normal(index)                                              \

#define tile_8bpp_draw_base_alpha(index)                                      \
  if(current_pixel)                                                           \
  {                                                                           \
    tile_expand_base_alpha(index);                                            \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    tile_expand_base_bg(index);                                               \
  }                                                                           \


#define tile_8bpp_draw_base_color16(index)                                    \
  tile_8bpp_draw_base_alpha(index)                                            \

#define tile_8bpp_draw_base_color32(index)                                    \
  tile_8bpp_draw_base_alpha(index)                                            \


#define tile_8bpp_draw_base(index, op, op_param, alpha_op)                    \
  tile_8bpp_pixel_op_##op(op_param);                                          \
  tile_8bpp_draw_base_##alpha_op(index)                                       \

// Transparent (layered) writes should only replace what is there if the
// pixel is not transparent (zero)

#define tile_8bpp_draw_transparent(index, op, op_param, alpha_op)             \
  tile_8bpp_pixel_op_##op(op_param);                                          \
  if(current_pixel)                                                           \
  {                                                                           \
    tile_expand_transparent_##alpha_op(index);                                \
  }                                                                           \

#define tile_8bpp_draw_copy(index, op, op_param, alpha_op)                    \
  tile_8bpp_pixel_op_##op(op_param);                                          \
  if(current_pixel)                                                           \
  {                                                                           \
    tile_expand_copy(index);                                                  \
  }                                                                           \

// Get the current tile from the map in 8bpp mode

#define get_tile_8bpp()                                                       \
  current_tile = *map_ptr;                                                    \
  tile_ptr = tile_base + ((current_tile & 0x3FF) * 64)                        \


// Draw half of a tile in 8bpp mode, for base renderer

#define tile_8bpp_draw_four_noflip(index, combine_op, alpha_op)               \
  tile_8bpp_draw_##combine_op(index + 0, mask, 0, alpha_op);                  \
  tile_8bpp_draw_##combine_op(index + 1, shift_mask, 8, alpha_op);            \
  tile_8bpp_draw_##combine_op(index + 2, shift_mask, 16, alpha_op);           \
  tile_8bpp_draw_##combine_op(index + 3, shift, 24, alpha_op)                 \


// Like the above, but draws the half-tile horizontally flipped

#define tile_8bpp_draw_four_flip(index, combine_op, alpha_op)                 \
  tile_8bpp_draw_##combine_op(index + 3, mask, 0, alpha_op);                  \
  tile_8bpp_draw_##combine_op(index + 2, shift_mask, 8, alpha_op);            \
  tile_8bpp_draw_##combine_op(index + 1, shift_mask, 16, alpha_op);           \
  tile_8bpp_draw_##combine_op(index + 0, shift, 24, alpha_op)                 \

#define tile_8bpp_draw_four_base(index, alpha_op, flip_op)                    \
  tile_8bpp_draw_four_##flip_op(index, base, alpha_op)                        \


// Draw half of a tile in 8bpp mode, for transparent renderer; as an
// optimization the entire thing is checked against zero (in transparent
// capable renders it is more likely for the pixels to be transparent than
// opaque)

#define tile_8bpp_draw_four_transparent(index, alpha_op, flip_op)             \
  if(current_pixels != 0)                                                     \
  {                                                                           \
    tile_8bpp_draw_four_##flip_op(index, transparent, alpha_op);              \
  }                                                                           \

#define tile_8bpp_draw_four_copy(index, alpha_op, flip_op)                    \
  if(current_pixels != 0)                                                     \
  {                                                                           \
    tile_8bpp_draw_four_##flip_op(index, copy, alpha_op);                     \
  }                                                                           \

// Helper macro for drawing 8bpp tiles clipped against the edge of the screen

#define partial_tile_8bpp(combine_op, alpha_op)                               \
  for(i = 0; i < partial_tile_run; i++)                                       \
  {                                                                           \
    tile_8bpp_draw_##combine_op(0, mask, 0, alpha_op);                        \
    current_pixels >>= 8;                                                     \
    advance_dest_ptr_##combine_op(1);                                         \
  }                                                                           \


// Draws 8bpp tiles clipped against the left side of the screen,
// partial_tile_offset indicates how much clipped in it is, partial_tile_run
// indicates how much it should draw.

#define partial_tile_right_noflip_8bpp(combine_op, alpha_op)                  \
  if(partial_tile_offset >= 4)                                                \
  {                                                                           \
    current_pixels = *((u32 *)(tile_ptr + 4)) >>                              \
     ((partial_tile_offset - 4) * 8);                                         \
    partial_tile_8bpp(combine_op, alpha_op);                                  \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    partial_tile_run -= 4;                                                    \
    current_pixels = *((u32 *)tile_ptr) >> (partial_tile_offset * 8);         \
    partial_tile_8bpp(combine_op, alpha_op);                                  \
    current_pixels = *((u32 *)(tile_ptr + 4));                                \
    tile_8bpp_draw_four_##combine_op(0, alpha_op, noflip);                    \
    advance_dest_ptr_##combine_op(4);                                         \
  }                                                                           \


// Draws 8bpp tiles clipped against both the left and right side of the
// screen, IE, runs of less than 8 - partial_tile_offset.

#define partial_tile_mid_noflip_8bpp(combine_op, alpha_op)                    \
  if(partial_tile_offset >= 4)                                                \
  {                                                                           \
    current_pixels = *((u32 *)(tile_ptr + 4)) >>                              \
     ((partial_tile_offset - 4) * 8);                                         \
    partial_tile_8bpp(combine_op, alpha_op);                                  \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    current_pixels = *((u32 *)tile_ptr) >> (partial_tile_offset * 8);         \
    if((partial_tile_offset + partial_tile_run) > 4)                          \
    {                                                                         \
      u32 old_run = partial_tile_run;                                         \
      partial_tile_run = 4 - partial_tile_offset;                             \
      partial_tile_8bpp(combine_op, alpha_op);                                \
      partial_tile_run = old_run - partial_tile_run;                          \
      current_pixels = *((u32 *)(tile_ptr + 4));                              \
      partial_tile_8bpp(combine_op, alpha_op);                                \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      partial_tile_8bpp(combine_op, alpha_op);                                \
    }                                                                         \
  }                                                                           \


// Draws 8bpp tiles clipped against the right side of the screen,
// partial_tile_run indicates how much there is to draw.

#define partial_tile_left_noflip_8bpp(combine_op, alpha_op)                   \
  if(partial_tile_run >= 4)                                                   \
  {                                                                           \
    current_pixels = *((u32 *)tile_ptr);                                      \
    tile_8bpp_draw_four_##combine_op(0, alpha_op, noflip);                    \
    advance_dest_ptr_##combine_op(4);                                         \
    tile_ptr += 4;                                                            \
    partial_tile_run -= 4;                                                    \
  }                                                                           \
                                                                              \
  current_pixels = *((u32 *)(tile_ptr));                                      \
  partial_tile_8bpp(combine_op, alpha_op)                                     \


// Draws a non-clipped (complete) 8bpp tile.

#define tile_noflip_8bpp(combine_op, alpha_op)                                \
  current_pixels = *((u32 *)tile_ptr);                                        \
  tile_8bpp_draw_four_##combine_op(0, alpha_op, noflip);                      \
  current_pixels = *((u32 *)(tile_ptr + 4));                                  \
  tile_8bpp_draw_four_##combine_op(4, alpha_op, noflip)                       \


// Like the above versions but draws flipped tiles.

#define partial_tile_flip_8bpp(combine_op, alpha_op)                          \
  for(i = 0; i < partial_tile_run; i++)                                       \
  {                                                                           \
    tile_8bpp_draw_##combine_op(0, shift, 24, alpha_op);                      \
    current_pixels <<= 8;                                                     \
    advance_dest_ptr_##combine_op(1);                                         \
  }                                                                           \

#define partial_tile_right_flip_8bpp(combine_op, alpha_op)                    \
  if(partial_tile_offset >= 4)                                                \
  {                                                                           \
    current_pixels = *((u32 *)tile_ptr) << ((partial_tile_offset - 4) * 8);   \
    partial_tile_flip_8bpp(combine_op, alpha_op);                             \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    partial_tile_run -= 4;                                                    \
    current_pixels = *((u32 *)(tile_ptr + 4)) <<                              \
     ((partial_tile_offset - 4) * 8);                                         \
    partial_tile_flip_8bpp(combine_op, alpha_op);                             \
    current_pixels = *((u32 *)tile_ptr);                                      \
    tile_8bpp_draw_four_##combine_op(0, alpha_op, flip);                      \
    advance_dest_ptr_##combine_op(4);                                         \
  }                                                                           \

#define partial_tile_mid_flip_8bpp(combine_op, alpha_op)                      \
  if(partial_tile_offset >= 4)                                                \
  {                                                                           \
    current_pixels = *((u32 *)tile_ptr) << ((partial_tile_offset - 4) * 8);   \
    partial_tile_flip_8bpp(combine_op, alpha_op);                             \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    current_pixels = *((u32 *)(tile_ptr + 4)) <<                              \
     ((partial_tile_offset - 4) * 8);                                         \
                                                                              \
    if((partial_tile_offset + partial_tile_run) > 4)                          \
    {                                                                         \
      u32 old_run = partial_tile_run;                                         \
      partial_tile_run = 4 - partial_tile_offset;                             \
      partial_tile_flip_8bpp(combine_op, alpha_op);                           \
      partial_tile_run = old_run - partial_tile_run;                          \
      current_pixels = *((u32 *)(tile_ptr));                                  \
      partial_tile_flip_8bpp(combine_op, alpha_op);                           \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      partial_tile_flip_8bpp(combine_op, alpha_op);                           \
    }                                                                         \
  }                                                                           \

#define partial_tile_left_flip_8bpp(combine_op, alpha_op)                     \
  if(partial_tile_run >= 4)                                                   \
  {                                                                           \
    current_pixels = *((u32 *)(tile_ptr + 4));                                \
    tile_8bpp_draw_four_##combine_op(0, alpha_op, flip);                      \
    advance_dest_ptr_##combine_op(4);                                         \
    tile_ptr -= 4;                                                            \
    partial_tile_run -= 4;                                                    \
  }                                                                           \
                                                                              \
  current_pixels = *((u32 *)(tile_ptr + 4));                                  \
  partial_tile_flip_8bpp(combine_op, alpha_op)                                \

#define tile_flip_8bpp(combine_op, alpha_op)                                  \
  current_pixels = *((u32 *)(tile_ptr + 4));                                  \
  tile_8bpp_draw_four_##combine_op(0, alpha_op, flip);                        \
  current_pixels = *((u32 *)tile_ptr);                                        \
  tile_8bpp_draw_four_##combine_op(4, alpha_op, flip)                         \


// Operations for isolating 4bpp tiles in a 32bit block

#define tile_4bpp_pixel_op_mask(op_param)                                     \
  current_pixel = current_pixels & 0x0F                                       \

#define tile_4bpp_pixel_op_shift_mask(shift)                                  \
  current_pixel = (current_pixels >> shift) & 0x0F                            \

#define tile_4bpp_pixel_op_shift(shift)                                       \
  current_pixel = current_pixels >> shift                                     \

#define tile_4bpp_pixel_op_none(op_param)                                     \

// Draws a single 4bpp pixel as base, normal renderer; checks to see if the
// pixel is zero because if so the current palette should not be applied.
// These ifs can be replaced with a lookup table, may or may not be superior
// this way, should be benchmarked. The lookup table would be from 0-255
// identity map except for multiples of 16, which would map to 0.

#define tile_4bpp_draw_base_normal(index)                                     \
  if(current_pixel)                                                           \
  {                                                                           \
    current_pixel |= current_palette;                                         \
    tile_expand_base_normal(index);                                           \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    tile_expand_base_normal(index);                                           \
  }                                                                           \


#define tile_4bpp_draw_base_alpha(index)                                      \
  if(current_pixel)                                                           \
  {                                                                           \
    current_pixel |= current_palette;                                         \
    tile_expand_base_alpha(index);                                            \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    tile_expand_base_bg(index);                                               \
  }                                                                           \

#define tile_4bpp_draw_base_color16(index)                                    \
  tile_4bpp_draw_base_alpha(index)                                            \

#define tile_4bpp_draw_base_color32(index)                                    \
  tile_4bpp_draw_base_alpha(index)                                            \


#define tile_4bpp_draw_base(index, op, op_param, alpha_op)                    \
  tile_4bpp_pixel_op_##op(op_param);                                          \
  tile_4bpp_draw_base_##alpha_op(index)                                       \


// Draws a single 4bpp pixel as layered, if not transparent.

#define tile_4bpp_draw_transparent(index, op, op_param, alpha_op)             \
  tile_4bpp_pixel_op_##op(op_param);                                          \
  if(current_pixel)                                                           \
  {                                                                           \
    current_pixel |= current_palette;                                         \
    tile_expand_transparent_##alpha_op(index);                                \
  }                                                                           \

#define tile_4bpp_draw_copy(index, op, op_param, alpha_op)                    \
  tile_4bpp_pixel_op_##op(op_param);                                          \
  if(current_pixel)                                                           \
  {                                                                           \
    current_pixel |= current_palette;                                         \
    tile_expand_copy(index);                                                  \
  }                                                                           \


// Draws eight background pixels in transparent mode, for alpha or normal
// renderers.

#define tile_4bpp_draw_eight_base_zero(value)                                 \
  dest_ptr[0] = value;                                                        \
  dest_ptr[1] = value;                                                        \
  dest_ptr[2] = value;                                                        \
  dest_ptr[3] = value;                                                        \
  dest_ptr[4] = value;                                                        \
  dest_ptr[5] = value;                                                        \
  dest_ptr[6] = value;                                                        \
  dest_ptr[7] = value                                                         \


// Draws eight background pixels for the alpha renderer, basically color zero
// with the background flag high.

#define tile_4bpp_draw_eight_base_zero_alpha()                                \
  tile_4bpp_draw_eight_base_zero(bg_combine)                                  \

#define tile_4bpp_draw_eight_base_zero_color16()                              \
  tile_4bpp_draw_eight_base_zero_alpha()                                      \

#define tile_4bpp_draw_eight_base_zero_color32()                              \
  tile_4bpp_draw_eight_base_zero_alpha()                                      \


// Draws eight background pixels for the normal renderer, just a bunch of
// zeros.

#ifdef RENDER_COLOR16_NORMAL

#define tile_4bpp_draw_eight_base_zero_normal()                               \
  current_pixel = 0;                                                          \
  tile_4bpp_draw_eight_base_zero(current_pixel)                               \

#else

#define tile_4bpp_draw_eight_base_zero_normal()                               \
  current_pixel = palette[0];                                                 \
  tile_4bpp_draw_eight_base_zero(current_pixel)                               \

#endif


// Draws eight 4bpp pixels.

#define tile_4bpp_draw_eight_noflip(combine_op, alpha_op)                     \
  tile_4bpp_draw_##combine_op(0, mask, 0, alpha_op);                          \
  tile_4bpp_draw_##combine_op(1, shift_mask, 4, alpha_op);                    \
  tile_4bpp_draw_##combine_op(2, shift_mask, 8, alpha_op);                    \
  tile_4bpp_draw_##combine_op(3, shift_mask, 12, alpha_op);                   \
  tile_4bpp_draw_##combine_op(4, shift_mask, 16, alpha_op);                   \
  tile_4bpp_draw_##combine_op(5, shift_mask, 20, alpha_op);                   \
  tile_4bpp_draw_##combine_op(6, shift_mask, 24, alpha_op);                   \
  tile_4bpp_draw_##combine_op(7, shift, 28, alpha_op)                         \


// Draws eight 4bpp pixels in reverse order (for hflip).

#define tile_4bpp_draw_eight_flip(combine_op, alpha_op)                       \
  tile_4bpp_draw_##combine_op(7, mask, 0, alpha_op);                          \
  tile_4bpp_draw_##combine_op(6, shift_mask, 4, alpha_op);                    \
  tile_4bpp_draw_##combine_op(5, shift_mask, 8, alpha_op);                    \
  tile_4bpp_draw_##combine_op(4, shift_mask, 12, alpha_op);                   \
  tile_4bpp_draw_##combine_op(3, shift_mask, 16, alpha_op);                   \
  tile_4bpp_draw_##combine_op(2, shift_mask, 20, alpha_op);                   \
  tile_4bpp_draw_##combine_op(1, shift_mask, 24, alpha_op);                   \
  tile_4bpp_draw_##combine_op(0, shift, 28, alpha_op)                         \


// Draws eight 4bpp pixels in base mode, checks if all are zero, if so draws
// the appropriate background pixels.

#define tile_4bpp_draw_eight_base(alpha_op, flip_op)                          \
  if(current_pixels != 0)                                                     \
  {                                                                           \
    tile_4bpp_draw_eight_##flip_op(base, alpha_op);                           \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    tile_4bpp_draw_eight_base_zero_##alpha_op();                              \
  }                                                                           \


// Draws eight 4bpp pixels in transparent (layered) mode, checks if all are
// zero and if so draws nothing.

#define tile_4bpp_draw_eight_transparent(alpha_op, flip_op)                   \
  if(current_pixels != 0)                                                     \
  {                                                                           \
    tile_4bpp_draw_eight_##flip_op(transparent, alpha_op);                    \
  }                                                                           \


#define tile_4bpp_draw_eight_copy(alpha_op, flip_op)                          \
  if(current_pixels != 0)                                                     \
  {                                                                           \
    tile_4bpp_draw_eight_##flip_op(copy, alpha_op);                           \
  }                                                                           \

// Gets the current tile in 4bpp mode, also getting the current palette and
// the pixel block.

#define get_tile_4bpp()                                                       \
  current_tile = *map_ptr;                                                    \
  current_palette = (current_tile >> 12) << 4;                                \
  tile_ptr = tile_base + ((current_tile & 0x3FF) * 32);                       \


// Helper macro for drawing clipped 4bpp tiles.

#define partial_tile_4bpp(combine_op, alpha_op)                               \
  for(i = 0; i < partial_tile_run; i++)                                       \
  {                                                                           \
    tile_4bpp_draw_##combine_op(0, mask, 0, alpha_op);                        \
    current_pixels >>= 4;                                                     \
    advance_dest_ptr_##combine_op(1);                                         \
  }                                                                           \


// Draws a 4bpp tile clipped against the left edge of the screen.
// partial_tile_offset is how far in it's clipped, partial_tile_run is
// how many to draw.

#define partial_tile_right_noflip_4bpp(combine_op, alpha_op)                  \
  current_pixels = *((u32 *)tile_ptr) >> (partial_tile_offset * 4);           \
  partial_tile_4bpp(combine_op, alpha_op)                                     \


// Draws a 4bpp tile clipped against both edges of the screen, same as right.

#define partial_tile_mid_noflip_4bpp(combine_op, alpha_op)                    \
  partial_tile_right_noflip_4bpp(combine_op, alpha_op)                        \


// Draws a 4bpp tile clipped against the right edge of the screen.
// partial_tile_offset is how many to draw.

#define partial_tile_left_noflip_4bpp(combine_op, alpha_op)                   \
  current_pixels = *((u32 *)tile_ptr);                                        \
  partial_tile_4bpp(combine_op, alpha_op)                                     \


// Draws a complete 4bpp tile row (not clipped)
#define tile_noflip_4bpp(combine_op, alpha_op)                                \
  current_pixels = *((u32 *)tile_ptr);                                        \
  tile_4bpp_draw_eight_##combine_op(alpha_op, noflip)                         \


// Like the above, but draws flipped tiles.

#define partial_tile_flip_4bpp(combine_op, alpha_op)                          \
  for(i = 0; i < partial_tile_run; i++)                                       \
  {                                                                           \
    tile_4bpp_draw_##combine_op(0, shift, 28, alpha_op);                      \
    current_pixels <<= 4;                                                     \
    advance_dest_ptr_##combine_op(1);                                         \
  }                                                                           \

#define partial_tile_right_flip_4bpp(combine_op, alpha_op)                    \
  current_pixels = *((u32 *)tile_ptr) << (partial_tile_offset * 4);           \
  partial_tile_flip_4bpp(combine_op, alpha_op)                                \

#define partial_tile_mid_flip_4bpp(combine_op, alpha_op)                      \
  partial_tile_right_flip_4bpp(combine_op, alpha_op)                          \

#define partial_tile_left_flip_4bpp(combine_op, alpha_op)                     \
  current_pixels = *((u32 *)tile_ptr);                                        \
  partial_tile_flip_4bpp(combine_op, alpha_op)                                \

#define tile_flip_4bpp(combine_op, alpha_op)                                  \
  current_pixels = *((u32 *)tile_ptr);                                        \
  tile_4bpp_draw_eight_##combine_op(alpha_op, flip)                           \


// Draws a single (partial or complete) tile from the tilemap, flipping
// as necessary.

#define single_tile_map(tile_type, combine_op, color_depth, alpha_op)         \
  get_tile_##color_depth();                                                   \
  if(current_tile & 0x800)                                                    \
    tile_ptr += vertical_pixel_flip;                                          \
                                                                              \
  if(current_tile & 0x400)                                                    \
  {                                                                           \
    tile_type##_flip_##color_depth(combine_op, alpha_op);                     \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    tile_type##_noflip_##color_depth(combine_op, alpha_op);                   \
  }                                                                           \


// Draws multiple sequential tiles from the tilemap, hflips and vflips as
// necessary.

#define multiple_tile_map(combine_op, color_depth, alpha_op)                  \
  for(i = 0; i < tile_run; i++)                                               \
  {                                                                           \
    single_tile_map(tile, combine_op, color_depth, alpha_op);                 \
    advance_dest_ptr_##combine_op(8);                                         \
    map_ptr++;                                                                \
  }                                                                           \

// Draws a partial tile from a tilemap clipped against the left edge of the
// screen.

#define partial_tile_right_map(combine_op, color_depth, alpha_op)             \
  single_tile_map(partial_tile_right, combine_op, color_depth, alpha_op);     \
  map_ptr++                                                                   \

// Draws a partial tile from a tilemap clipped against both edges of the
// screen.

#define partial_tile_mid_map(combine_op, color_depth, alpha_op)               \
  single_tile_map(partial_tile_mid, combine_op, color_depth, alpha_op)        \

// Draws a partial tile from a tilemap clipped against the right edge of the
// screen.

#define partial_tile_left_map(combine_op, color_depth, alpha_op)              \
  single_tile_map(partial_tile_left, combine_op, color_depth, alpha_op)       \


// Advances a non-flipped 4bpp obj to the next tile.

#define obj_advance_noflip_4bpp()                                             \
  tile_ptr += 32                                                              \


// Advances a non-flipped 8bpp obj to the next tile.

#define obj_advance_noflip_8bpp()                                             \
  tile_ptr += 64                                                              \


// Advances a flipped 4bpp obj to the next tile.

#define obj_advance_flip_4bpp()                                               \
  tile_ptr -= 32                                                              \


// Advances a flipped 8bpp obj to the next tile.

#define obj_advance_flip_8bpp()                                               \
  tile_ptr -= 64                                                              \



// Draws multiple sequential tiles from an obj, flip_op determines if it should
// be flipped or not (set to flip or noflip)

#define multiple_tile_obj(combine_op, color_depth, alpha_op, flip_op)         \
  for(i = 0; i < tile_run; i++)                                               \
  {                                                                           \
    tile_##flip_op##_##color_depth(combine_op, alpha_op);                     \
    obj_advance_##flip_op##_##color_depth();                                  \
    advance_dest_ptr_##combine_op(8);                                         \
  }                                                                           \


// Draws an obj's tile clipped against the left side of the screen

#define partial_tile_right_obj(combine_op, color_depth, alpha_op, flip_op)    \
  partial_tile_right_##flip_op##_##color_depth(combine_op, alpha_op);         \
  obj_advance_##flip_op##_##color_depth()                                     \

// Draws an obj's tile clipped against both sides of the screen

#define partial_tile_mid_obj(combine_op, color_depth, alpha_op, flip_op)      \
  partial_tile_mid_##flip_op##_##color_depth(combine_op, alpha_op)            \

// Draws an obj's tile clipped against the right side of the screen

#define partial_tile_left_obj(combine_op, color_depth, alpha_op, flip_op)     \
  partial_tile_left_##flip_op##_##color_depth(combine_op, alpha_op)           \


// Extra variables specific for 8bpp/4bpp tile renderers.

#define tile_extra_variables_8bpp()                                           \

#define tile_extra_variables_4bpp()                                           \
  u32 current_palette                                                         \


// Byte lengths of complete tiles and tile rows in 4bpp and 8bpp.

#define tile_width_4bpp 4
#define tile_size_4bpp 32
#define tile_width_8bpp 8
#define tile_size_8bpp 64


// Render a single scanline of text tiles

#define tile_render(color_depth, combine_op, alpha_op)                        \
{                                                                             \
  u32 vertical_pixel_offset = (vertical_offset % 8) *                         \
   tile_width_##color_depth;                                                  \
  u32 vertical_pixel_flip =                                                   \
   ((tile_size_##color_depth - tile_width_##color_depth) -                    \
   vertical_pixel_offset) - vertical_pixel_offset;                            \
  tile_extra_variables_##color_depth();                                       \
  u8 *tile_base = vram + (((bg_control >> 2) & 0x03) * (1024 * 16)) +         \
   vertical_pixel_offset;                                                     \
  u32 pixel_run = 256 - (horizontal_offset % 256);                            \
  u32 current_tile;                                                           \
                                                                              \
  map_base += ((vertical_offset % 256) / 8) * 32;                             \
  partial_tile_offset = (horizontal_offset % 8);                              \
                                                                              \
  if(pixel_run >= end)                                                        \
  {                                                                           \
    if(partial_tile_offset)                                                   \
    {                                                                         \
      partial_tile_run = 8 - partial_tile_offset;                             \
      if(end < partial_tile_run)                                              \
      {                                                                       \
        partial_tile_run = end;                                               \
        partial_tile_mid_map(combine_op, color_depth, alpha_op);              \
        return;                                                               \
      }                                                                       \
      else                                                                    \
      {                                                                       \
        end -= partial_tile_run;                                              \
        partial_tile_right_map(combine_op, color_depth, alpha_op);            \
      }                                                                       \
    }                                                                         \
                                                                              \
    tile_run = end / 8;                                                       \
    multiple_tile_map(combine_op, color_depth, alpha_op);                     \
                                                                              \
    partial_tile_run = end % 8;                                               \
                                                                              \
    if(partial_tile_run)                                                      \
    {                                                                         \
      partial_tile_left_map(combine_op, color_depth, alpha_op);               \
    }                                                                         \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    if(partial_tile_offset)                                                   \
    {                                                                         \
      partial_tile_run = 8 - partial_tile_offset;                             \
      partial_tile_right_map(combine_op, color_depth, alpha_op);              \
    }                                                                         \
                                                                              \
    tile_run = (pixel_run - partial_tile_run) / 8;                            \
    multiple_tile_map(combine_op, color_depth, alpha_op);                     \
    map_ptr = second_ptr;                                                     \
    end -= pixel_run;                                                         \
    tile_run = end / 8;                                                       \
    multiple_tile_map(combine_op, color_depth, alpha_op);                     \
                                                                              \
    partial_tile_run = end % 8;                                               \
    if(partial_tile_run)                                                      \
    {                                                                         \
      partial_tile_left_map(combine_op, color_depth, alpha_op);               \
    }                                                                         \
  }                                                                           \
}                                                                             \

#define render_scanline_dest_normal         u16
#define render_scanline_dest_alpha          u32
#define render_scanline_dest_alpha_obj      u32
#define render_scanline_dest_color16        u16
#define render_scanline_dest_color32        u32
#define render_scanline_dest_partial_alpha  u32
#define render_scanline_dest_copy_tile      u16
#define render_scanline_dest_copy_bitmap    u16


// If rendering a scanline that is not a target A then there's no point in
// keeping what's underneath it because it can't blend with it.

#define render_scanline_skip_alpha(bg_type, combine_op)                       \
  if((pixel_combine & 0x00000200) == 0)                                       \
  {                                                                           \
    render_scanline_##bg_type##_##combine_op##_color32(layer,                 \
     start, end, scanline);                                                   \
    return;                                                                   \
  }                                                                           \


#ifdef RENDER_COLOR16_NORMAL

#define render_scanline_extra_variables_base_normal(bg_type)                  \
  const u32 pixel_combine = 0                                                 \

#else

#define render_scanline_extra_variables_base_normal(bg_type)                  \
  u16 *palette = palette_ram_converted                                        \

#endif


#define render_scanline_extra_variables_base_alpha(bg_type)                   \
  u32 bg_combine = color_combine_mask(5);                                     \
  u32 pixel_combine = color_combine_mask(layer) | (bg_combine << 16);         \
  render_scanline_skip_alpha(bg_type, base)                                   \

#define render_scanline_extra_variables_base_color()                          \
  u32 bg_combine = color_combine_mask(5);                                     \
  u32 pixel_combine = color_combine_mask(layer)                               \

#define render_scanline_extra_variables_base_color16(bg_type)                 \
  render_scanline_extra_variables_base_color()                                \

#define render_scanline_extra_variables_base_color32(bg_type)                 \
  render_scanline_extra_variables_base_color()                                \


#define render_scanline_extra_variables_transparent_normal(bg_type)           \
  render_scanline_extra_variables_base_normal(bg_type)                        \

#define render_scanline_extra_variables_transparent_alpha(bg_type)            \
  u32 pixel_combine = color_combine_mask(layer);                              \
  render_scanline_skip_alpha(bg_type, transparent)                            \

#define render_scanline_extra_variables_transparent_color()                   \
  u32 pixel_combine = color_combine_mask(layer)                               \

#define render_scanline_extra_variables_transparent_color16(bg_type)          \
  render_scanline_extra_variables_transparent_color()                         \

#define render_scanline_extra_variables_transparent_color32(bg_type)          \
  render_scanline_extra_variables_transparent_color()                         \




static const u32 map_widths[] = { 256, 512, 256, 512 };

// Build text scanline rendering functions.

#define render_scanline_text_builder(combine_op, alpha_op)                    \
static void render_scanline_text_##combine_op##_##alpha_op(u32 layer,         \
 u32 start, u32 end, void *scanline)                                          \
{                                                                             \
  render_scanline_extra_variables_##combine_op##_##alpha_op(text);            \
  u32 bg_control = io_registers[REG_BG0CNT + layer];                          \
  u32 map_size = (bg_control >> 14) & 0x03;                                   \
  u32 map_width = map_widths[map_size];                                       \
  u32 horizontal_offset =                                                     \
   (io_registers[REG_BG0HOFS + (layer * 2)] + start) % 512;                   \
  u32 vertical_offset = (io_registers[REG_VCOUNT] +                           \
   io_registers[REG_BG0VOFS + (layer * 2)]) % 512;                            \
  u32 current_pixel;                                                          \
  u32 current_pixels;                                                         \
  u32 partial_tile_run = 0;                                                   \
  u32 partial_tile_offset;                                                    \
  u32 tile_run;                                                               \
  u32 i;                                                                      \
  render_scanline_dest_##alpha_op *dest_ptr =                                 \
   ((render_scanline_dest_##alpha_op *)scanline) + start;                     \
                                                                              \
  u16 *map_base = (u16 *)(vram + ((bg_control >> 8) & 0x1F) * (1024 * 2));    \
  u16 *map_ptr, *second_ptr;                                                  \
  u8 *tile_ptr;                                                               \
                                                                              \
  end -= start;                                                               \
                                                                              \
  if((map_size & 0x02) && (vertical_offset >= 256))                           \
  {                                                                           \
    map_base += ((map_width / 8) * 32) +                                      \
     (((vertical_offset - 256) / 8) * 32);                                    \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    map_base += (((vertical_offset % 256) / 8) * 32);                         \
  }                                                                           \
                                                                              \
  if(map_size & 0x01)                                                         \
  {                                                                           \
    if(horizontal_offset >= 256)                                              \
    {                                                                         \
      horizontal_offset -= 256;                                               \
      map_ptr = map_base + (32 * 32) + (horizontal_offset / 8);               \
      second_ptr = map_base;                                                  \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      map_ptr = map_base + (horizontal_offset / 8);                           \
      second_ptr = map_base + (32 * 32);                                      \
    }                                                                         \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    horizontal_offset %= 256;                                                 \
    map_ptr = map_base + (horizontal_offset / 8);                             \
    second_ptr = map_base;                                                    \
  }                                                                           \
                                                                              \
  if(bg_control & 0x80)                                                       \
  {                                                                           \
    tile_render(8bpp, combine_op, alpha_op);                                  \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    tile_render(4bpp, combine_op, alpha_op);                                  \
  }                                                                           \
}                                                                             \

render_scanline_text_builder(base, normal);
render_scanline_text_builder(transparent, normal);
render_scanline_text_builder(base, color16);
render_scanline_text_builder(transparent, color16);
render_scanline_text_builder(base, color32);
render_scanline_text_builder(transparent, color32);
render_scanline_text_builder(base, alpha);
render_scanline_text_builder(transparent, alpha);


s32 affine_reference_x[2];
s32 affine_reference_y[2];

#define affine_render_bg_pixel_normal()                                       \
  current_pixel = palette_ram_converted[0]                                    \

#define affine_render_bg_pixel_alpha()                                        \
  current_pixel = bg_combine                                                  \

#define affine_render_bg_pixel_color16()                                      \
  affine_render_bg_pixel_alpha()                                              \

#define affine_render_bg_pixel_color32()                                      \
  affine_render_bg_pixel_alpha()                                              \

#define affine_render_bg_pixel_base(alpha_op)                                 \
  affine_render_bg_pixel_##alpha_op()                                         \

#define affine_render_bg_pixel_transparent(alpha_op)                          \

#define affine_render_bg_pixel_copy(alpha_op)                                 \

#define affine_render_bg_base(alpha_op)                                       \
  dest_ptr[0] = current_pixel

#define affine_render_bg_transparent(alpha_op)                                \

#define affine_render_bg_copy(alpha_op)                                       \

#define affine_render_bg_remainder_base(alpha_op)                             \
  affine_render_bg_pixel_##alpha_op();                                        \
  for(; i < end; i++)                                                         \
  {                                                                           \
    affine_render_bg_base(alpha_op);                                          \
    advance_dest_ptr_base(1);                                                 \
  }                                                                           \

#define affine_render_bg_remainder_transparent(alpha_op)                      \

#define affine_render_bg_remainder_copy(alpha_op)                             \

#define affine_render_next(combine_op)                                        \
  source_x += dx;                                                             \
  source_y += dy;                                                             \
  advance_dest_ptr_##combine_op(1)                                            \

#define affine_render_scale_offset()                                          \
  tile_base += ((pixel_y % 8) * 8);                                           \
  map_base += (pixel_y / 8) << map_pitch                                      \

#define affine_render_scale_pixel(combine_op, alpha_op)                       \
  map_offset = (pixel_x / 8);                                                 \
  if(map_offset != last_map_offset)                                           \
  {                                                                           \
    tile_ptr = tile_base + (map_base[map_offset] * 64);                       \
    last_map_offset = map_offset;                                             \
  }                                                                           \
  tile_ptr = tile_base + (map_base[(pixel_x / 8)] * 64);                      \
  current_pixel = tile_ptr[(pixel_x % 8)];                                    \
  tile_8bpp_draw_##combine_op(0, none, 0, alpha_op);                          \
  affine_render_next(combine_op)                                              \

#define affine_render_scale(combine_op, alpha_op)                             \
{                                                                             \
  pixel_y = source_y >> 8;                                                    \
  u32 i = 0;                                                                  \
  affine_render_bg_pixel_##combine_op(alpha_op);                              \
  if((u32)pixel_y < (u32)width_height)                                        \
  {                                                                           \
    affine_render_scale_offset();                                             \
    for(; i < end; i++)                                                       \
    {                                                                         \
      pixel_x = source_x >> 8;                                                \
                                                                              \
      if((u32)pixel_x < (u32)width_height)                                    \
      {                                                                       \
        break;                                                                \
      }                                                                       \
                                                                              \
      affine_render_bg_##combine_op(alpha_op);                                \
      affine_render_next(combine_op);                                         \
    }                                                                         \
                                                                              \
    for(; i < end; i++)                                                       \
    {                                                                         \
      pixel_x = source_x >> 8;                                                \
                                                                              \
      if((u32)pixel_x >= (u32)width_height)                                   \
        break;                                                                \
                                                                              \
      affine_render_scale_pixel(combine_op, alpha_op);                        \
    }                                                                         \
  }                                                                           \
  affine_render_bg_remainder_##combine_op(alpha_op);                          \
}                                                                             \

#define affine_render_scale_wrap(combine_op, alpha_op)                        \
{                                                                             \
  u32 wrap_mask = width_height - 1;                                           \
  pixel_y = (source_y >> 8) & wrap_mask;                                      \
  if((u32)pixel_y < (u32)width_height)                                        \
  {                                                                           \
    affine_render_scale_offset();                                             \
    for(i = 0; i < end; i++)                                                  \
    {                                                                         \
      pixel_x = (source_x >> 8) & wrap_mask;                                  \
      affine_render_scale_pixel(combine_op, alpha_op);                        \
    }                                                                         \
  }                                                                           \
}                                                                             \


#define affine_render_rotate_pixel(combine_op, alpha_op)                      \
  map_offset = (pixel_x / 8) + ((pixel_y / 8) << map_pitch);                  \
  if(map_offset != last_map_offset)                                           \
  {                                                                           \
    tile_ptr = tile_base + (map_base[map_offset] * 64);                       \
    last_map_offset = map_offset;                                             \
  }                                                                           \
                                                                              \
  current_pixel = tile_ptr[(pixel_x % 8) + ((pixel_y % 8) * 8)];              \
  tile_8bpp_draw_##combine_op(0, none, 0, alpha_op);                          \
  affine_render_next(combine_op)                                              \

#define affine_render_rotate(combine_op, alpha_op)                            \
{                                                                             \
  affine_render_bg_pixel_##combine_op(alpha_op);                              \
  for(i = 0; i < end; i++)                                                    \
  {                                                                           \
    pixel_x = source_x >> 8;                                                  \
    pixel_y = source_y >> 8;                                                  \
                                                                              \
    if(((u32)pixel_x < (u32)width_height) &&                                  \
     ((u32)pixel_y < (u32)width_height))                                      \
    {                                                                         \
      break;                                                                  \
    }                                                                         \
    affine_render_bg_##combine_op(alpha_op);                                  \
    affine_render_next(combine_op);                                           \
  }                                                                           \
                                                                              \
  for(; i < end; i++)                                                         \
  {                                                                           \
    pixel_x = source_x >> 8;                                                  \
    pixel_y = source_y >> 8;                                                  \
                                                                              \
    if(((u32)pixel_x >= (u32)width_height) ||                                 \
     ((u32)pixel_y >= (u32)width_height))                                     \
    {                                                                         \
      affine_render_bg_remainder_##combine_op(alpha_op);                      \
      break;                                                                  \
    }                                                                         \
                                                                              \
    affine_render_rotate_pixel(combine_op, alpha_op);                         \
  }                                                                           \
}                                                                             \

#define affine_render_rotate_wrap(combine_op, alpha_op)                       \
{                                                                             \
  u32 wrap_mask = width_height - 1;                                           \
  for(i = 0; i < end; i++)                                                    \
  {                                                                           \
    pixel_x = (source_x >> 8) & wrap_mask;                                    \
    pixel_y = (source_y >> 8) & wrap_mask;                                    \
                                                                              \
    affine_render_rotate_pixel(combine_op, alpha_op);                         \
  }                                                                           \
}                                                                             \


// Build affine background renderers.

#define render_scanline_affine_builder(combine_op, alpha_op)                  \
void render_scanline_affine_##combine_op##_##alpha_op(u32 layer,              \
 u32 start, u32 end, void *scanline)                                          \
{                                                                             \
  render_scanline_extra_variables_##combine_op##_##alpha_op(affine);          \
  u32 bg_control = io_registers[REG_BG0CNT + layer];                          \
  u32 current_pixel;                                                          \
  s32 source_x, source_y;                                                     \
  u32 pixel_x, pixel_y;                                                       \
  u32 layer_offset = (layer - 2) * 8;                                         \
  s32 dx, dy;                                                                 \
  u32 map_size = (bg_control >> 14) & 0x03;                                   \
  u32 width_height = 1 << (7 + map_size);                                     \
  u32 map_pitch = map_size + 4;                                               \
  u8 *map_base = vram + (((bg_control >> 8) & 0x1F) * (1024 * 2));            \
  u8 *tile_base = vram + (((bg_control >> 2) & 0x03) * (1024 * 16));          \
  u8 *tile_ptr = NULL;                                                        \
  u32 map_offset, last_map_offset = (u32)-1;                                  \
  u32 i;                                                                      \
  render_scanline_dest_##alpha_op *dest_ptr =                                 \
   ((render_scanline_dest_##alpha_op *)scanline) + start;                     \
                                                                              \
  dx = (s16)io_registers[REG_BG2PA + layer_offset];                           \
  dy = (s16)io_registers[REG_BG2PC + layer_offset];                           \
  source_x = affine_reference_x[layer - 2] + (start * dx);                    \
  source_y = affine_reference_y[layer - 2] + (start * dy);                    \
                                                                              \
  end -= start;                                                               \
                                                                              \
  switch(((bg_control >> 12) & 0x02) | (dy != 0))                             \
  {                                                                           \
    case 0x00:                                                                \
      affine_render_scale(combine_op, alpha_op);                              \
      break;                                                                  \
                                                                              \
    case 0x01:                                                                \
      affine_render_rotate(combine_op, alpha_op);                             \
      break;                                                                  \
                                                                              \
    case 0x02:                                                                \
      affine_render_scale_wrap(combine_op, alpha_op);                         \
      break;                                                                  \
                                                                              \
    case 0x03:                                                                \
      affine_render_rotate_wrap(combine_op, alpha_op);                        \
      break;                                                                  \
  }                                                                           \
}                                                                             \

render_scanline_affine_builder(base, normal);
render_scanline_affine_builder(transparent, normal);
render_scanline_affine_builder(base, color16);
render_scanline_affine_builder(transparent, color16);
render_scanline_affine_builder(base, color32);
render_scanline_affine_builder(transparent, color32);
render_scanline_affine_builder(base, alpha);
render_scanline_affine_builder(transparent, alpha);


#define bitmap_render_pixel_mode3(alpha_op)                                   \
  convert_palette(current_pixel);                                             \
  *dest_ptr = current_pixel                                                   \

#define bitmap_render_pixel_mode4(alpha_op)                                   \
  tile_expand_base_##alpha_op(0)                                              \

#define bitmap_render_pixel_mode5(alpha_op)                                   \
  bitmap_render_pixel_mode3(alpha_op)                                         \


#define bitmap_render_scale(type, alpha_op, width, height)                    \
  pixel_y = (source_y >> 8);                                                  \
  if((u32)pixel_y < (u32)height)                                              \
  {                                                                           \
    pixel_x = (source_x >> 8);                                                \
    src_ptr += (pixel_y * width);                                             \
    if(dx == 0x100)                                                           \
    {                                                                         \
      if(pixel_x < 0)                                                         \
      {                                                                       \
        end += pixel_x;                                                       \
        dest_ptr -= pixel_x;                                                  \
        pixel_x = 0;                                                          \
      }                                                                       \
      else                                                                    \
                                                                              \
      if(pixel_x > 0)                                                         \
      {                                                                       \
        src_ptr += pixel_x;                                                   \
      }                                                                       \
                                                                              \
      if((pixel_x + end) >= width)                                            \
        end = (width - pixel_x);                                              \
                                                                              \
      for(i = 0; (s32)i < (s32)end; i++)                                      \
      {                                                                       \
        current_pixel = *src_ptr;                                             \
        bitmap_render_pixel_##type(alpha_op);                                 \
        src_ptr++;                                                            \
        dest_ptr++;                                                           \
      }                                                                       \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      if((u32)(source_y >> 8) < (u32)height)                                  \
      {                                                                       \
        for(i = 0; i < end; i++)                                              \
        {                                                                     \
          pixel_x = (source_x >> 8);                                          \
                                                                              \
          if((u32)pixel_x < (u32)width)                                       \
            break;                                                            \
                                                                              \
          source_x += dx;                                                     \
          dest_ptr++;                                                         \
        }                                                                     \
                                                                              \
        for(; i < end; i++)                                                   \
        {                                                                     \
          pixel_x = (source_x >> 8);                                          \
                                                                              \
          if((u32)pixel_x >= (u32)width)                                      \
            break;                                                            \
                                                                              \
          current_pixel = src_ptr[pixel_x];                                   \
          bitmap_render_pixel_##type(alpha_op);                               \
                                                                              \
          source_x += dx;                                                     \
          dest_ptr++;                                                         \
        }                                                                     \
      }                                                                       \
    }                                                                         \
  }                                                                           \

#define bitmap_render_rotate(type, alpha_op, width, height)                   \
  for(i = 0; i < end; i++)                                                    \
  {                                                                           \
    pixel_x = source_x >> 8;                                                  \
    pixel_y = source_y >> 8;                                                  \
                                                                              \
    if(((u32)pixel_x < (u32)width) && ((u32)pixel_y < (u32)height))           \
      break;                                                                  \
                                                                              \
    source_x += dx;                                                           \
    source_y += dy;                                                           \
    dest_ptr++;                                                               \
  }                                                                           \
                                                                              \
  for(; i < end; i++)                                                         \
  {                                                                           \
    pixel_x = (source_x >> 8);                                                \
    pixel_y = (source_y >> 8);                                                \
                                                                              \
    if(((u32)pixel_x >= (u32)width) || ((u32)pixel_y >= (u32)height))         \
      break;                                                                  \
                                                                              \
    current_pixel = src_ptr[pixel_x + (pixel_y * width)];                     \
     bitmap_render_pixel_##type(alpha_op);                                    \
                                                                              \
    source_x += dx;                                                           \
    source_y += dy;                                                           \
    dest_ptr++;                                                               \
  }                                                                           \


#define render_scanline_vram_setup_mode3()                                    \
  u16 *src_ptr = (u16 *)vram                                                  \

#define render_scanline_vram_setup_mode5()                                    \
  u16 *src_ptr;                                                               \
  if(io_registers[REG_DISPCNT] & 0x10)                                        \
    src_ptr = (u16 *)(vram + 0xA000);                                         \
  else                                                                        \
    src_ptr = (u16 *)vram                                                     \


#ifdef RENDER_COLOR16_NORMAL

#define render_scanline_vram_setup_mode4()                                    \
  const u32 pixel_combine = 0;                                                \
  u8 *src_ptr;                                                                \
  if(io_registers[REG_DISPCNT] & 0x10)                                        \
    src_ptr = vram + 0xA000;                                                  \
  else                                                                        \
    src_ptr = vram                                                            \


#else

#define render_scanline_vram_setup_mode4()                                    \
  u16 *palette = palette_ram_converted;                                       \
  u8 *src_ptr;                                                                \
  if(io_registers[REG_DISPCNT] & 0x10)                                        \
    src_ptr = vram + 0xA000;                                                  \
  else                                                                        \
    src_ptr = vram                                                            \

#endif



// Build bitmap scanline rendering functions.

#define render_scanline_bitmap_builder(type, alpha_op, width, height)         \
static void render_scanline_bitmap_##type##_##alpha_op(u32 start, u32 end,    \
 void *scanline)                                                              \
{                                                                             \
  u32 current_pixel;                                                          \
  s32 source_x, source_y;                                                     \
  s32 pixel_x, pixel_y;                                                       \
                                                                              \
  s32 dx = (s16)io_registers[REG_BG2PA];                                      \
  s32 dy = (s16)io_registers[REG_BG2PC];                                      \
                                                                              \
  u32 i;                                                                      \
                                                                              \
  render_scanline_dest_##alpha_op *dest_ptr =                                 \
   ((render_scanline_dest_##alpha_op *)scanline) + start;                     \
  render_scanline_vram_setup_##type();                                        \
                                                                              \
  end -= start;                                                               \
                                                                              \
  source_x = affine_reference_x[0] + (start * dx);                            \
  source_y = affine_reference_y[0] + (start * dy);                            \
                                                                              \
  if(dy == 0)                                                                 \
  {                                                                           \
    bitmap_render_scale(type, alpha_op, width, height);                       \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    bitmap_render_rotate(type, alpha_op, width, height);                      \
  }                                                                           \
}                                                                             \

render_scanline_bitmap_builder(mode3, normal, 240, 160);
render_scanline_bitmap_builder(mode4, normal, 240, 160);
render_scanline_bitmap_builder(mode5, normal, 160, 128);


// Fill in the renderers for a layer based on the mode type,

#define tile_layer_render_functions(type)                                     \
{                                                                             \
  render_scanline_##type##_base_normal,                                       \
  render_scanline_##type##_transparent_normal,                                \
  render_scanline_##type##_base_alpha,                                        \
  render_scanline_##type##_transparent_alpha,                                 \
  render_scanline_##type##_base_color16,                                      \
  render_scanline_##type##_transparent_color16,                               \
  render_scanline_##type##_base_color32,                                      \
  render_scanline_##type##_transparent_color32                                \
}                                                                             \


// Use if a layer is unsupported for that mode.

#define tile_layer_render_null()                                              \
{                                                                             \
  NULL, NULL, NULL, NULL                                                      \
}                                                                             \

#define bitmap_layer_render_functions(type)                                   \
{                                                                             \
  render_scanline_bitmap_##type##_normal                                      \
}                                                                             \

// Structs containing functions to render the layers for each mode, for
// each render type.
static const tile_layer_render_struct tile_mode_renderers[3][4] =
{
  {
    tile_layer_render_functions(text), tile_layer_render_functions(text),
    tile_layer_render_functions(text), tile_layer_render_functions(text)
  },
  {
    tile_layer_render_functions(text), tile_layer_render_functions(text),
    tile_layer_render_functions(affine), tile_layer_render_functions(text)
  },
  {
    tile_layer_render_functions(text), tile_layer_render_functions(text),
    tile_layer_render_functions(affine), tile_layer_render_functions(affine)
  }
};

static const bitmap_layer_render_struct bitmap_mode_renderers[3] =
{
  bitmap_layer_render_functions(mode3),
  bitmap_layer_render_functions(mode4),
  bitmap_layer_render_functions(mode5)
};


#define render_scanline_layer_functions_tile()                                \
  const tile_layer_render_struct *layer_renderers =                           \
   tile_mode_renderers[dispcnt & 0x07]                                        \

#define render_scanline_layer_functions_bitmap()                              \
  const bitmap_layer_render_struct *layer_renderers =                         \
   bitmap_mode_renderers + ((dispcnt & 0x07) - 3)                             \


// Adjust a flipped obj's starting position

#define obj_tile_offset_noflip(color_depth)                                   \

#define obj_tile_offset_flip(color_depth)                                     \
  + (tile_size_##color_depth * ((obj_width - 8) / 8))                         \


// Adjust the obj's starting point if it goes too far off the left edge of
// the screen.

#define obj_tile_right_offset_noflip(color_depth)                             \
  tile_ptr += (partial_tile_offset / 8) * tile_size_##color_depth             \

#define obj_tile_right_offset_flip(color_depth)                               \
  tile_ptr -= (partial_tile_offset / 8) * tile_size_##color_depth             \

// Get the current row offset into an obj in 1D map space

#define obj_tile_offset_1D(color_depth, flip_op)                              \
  tile_ptr = tile_base + ((obj_attribute_2 & 0x3FF) * 32)                     \
   + ((vertical_offset / 8) * (obj_width / 8) * tile_size_##color_depth)      \
   + ((vertical_offset % 8) * tile_width_##color_depth)                       \
   obj_tile_offset_##flip_op(color_depth)                                     \

// Get the current row offset into an obj in 2D map space

#define obj_tile_offset_2D(color_depth, flip_op)                              \
  tile_ptr = tile_base + ((obj_attribute_2 & 0x3FF) * 32)                     \
   + ((vertical_offset / 8) * 1024)                                           \
   + ((vertical_offset % 8) * tile_width_##color_depth)                       \
   obj_tile_offset_##flip_op(color_depth)                                     \


// Get the palette for 4bpp obj.

#define obj_get_palette_4bpp()                                                \
  current_palette = (obj_attribute_2 >> 8) & 0xF0                             \

#define obj_get_palette_8bpp()                                                \


// Render the current row of an obj.

#define obj_render(combine_op, color_depth, alpha_op, map_space, flip_op)     \
{                                                                             \
  obj_get_palette_##color_depth();                                            \
  obj_tile_offset_##map_space(color_depth, flip_op);                          \
                                                                              \
  if(obj_x < (s32)start)                                                      \
  {                                                                           \
    dest_ptr = scanline + start;                                              \
    pixel_run = obj_width - (start - obj_x);                                  \
    if((s32)pixel_run > 0)                                                    \
    {                                                                         \
      if((obj_x + obj_width) >= end)                                          \
      {                                                                       \
        pixel_run = end - start;                                              \
        partial_tile_offset = start - obj_x;                                  \
        obj_tile_right_offset_##flip_op(color_depth);                         \
        partial_tile_offset %= 8;                                             \
                                                                              \
        if(partial_tile_offset)                                               \
        {                                                                     \
          partial_tile_run = 8 - partial_tile_offset;                         \
          if((s32)pixel_run < (s32)partial_tile_run)                          \
          {                                                                   \
            if((s32)pixel_run > 0)                                            \
            {                                                                 \
              partial_tile_run = pixel_run;                                   \
              partial_tile_mid_obj(combine_op, color_depth, alpha_op,         \
               flip_op);                                                      \
            }                                                                 \
            continue;                                                         \
          }                                                                   \
          else                                                                \
          {                                                                   \
            pixel_run -= partial_tile_run;                                    \
            partial_tile_right_obj(combine_op, color_depth, alpha_op,         \
             flip_op);                                                        \
          }                                                                   \
        }                                                                     \
        tile_run = pixel_run / 8;                                             \
        multiple_tile_obj(combine_op, color_depth, alpha_op, flip_op);        \
        partial_tile_run = pixel_run % 8;                                     \
        if(partial_tile_run)                                                  \
        {                                                                     \
          partial_tile_left_obj(combine_op, color_depth, alpha_op,            \
           flip_op);                                                          \
        }                                                                     \
      }                                                                       \
      else                                                                    \
      {                                                                       \
        partial_tile_offset = start - obj_x;                                  \
        obj_tile_right_offset_##flip_op(color_depth);                         \
        partial_tile_offset %= 8;                                             \
        if(partial_tile_offset)                                               \
        {                                                                     \
          partial_tile_run = 8 - partial_tile_offset;                         \
          partial_tile_right_obj(combine_op, color_depth, alpha_op,           \
           flip_op);                                                          \
        }                                                                     \
        tile_run = pixel_run / 8;                                             \
        multiple_tile_obj(combine_op, color_depth, alpha_op, flip_op);        \
      }                                                                       \
    }                                                                         \
  }                                                                           \
  else                                                                        \
                                                                              \
  if((obj_x + obj_width) >= end)                                              \
  {                                                                           \
    pixel_run = end - obj_x;                                                  \
    if((s32)pixel_run > 0)                                                    \
    {                                                                         \
      dest_ptr = scanline + obj_x;                                            \
      tile_run = pixel_run / 8;                                               \
      multiple_tile_obj(combine_op, color_depth, alpha_op, flip_op);          \
      partial_tile_run = pixel_run % 8;                                       \
      if(partial_tile_run)                                                    \
      {                                                                       \
        partial_tile_left_obj(combine_op, color_depth, alpha_op, flip_op);    \
      }                                                                       \
    }                                                                         \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    dest_ptr = scanline + obj_x;                                              \
    tile_run = obj_width / 8;                                                 \
    multiple_tile_obj(combine_op, color_depth, alpha_op, flip_op);            \
  }                                                                           \
}                                                                             \

#define obj_scale_offset_1D(color_depth)                                      \
  tile_ptr = tile_base + ((obj_attribute_2 & 0x3FF) * 32)                     \
   + ((vertical_offset / 8) * (max_x / 8) * tile_size_##color_depth)          \
   + ((vertical_offset % 8) * tile_width_##color_depth)                       \

// Get the current row offset into an obj in 2D map space

#define obj_scale_offset_2D(color_depth)                                      \
  tile_ptr = tile_base + ((obj_attribute_2 & 0x3FF) * 32)                     \
   + ((vertical_offset / 8) * 1024)                                           \
   + ((vertical_offset % 8) * tile_width_##color_depth)                       \

#define obj_render_scale_pixel_4bpp(combine_op, alpha_op)                     \
  if(tile_x & 0x01)                                                           \
  {                                                                           \
    current_pixel = tile_ptr[tile_map_offset + ((tile_x >> 1) & 0x03)] >> 4;  \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    current_pixel =                                                           \
     tile_ptr[tile_map_offset + ((tile_x >> 1) & 0x03)] & 0x0F;               \
  }                                                                           \
                                                                              \
  tile_4bpp_draw_##combine_op(0, none, 0, alpha_op)                           \


#define obj_render_scale_pixel_8bpp(combine_op, alpha_op)                     \
  current_pixel = tile_ptr[tile_map_offset + (tile_x & 0x07)];                \
  tile_8bpp_draw_##combine_op(0, none, 0, alpha_op);                          \

#define obj_render_scale(combine_op, color_depth, alpha_op, map_space)        \
{                                                                             \
  u32 vertical_offset;                                                        \
  source_y += (y_delta * dmy);                                                \
  vertical_offset = (source_y >> 8);                                          \
  if((u32)vertical_offset < (u32)max_y)                                       \
  {                                                                           \
    obj_scale_offset_##map_space(color_depth);                                \
    source_x += (y_delta * dmx) - (middle_x * dx);                            \
                                                                              \
    for(i = 0; i < obj_width; i++)                                            \
    {                                                                         \
      tile_x = (source_x >> 8);                                               \
                                                                              \
      if((u32)tile_x < (u32)max_x)                                            \
        break;                                                                \
                                                                              \
      source_x += dx;                                                         \
      advance_dest_ptr_##combine_op(1);                                       \
    }                                                                         \
                                                                              \
    for(; i < obj_width; i++)                                                 \
    {                                                                         \
      tile_x = (source_x >> 8);                                               \
                                                                              \
      if((u32)tile_x >= (u32)max_x)                                           \
        break;                                                                \
                                                                              \
      tile_map_offset = (tile_x >> 3) * tile_size_##color_depth;              \
      obj_render_scale_pixel_##color_depth(combine_op, alpha_op);             \
                                                                              \
      source_x += dx;                                                         \
      advance_dest_ptr_##combine_op(1);                                       \
    }                                                                         \
  }                                                                           \
}                                                                             \


#define obj_rotate_offset_1D(color_depth)                                     \
  obj_tile_pitch = (max_x / 8) * tile_size_##color_depth                      \

#define obj_rotate_offset_2D(color_depth)                                     \
  obj_tile_pitch = 1024                                                       \

#define obj_render_rotate_pixel_4bpp(combine_op, alpha_op)                    \
  if(tile_x & 0x01)                                                           \
  {                                                                           \
    current_pixel = tile_ptr[tile_map_offset +                                \
     ((tile_x >> 1) & 0x03) + ((tile_y & 0x07) * obj_pitch)] >> 4;            \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    current_pixel = tile_ptr[tile_map_offset +                                \
     ((tile_x >> 1) & 0x03) + ((tile_y & 0x07) * obj_pitch)] & 0x0F;          \
  }                                                                           \
                                                                              \
  tile_4bpp_draw_##combine_op(0, none, 0, alpha_op)                           \

#define obj_render_rotate_pixel_8bpp(combine_op, alpha_op)                    \
  current_pixel = tile_ptr[tile_map_offset +                                  \
   (tile_x & 0x07) + ((tile_y & 0x07) * obj_pitch)];                          \
                                                                              \
  tile_8bpp_draw_##combine_op(0, none, 0, alpha_op)                           \

#define obj_render_rotate(combine_op, color_depth, alpha_op, map_space)       \
{                                                                             \
  tile_ptr = tile_base + ((obj_attribute_2 & 0x3FF) * 32);                    \
  obj_rotate_offset_##map_space(color_depth);                                 \
                                                                              \
  source_x += (y_delta * dmx) - (middle_x * dx);                              \
  source_y += (y_delta * dmy) - (middle_x * dy);                              \
                                                                              \
  for(i = 0; i < obj_width; i++)                                              \
  {                                                                           \
    tile_x = (source_x >> 8);                                                 \
    tile_y = (source_y >> 8);                                                 \
                                                                              \
    if(((u32)tile_x < (u32)max_x) && ((u32)tile_y < (u32)max_y))              \
      break;                                                                  \
                                                                              \
    source_x += dx;                                                           \
    source_y += dy;                                                           \
    advance_dest_ptr_##combine_op(1);                                         \
  }                                                                           \
                                                                              \
  for(; i < obj_width; i++)                                                   \
  {                                                                           \
    tile_x = (source_x >> 8);                                                 \
    tile_y = (source_y >> 8);                                                 \
                                                                              \
    if(((u32)tile_x >= (u32)max_x) || ((u32)tile_y >= (u32)max_y))            \
      break;                                                                  \
                                                                              \
    tile_map_offset = ((tile_x >> 3) * tile_size_##color_depth) +             \
    ((tile_y >> 3) * obj_tile_pitch);                                         \
    obj_render_rotate_pixel_##color_depth(combine_op, alpha_op);              \
                                                                              \
    source_x += dx;                                                           \
    source_y += dy;                                                           \
    advance_dest_ptr_##combine_op(1);                                         \
  }                                                                           \
}                                                                             \

// Render the current row of an affine transformed OBJ.

#define obj_render_affine(combine_op, color_depth, alpha_op, map_space)       \
{                                                                             \
  s16 *params = (s16 *)oam_ram + (((obj_attribute_1 >> 9) & 0x1F) * 16);      \
  s32 dx = params[3];                                                         \
  s32 dmx = params[7];                                                        \
  s32 dy = params[11];                                                        \
  s32 dmy = params[15];                                                       \
  s32 source_x, source_y;                                                     \
  s32 tile_x, tile_y;                                                         \
  u32 tile_map_offset;                                                        \
  s32 middle_x;                                                               \
  s32 middle_y;                                                               \
  s32 max_x = obj_width;                                                      \
  s32 max_y = obj_height;                                                     \
  s32 y_delta;                                                                \
  u32 obj_pitch = tile_width_##color_depth;                                   \
  u32 obj_tile_pitch;                                                         \
                                                                              \
  middle_x = (obj_width / 2);                                                 \
  middle_y = (obj_height / 2);                                                \
                                                                              \
  source_x = (middle_x << 8);                                                 \
  source_y = (middle_y << 8);                                                 \
                                                                              \
                                                                              \
  if(obj_attribute_0 & 0x200)                                                 \
  {                                                                           \
    obj_width *= 2;                                                           \
    obj_height *= 2;                                                          \
    middle_x *= 2;                                                            \
    middle_y *= 2;                                                            \
  }                                                                           \
                                                                              \
  if((s32)obj_x < (s32)start)                                                 \
  {                                                                           \
    u32 x_delta = start - obj_x;                                              \
    middle_x -= x_delta;                                                      \
    obj_width -= x_delta;                                                     \
    obj_x = start;                                                            \
                                                                              \
    if((s32)obj_width <= 0)                                                   \
      continue;                                                               \
  }                                                                           \
                                                                              \
  if((s32)(obj_x + obj_width) >= (s32)end)                                    \
  {                                                                           \
    obj_width = end - obj_x;                                                  \
                                                                              \
    if((s32)obj_width <= 0)                                                   \
      continue;                                                               \
  }                                                                           \
  dest_ptr = scanline + obj_x;                                                \
                                                                              \
  y_delta = vcount - (obj_y + middle_y);                                      \
                                                                              \
  obj_get_palette_##color_depth();                                            \
                                                                              \
  if(dy == 0)                                                                 \
  {                                                                           \
    obj_render_scale(combine_op, color_depth, alpha_op, map_space);           \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    obj_render_rotate(combine_op, color_depth, alpha_op, map_space);          \
  }                                                                           \
}                                                                             \

static const u32 obj_width_table[] =
  { 8, 16, 32, 64, 16, 32, 32, 64, 8, 8, 16, 32 };
static const u32 obj_height_table[] =
  { 8, 16, 32, 64, 8, 8, 16, 32, 16, 32, 32, 64 };

static u8 obj_priority_list[5][160][128];
static u32 obj_priority_count[5][160];
static u32 obj_alpha_count[160];


// Build obj rendering functions

#ifdef RENDER_COLOR16_NORMAL

#define render_scanline_obj_extra_variables_normal(bg_type)                   \
  const u32 pixel_combine = (1 << 8)                                          \

#else

#define render_scanline_obj_extra_variables_normal(bg_type)                   \
  u16 *palette = palette_ram_converted + 256                                  \

#endif


#define render_scanline_obj_extra_variables_color()                           \
  u32 pixel_combine = color_combine_mask(4) | (1 << 8)                        \

#define render_scanline_obj_extra_variables_alpha_obj(map_space)              \
  render_scanline_obj_extra_variables_color();                                \
  u32 dest;                                                                   \
  if((pixel_combine & 0x00000200) == 0)                                       \
  {                                                                           \
    render_scanline_obj_color32_##map_space(priority, start, end, scanline);  \
    return;                                                                   \
  }                                                                           \

#define render_scanline_obj_extra_variables_color16(map_space)                \
  render_scanline_obj_extra_variables_color()                                 \

#define render_scanline_obj_extra_variables_color32(map_space)                \
  render_scanline_obj_extra_variables_color()                                 \

#define render_scanline_obj_extra_variables_partial_alpha(map_space)          \
  render_scanline_obj_extra_variables_color();                                \
  u32 base_pixel_combine = pixel_combine;                                     \
  u32 dest                                                                    \

#define render_scanline_obj_extra_variables_copy(type)                        \
  u32 bldcnt = io_registers[REG_BLDCNT];                                      \
  u32 dispcnt = io_registers[REG_DISPCNT];                                    \
  u32 obj_enable = io_registers[REG_WINOUT] >> 8;                             \
  render_scanline_layer_functions_##type();                                   \
  u32 copy_start, copy_end;                                                   \
  u16 copy_buffer[240];                                                       \
  u16 *copy_ptr                                                               \

#define render_scanline_obj_extra_variables_copy_tile(map_space)              \
  render_scanline_obj_extra_variables_copy(tile)                              \

#define render_scanline_obj_extra_variables_copy_bitmap(map_space)            \
  render_scanline_obj_extra_variables_copy(bitmap)                            \


#define render_scanline_obj_main(combine_op, alpha_op, map_space)             \
  if(obj_attribute_0 & 0x100)                                                 \
  {                                                                           \
    if((obj_attribute_0 >> 13) & 0x01)                                        \
    {                                                                         \
      obj_render_affine(combine_op, 8bpp, alpha_op, map_space);               \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      obj_render_affine(combine_op, 4bpp, alpha_op, map_space);               \
    }                                                                         \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    vertical_offset = vcount - obj_y;                                         \
                                                                              \
    if((obj_attribute_1 >> 13) & 0x01)                                        \
      vertical_offset = obj_height - vertical_offset - 1;                     \
                                                                              \
    switch(((obj_attribute_0 >> 12) & 0x02) |                                 \
     ((obj_attribute_1 >> 12) & 0x01))                                        \
    {                                                                         \
      case 0x0:                                                               \
        obj_render(combine_op, 4bpp, alpha_op, map_space, noflip);            \
        break;                                                                \
                                                                              \
      case 0x1:                                                               \
        obj_render(combine_op, 4bpp, alpha_op, map_space, flip);              \
        break;                                                                \
                                                                              \
      case 0x2:                                                               \
        obj_render(combine_op, 8bpp, alpha_op, map_space, noflip);            \
        break;                                                                \
                                                                              \
      case 0x3:                                                               \
        obj_render(combine_op, 8bpp, alpha_op, map_space, flip);              \
        break;                                                                \
    }                                                                         \
  }                                                                           \

#define render_scanline_obj_no_partial_alpha(combine_op, alpha_op, map_space) \
  render_scanline_obj_main(combine_op, alpha_op, map_space)                   \

#define render_scanline_obj_partial_alpha(combine_op, alpha_op, map_space)    \
  if((obj_attribute_0 >> 10) & 0x03)                                          \
  {                                                                           \
    pixel_combine = 0x00000300;                                               \
    render_scanline_obj_main(combine_op, alpha_obj, map_space);               \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    pixel_combine = base_pixel_combine;                                       \
    render_scanline_obj_main(combine_op, color32, map_space);                 \
  }                                                                           \

#define render_scanline_obj_prologue_transparent(alpha_op)                    \

#define render_scanline_obj_prologue_copy_body(type)                          \
  copy_start = obj_x;                                                         \
  if(obj_attribute_0 & 0x200)                                                 \
    copy_end = obj_x + (obj_width * 2);                                       \
  else                                                                        \
    copy_end = obj_x + obj_width;                                             \
                                                                              \
  if(copy_start < start)                                                      \
    copy_start = start;                                                       \
  if(copy_end > end)                                                          \
    copy_end = end;                                                           \
                                                                              \
  if((copy_start < end) && (copy_end > start))                                \
  {                                                                           \
    render_scanline_conditional_##type(copy_start, copy_end, copy_buffer,     \
     obj_enable, dispcnt, bldcnt, layer_renderers);                           \
    copy_ptr = copy_buffer + copy_start;                                      \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    continue;                                                                 \
  }                                                                           \

#define render_scanline_obj_prologue_copy_tile()                              \
  render_scanline_obj_prologue_copy_body(tile)                                \

#define render_scanline_obj_prologue_copy_bitmap()                            \
  render_scanline_obj_prologue_copy_body(bitmap)                              \

#define render_scanline_obj_prologue_copy(alpha_op)                           \
  render_scanline_obj_prologue_##alpha_op()                                   \


#define render_scanline_obj_builder(combine_op, alpha_op, map_space,          \
 partial_alpha_op)                                                            \
static void render_scanline_obj_##alpha_op##_##map_space(u32 priority,        \
 u32 start, u32 end, render_scanline_dest_##alpha_op *scanline)               \
{                                                                             \
  render_scanline_obj_extra_variables_##alpha_op(map_space);                  \
  s32 obj_num, i;                                                             \
  s32 obj_x, obj_y;                                                           \
  s32 obj_size;                                                               \
  s32 obj_width, obj_height;                                                  \
  u32 obj_attribute_0, obj_attribute_1, obj_attribute_2;                      \
  s32 vcount = io_registers[REG_VCOUNT];                                      \
  u32 tile_run;                                                               \
  u32 current_pixels;                                                         \
  u32 current_pixel;                                                          \
  u32 current_palette;                                                        \
  u32 vertical_offset;                                                        \
  u32 partial_tile_run, partial_tile_offset;                                  \
  u32 pixel_run;                                                              \
  u16 *oam_ptr;                                                               \
  render_scanline_dest_##alpha_op *dest_ptr;                                  \
  u8 *tile_base = vram + 0x10000;                                             \
  u8 *tile_ptr;                                                               \
  u32 obj_count = obj_priority_count[priority][vcount];                       \
  u8 *obj_list = obj_priority_list[priority][vcount];                         \
                                                                              \
  for(obj_num = 0; obj_num < obj_count; obj_num++)                            \
  {                                                                           \
    oam_ptr = oam_ram + (obj_list[obj_num] * 4);                              \
    obj_attribute_0 = oam_ptr[0];                                             \
    obj_attribute_1 = oam_ptr[1];                                             \
    obj_attribute_2 = oam_ptr[2];                                             \
    obj_size = ((obj_attribute_0 >> 12) & 0x0C) | (obj_attribute_1 >> 14);    \
                                                                              \
    obj_x = (s32)(obj_attribute_1 << 23) >> 23;                               \
    obj_width = obj_width_table[obj_size];                                    \
                                                                              \
    render_scanline_obj_prologue_##combine_op(alpha_op);                      \
                                                                              \
    obj_y = obj_attribute_0 & 0xFF;                                           \
                                                                              \
    if(obj_y > 160)                                                           \
      obj_y -= 256;                                                           \
                                                                              \
    obj_height = obj_height_table[obj_size];                                  \
    render_scanline_obj_##partial_alpha_op(combine_op, alpha_op, map_space);  \
  }                                                                           \
}                                                                             \

render_scanline_obj_builder(transparent, normal, 1D, no_partial_alpha);
render_scanline_obj_builder(transparent, normal, 2D, no_partial_alpha);
render_scanline_obj_builder(transparent, color16, 1D, no_partial_alpha);
render_scanline_obj_builder(transparent, color16, 2D, no_partial_alpha);
render_scanline_obj_builder(transparent, color32, 1D, no_partial_alpha);
render_scanline_obj_builder(transparent, color32, 2D, no_partial_alpha);
render_scanline_obj_builder(transparent, alpha_obj, 1D, no_partial_alpha);
render_scanline_obj_builder(transparent, alpha_obj, 2D, no_partial_alpha);
render_scanline_obj_builder(transparent, partial_alpha, 1D, partial_alpha);
render_scanline_obj_builder(transparent, partial_alpha, 2D, partial_alpha);
render_scanline_obj_builder(copy, copy_tile, 1D, no_partial_alpha);
render_scanline_obj_builder(copy, copy_tile, 2D, no_partial_alpha);
render_scanline_obj_builder(copy, copy_bitmap, 1D, no_partial_alpha);
render_scanline_obj_builder(copy, copy_bitmap, 2D, no_partial_alpha);



static void order_obj(u32 video_mode)
{
  s32 obj_num, priority, row;
  s32 obj_x, obj_y;
  s32 obj_size, obj_mode;
  s32 obj_width, obj_height;
  u32 obj_priority;
  u32 obj_attribute_0, obj_attribute_1, obj_attribute_2;
  u32 current_count;
  u16 *oam_ptr = oam_ram + 508;

  for(priority = 0; priority < 5; priority++)
  {
    for(row = 0; row < 160; row++)
    {
      obj_priority_count[priority][row] = 0;
    }
  }

  for(row = 0; row < 160; row++)
  {
    obj_alpha_count[row] = 0;
  }

  for(obj_num = 127; obj_num >= 0; obj_num--, oam_ptr -= 4)
  {
    obj_attribute_0 = oam_ptr[0];
    obj_attribute_2 = oam_ptr[2];
    obj_size = obj_attribute_0 & 0xC000;
    obj_priority = (obj_attribute_2 >> 10) & 0x03;
    obj_mode = (obj_attribute_0 >> 10) & 0x03;

    if(((obj_attribute_0 & 0x0300) != 0x0200) && (obj_size != 0xC000) &&
     (obj_mode != 3) && ((video_mode < 3) ||
     ((obj_attribute_2 & 0x3FF) >= 512)))
    {
      obj_y = obj_attribute_0 & 0xFF;
      if(obj_y > 160)
        obj_y -= 256;

      obj_attribute_1 = oam_ptr[1];
      obj_size = ((obj_size >> 12) & 0x0C) | (obj_attribute_1 >> 14);
      obj_height = obj_height_table[obj_size];
      obj_width = obj_width_table[obj_size];

      if(obj_attribute_0 & 0x200)
      {
        obj_height *= 2;
        obj_width *= 2;
      }

      if(((obj_y + obj_height) > 0) && (obj_y < 160))
      {
        obj_x = (s32)(obj_attribute_1 << 23) >> 23;

        if(((obj_x + obj_width) > 0) && (obj_x < 240))
        {
          if(obj_y < 0)
          {
            obj_height += obj_y;
            obj_y = 0;
          }

          if((obj_y + obj_height) >= 160)
          {
            obj_height = 160 - obj_y;
          }

          if(obj_mode == 1)
          {
            for(row = obj_y; row < obj_y + obj_height; row++)
            {
              current_count = obj_priority_count[obj_priority][row];
              obj_priority_list[obj_priority][row][current_count] = obj_num;
              obj_priority_count[obj_priority][row] = current_count + 1;
              obj_alpha_count[row]++;
            }
          }
          else
          {
            if(obj_mode == 2)
            {
              obj_priority = 4;
            }

            for(row = obj_y; row < obj_y + obj_height; row++)
            {
              current_count = obj_priority_count[obj_priority][row];
              obj_priority_list[obj_priority][row][current_count] = obj_num;
              obj_priority_count[obj_priority][row] = current_count + 1;
            }
          }
        }
      }
    }
  }
}

u32 layer_order[16];
u32 layer_count;

static void order_layers(u32 layer_flags)
{
  s32 priority, layer_number;
  layer_count = 0;

  for(priority = 3; priority >= 0; priority--)
  {
    for(layer_number = 3; layer_number >= 0; layer_number--)
    {
      if(((layer_flags >> layer_number) & 1) &&
       ((io_registers[REG_BG0CNT + layer_number] & 0x03) == priority))
      {
        layer_order[layer_count] = layer_number;
        layer_count++;
      }
    }

    if((obj_priority_count[priority][io_registers[REG_VCOUNT]] > 0)
     && (layer_flags & 0x10))
    {
      layer_order[layer_count] = priority | 0x04;
      layer_count++;
    }
  }
}

#define fill_line(_start, _end)                                               \
  u32 i;                                                                      \
                                                                              \
  for(i = _start; i < _end; i++)                                              \
  {                                                                           \
    dest_ptr[i] = color;                                                      \
  }                                                                           \


#define fill_line_color_normal()                                              \
  color = palette_ram_converted[color]                                        \

#define fill_line_color_alpha()                                               \

#define fill_line_color_color16()                                             \

#define fill_line_color_color32()                                             \

#define fill_line_builder(type)                                               \
static void fill_line_##type(u16 color, render_scanline_dest_##type *dest_ptr,\
 u32 start, u32 end)                                                          \
{                                                                             \
  fill_line_color_##type();                                                   \
  fill_line(start, end);                                                      \
}                                                                             \

fill_line_builder(normal);
fill_line_builder(alpha);
fill_line_builder(color16);
fill_line_builder(color32);


// Alpha blend two pixels (pixel_top and pixel_bottom).

#define blend_pixel()                                                         \
  pixel_bottom = palette_ram_converted[(pixel_pair >> 16) & 0x1FF];           \
  pixel_bottom = (pixel_bottom | (pixel_bottom << 16)) & 0x07E0F81F;          \
  pixel_top = ((pixel_top * blend_a) + (pixel_bottom * blend_b)) >> 4         \


// Alpha blend two pixels, allowing for saturation (individual channels > 31).
// The operation is optimized towards saturation not occuring.

#define blend_saturate_pixel()                                                \
  pixel_bottom = palette_ram_converted[(pixel_pair >> 16) & 0x1FF];           \
  pixel_bottom = (pixel_bottom | (pixel_bottom << 16)) & 0x07E0F81F;          \
  pixel_top = ((pixel_top * blend_a) + (pixel_bottom * blend_b)) >> 4;        \
  if(pixel_top & 0x08010020)                                                  \
  {                                                                           \
    if(pixel_top & 0x08000000)                                                \
      pixel_top |= 0x07E00000;                                                \
                                                                              \
    if(pixel_top & 0x00010000)                                                \
      pixel_top |= 0x0000F800;                                                \
                                                                              \
    if(pixel_top & 0x00000020)                                                \
      pixel_top |= 0x0000001F;                                                \
  }                                                                           \

#define brighten_pixel()                                                      \
  pixel_top = upper + ((pixel_top * blend) >> 4);                             \

#define darken_pixel()                                                        \
  pixel_top = (pixel_top * blend) >> 4;                                       \

#define effect_condition_alpha                                                \
  ((pixel_pair & 0x04000200) == 0x04000200)                                   \

#define effect_condition_fade(pixel_source)                                   \
  ((pixel_source & 0x00000200) == 0x00000200)                                 \

#define expand_pixel_no_dest(expand_type, pixel_source)                       \
  pixel_top = (pixel_top | (pixel_top << 16)) & 0x07E0F81F;                   \
  expand_type##_pixel();                                                      \
  pixel_top &= 0x07E0F81F;                                                    \
  pixel_top = (pixel_top >> 16) | pixel_top                                   \

#define expand_pixel(expand_type, pixel_source)                               \
  pixel_top = palette_ram_converted[pixel_source & 0x1FF];                    \
  expand_pixel_no_dest(expand_type, pixel_source);                            \
  *screen_dest_ptr = pixel_top                                                \

#define expand_loop(expand_type, effect_condition, pixel_source)              \
  screen_src_ptr += start;                                                    \
  screen_dest_ptr += start;                                                   \
                                                                              \
  end -= start;                                                               \
                                                                              \
  for(i = 0; i < end; i++)                                                    \
  {                                                                           \
    pixel_source = *screen_src_ptr;                                           \
    if(effect_condition)                                                      \
    {                                                                         \
      expand_pixel(expand_type, pixel_source);                                \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      *screen_dest_ptr =                                                      \
       palette_ram_converted[pixel_source & 0x1FF];                           \
    }                                                                         \
                                                                              \
    screen_src_ptr++;                                                         \
    screen_dest_ptr++;                                                        \
  }                                                                           \


#define expand_loop_partial_alpha(alpha_expand, expand_type)                  \
  screen_src_ptr += start;                                                    \
  screen_dest_ptr += start;                                                   \
                                                                              \
  end -= start;                                                               \
                                                                              \
  for(i = 0; i < end; i++)                                                    \
  {                                                                           \
    pixel_pair = *screen_src_ptr;                                             \
    if(effect_condition_fade(pixel_pair))                                     \
    {                                                                         \
      if(effect_condition_alpha)                                              \
      {                                                                       \
        expand_pixel(alpha_expand, pixel_pair);                               \
      }                                                                       \
      else                                                                    \
      {                                                                       \
        expand_pixel(expand_type, pixel_pair);                                \
      }                                                                       \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      *screen_dest_ptr =                                                      \
       palette_ram_converted[pixel_pair & 0x1FF];                             \
    }                                                                         \
                                                                              \
    screen_src_ptr++;                                                         \
    screen_dest_ptr++;                                                        \
  }                                                                           \


#define expand_partial_alpha(expand_type)                                     \
  if((blend_a + blend_b) > 16)                                                \
  {                                                                           \
    expand_loop_partial_alpha(blend_saturate, expand_type);                   \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    expand_loop_partial_alpha(blend, expand_type);                            \
  }                                                                           \



// Blend top two pixels of scanline with each other.

#ifdef RENDER_COLOR16_NORMAL

#ifndef ARM_ARCH

void expand_normal(u16 *screen_ptr, u32 start, u32 end)
{
  u32 i, pixel_source;
  screen_ptr += start;

  return;

  end -= start;

  for(i = 0; i < end; i++)
  {
    pixel_source = *screen_ptr;
    *screen_ptr = palette_ram_converted[pixel_source];

    screen_ptr++;
  }
}

#endif

#else

#define expand_normal(screen_ptr, start, end)

#endif


void expand_blend(u32 *screen_src_ptr, u16 *screen_dest_ptr,
 u32 start, u32 end);

#ifndef ARM_ARCH

void expand_blend(u32 *screen_src_ptr, u16 *screen_dest_ptr,
 u32 start, u32 end)
{
  u32 pixel_pair;
  u32 pixel_top, pixel_bottom;
  u32 bldalpha = io_registers[REG_BLDALPHA];
  u32 blend_a = bldalpha & 0x1F;
  u32 blend_b = (bldalpha >> 8) & 0x1F;
  u32 i;

  if(blend_a > 16)
    blend_a = 16;

  if(blend_b > 16)
    blend_b = 16;

  // The individual colors can saturate over 31, this should be taken
  // care of in an alternate pass as it incurs a huge additional speedhit.
  if((blend_a + blend_b) > 16)
  {
    expand_loop(blend_saturate, effect_condition_alpha, pixel_pair);
  }
  else
  {
    expand_loop(blend, effect_condition_alpha, pixel_pair);
  }
}

#endif

// Blend scanline with white.

static void expand_darken(u16 *screen_src_ptr, u16 *screen_dest_ptr,
 u32 start, u32 end)
{
  u32 pixel_top;
  s32 blend = 16 - (io_registers[REG_BLDY] & 0x1F);
  u32 i;

  if(blend < 0)
    blend = 0;

  expand_loop(darken, effect_condition_fade(pixel_top), pixel_top);
}


// Blend scanline with black.

static void expand_brighten(u16 *screen_src_ptr, u16 *screen_dest_ptr,
 u32 start, u32 end)
{
  u32 pixel_top;
  u32 blend = io_registers[REG_BLDY] & 0x1F;
  u32 upper;
  u32 i;

  if(blend > 16)
    blend = 16;

  upper = ((0x07E0F81F * blend) >> 4) & 0x07E0F81F;
  blend = 16 - blend;

  expand_loop(brighten, effect_condition_fade(pixel_top), pixel_top);

}


// Expand scanline such that if both top and bottom pass it's alpha,
// if only top passes it's as specified, and if neither pass it's normal.

static void expand_darken_partial_alpha(u32 *screen_src_ptr, u16 *screen_dest_ptr,
 u32 start, u32 end)
{
  s32 blend = 16 - (io_registers[REG_BLDY] & 0x1F);
  u32 pixel_pair;
  u32 pixel_top, pixel_bottom;
  u32 bldalpha = io_registers[REG_BLDALPHA];
  u32 blend_a = bldalpha & 0x1F;
  u32 blend_b = (bldalpha >> 8) & 0x1F;
  u32 i;

  if(blend < 0)
    blend = 0;

  if(blend_a > 16)
    blend_a = 16;

  if(blend_b > 16)
    blend_b = 16;

  expand_partial_alpha(darken);
}


static void expand_brighten_partial_alpha(u32 *screen_src_ptr, u16 *screen_dest_ptr,
 u32 start, u32 end)
{
  s32 blend = io_registers[REG_BLDY] & 0x1F;
  u32 pixel_pair;
  u32 pixel_top, pixel_bottom;
  u32 bldalpha = io_registers[REG_BLDALPHA];
  u32 blend_a = bldalpha & 0x1F;
  u32 blend_b = (bldalpha >> 8) & 0x1F;
  u32 upper;
  u32 i;

  if(blend > 16)
    blend = 16;

  upper = ((0x07E0F81F * blend) >> 4) & 0x07E0F81F;
  blend = 16 - blend;

  if(blend_a > 16)
    blend_a = 16;

  if(blend_b > 16)
    blend_b = 16;

  expand_partial_alpha(brighten);
}


// Render an OBJ layer from start to end, depending on the type (1D or 2D)
// stored in dispcnt.

#define render_obj_layer(type, dest, _start, _end)                            \
  current_layer &= ~0x04;                                                     \
  if(dispcnt & 0x40)                                                          \
    render_scanline_obj_##type##_1D(current_layer, _start, _end, dest);       \
  else                                                                        \
    render_scanline_obj_##type##_2D(current_layer, _start, _end, dest)        \


// Render a target all the way with the background color as taken from the
// palette.

#define fill_line_bg(type, dest, _start, _end)                                \
  fill_line_##type(0, dest, _start, _end)                                     \


// Render all layers as they appear in the layer order.

#define render_layers(tile_alpha, obj_alpha, dest)                            \
{                                                                             \
  current_layer = layer_order[0];                                             \
  if(current_layer & 0x04)                                                    \
  {                                                                           \
    /* If the first one is OBJ render the background then render it. */       \
    fill_line_bg(tile_alpha, dest, 0, 240);                                   \
    render_obj_layer(obj_alpha, dest, 0, 240);                                \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    /* Otherwise render a base layer. */                                      \
    layer_renderers[current_layer].tile_alpha##_render_base(current_layer,    \
     0, 240, dest);                                                           \
  }                                                                           \
                                                                              \
  /* Render the rest of the layers. */                                        \
  for(layer_order_pos = 1; layer_order_pos < layer_count; layer_order_pos++)  \
  {                                                                           \
    current_layer = layer_order[layer_order_pos];                             \
    if(current_layer & 0x04)                                                  \
    {                                                                         \
      render_obj_layer(obj_alpha, dest, 0, 240);                              \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      layer_renderers[current_layer].                                         \
       tile_alpha##_render_transparent(current_layer, 0, 240, dest);          \
    }                                                                         \
  }                                                                           \
}                                                                             \

#define render_condition_alpha                                                \
  (((io_registers[REG_BLDALPHA] & 0x1F1F) != 0x001F) &&                       \
   ((io_registers[REG_BLDCNT] & 0x3F) != 0) &&                                \
   ((io_registers[REG_BLDCNT] & 0x3F00) != 0))                                \

#define render_condition_fade                                                 \
  (((io_registers[REG_BLDY] & 0x1F) != 0) &&                                  \
   ((io_registers[REG_BLDCNT] & 0x3F) != 0))                                  \

#define render_layers_color_effect(renderer, layer_condition,                 \
 alpha_condition, fade_condition, _start, _end)                               \
{                                                                             \
  if(layer_condition)                                                         \
  {                                                                           \
    if(obj_alpha_count[io_registers[REG_VCOUNT]] > 0)                         \
    {                                                                         \
      /* Render based on special effects mode. */                             \
      u32 screen_buffer[240];                                                 \
      switch((bldcnt >> 6) & 0x03)                                            \
      {                                                                       \
        /* Alpha blend */                                                     \
        case 0x01:                                                            \
        {                                                                     \
          if(alpha_condition)                                                 \
          {                                                                   \
            renderer(alpha, alpha_obj, screen_buffer);                        \
            expand_blend(screen_buffer, scanline, _start, _end);              \
            return;                                                           \
          }                                                                   \
          break;                                                              \
        }                                                                     \
                                                                              \
        /* Fade to white */                                                   \
        case 0x02:                                                            \
        {                                                                     \
          if(fade_condition)                                                  \
          {                                                                   \
            renderer(color32, partial_alpha, screen_buffer);                  \
            expand_brighten_partial_alpha(screen_buffer, scanline,            \
             _start, _end);                                                   \
            return;                                                           \
          }                                                                   \
          break;                                                              \
        }                                                                     \
                                                                              \
        /* Fade to black */                                                   \
        case 0x03:                                                            \
        {                                                                     \
          if(fade_condition)                                                  \
          {                                                                   \
            renderer(color32, partial_alpha, screen_buffer);                  \
            expand_darken_partial_alpha(screen_buffer, scanline,              \
             _start, _end);                                                   \
            return;                                                           \
          }                                                                   \
          break;                                                              \
        }                                                                     \
      }                                                                       \
                                                                              \
      renderer(color32, partial_alpha, screen_buffer);                        \
      expand_blend(screen_buffer, scanline, _start, _end);                    \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      /* Render based on special effects mode. */                             \
      switch((bldcnt >> 6) & 0x03)                                            \
      {                                                                       \
        /* Alpha blend */                                                     \
        case 0x01:                                                            \
        {                                                                     \
          if(alpha_condition)                                                 \
          {                                                                   \
            u32 screen_buffer[240];                                           \
            renderer(alpha, alpha_obj, screen_buffer);                        \
            expand_blend(screen_buffer, scanline, _start, _end);              \
            return;                                                           \
          }                                                                   \
          break;                                                              \
        }                                                                     \
                                                                              \
        /* Fade to white */                                                   \
        case 0x02:                                                            \
        {                                                                     \
          if(fade_condition)                                                  \
          {                                                                   \
            renderer(color16, color16, scanline);                             \
            expand_brighten(scanline, scanline, _start, _end);                \
            return;                                                           \
          }                                                                   \
          break;                                                              \
        }                                                                     \
                                                                              \
        /* Fade to black */                                                   \
        case 0x03:                                                            \
        {                                                                     \
          if(fade_condition)                                                  \
          {                                                                   \
            renderer(color16, color16, scanline);                             \
            expand_darken(scanline, scanline, _start, _end);                  \
            return;                                                           \
          }                                                                   \
          break;                                                              \
        }                                                                     \
      }                                                                       \
                                                                              \
      renderer(normal, normal, scanline);                                     \
      expand_normal(scanline, _start, _end);                                  \
    }                                                                         \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    u32 pixel_top = palette_ram_converted[0];                                 \
    switch((bldcnt >> 6) & 0x03)                                              \
    {                                                                         \
      /* Fade to white */                                                     \
      case 0x02:                                                              \
      {                                                                       \
        if(color_combine_mask_a(5))                                           \
        {                                                                     \
          u32 blend = io_registers[REG_BLDY] & 0x1F;                          \
          u32 upper;                                                          \
                                                                              \
          if(blend > 16)                                                      \
            blend = 16;                                                       \
                                                                              \
          upper = ((0x07E0F81F * blend) >> 4) & 0x07E0F81F;                   \
          blend = 16 - blend;                                                 \
                                                                              \
          expand_pixel_no_dest(brighten, pixel_top);                          \
        }                                                                     \
        break;                                                                \
      }                                                                       \
                                                                              \
      /* Fade to black */                                                     \
      case 0x03:                                                              \
      {                                                                       \
        if(color_combine_mask_a(5))                                           \
        {                                                                     \
          s32 blend = 16 - (io_registers[REG_BLDY] & 0x1F);                   \
                                                                              \
          if(blend < 0)                                                       \
            blend = 0;                                                        \
                                                                              \
          expand_pixel_no_dest(darken, pixel_top);                            \
        }                                                                     \
        break;                                                                \
      }                                                                       \
    }                                                                         \
    fill_line_color16(pixel_top, scanline, _start, _end);                     \
  }                                                                           \
}                                                                             \


// Renders an entire scanline from 0 to 240, based on current color mode.

static void render_scanline_tile(u16 *scanline, u32 dispcnt)
{
  u32 current_layer;
  u32 layer_order_pos;
  u32 bldcnt = io_registers[REG_BLDCNT];
  render_scanline_layer_functions_tile();

  render_layers_color_effect(render_layers, layer_count,
   render_condition_alpha, render_condition_fade, 0, 240);
}

static void render_scanline_bitmap(u16 *scanline, u32 dispcnt)
{
  render_scanline_layer_functions_bitmap();
  u32 current_layer;
  u32 layer_order_pos;

  fill_line_bg(normal, scanline, 0, 240);

  for(layer_order_pos = 0; layer_order_pos < layer_count; layer_order_pos++)
  {
    current_layer = layer_order[layer_order_pos];
    if(current_layer & 0x04)
    {
      render_obj_layer(normal, scanline, 0, 240);
    }
    else
    {
      layer_renderers->normal_render(0, 240, scanline);
    }
  }
}

// Render layers from start to end based on if they're allowed in the
// enable flags.

#define render_layers_conditional(tile_alpha, obj_alpha, dest)                \
{                                                                             \
  __label__ skip;                                                             \
  current_layer = layer_order[layer_order_pos];                               \
  /* If OBJ aren't enabled skip to the first non-OBJ layer */                 \
  if(!(enable_flags & 0x10))                                                  \
  {                                                                           \
    while((current_layer & 0x04) || !((1 << current_layer) & enable_flags))   \
    {                                                                         \
      layer_order_pos++;                                                      \
      current_layer = layer_order[layer_order_pos];                           \
                                                                              \
      /* Oops, ran out of layers, render the background. */                   \
      if(layer_order_pos == layer_count)                                      \
      {                                                                       \
        fill_line_bg(tile_alpha, dest, start, end);                           \
        goto skip;                                                            \
      }                                                                       \
    }                                                                         \
                                                                              \
    /* Render the first valid layer */                                        \
    layer_renderers[current_layer].tile_alpha##_render_base(current_layer,    \
     start, end, dest);                                                       \
                                                                              \
    layer_order_pos++;                                                        \
                                                                              \
    /* Render the rest of the layers if active, skipping OBJ ones. */         \
    for(; layer_order_pos < layer_count; layer_order_pos++)                   \
    {                                                                         \
      current_layer = layer_order[layer_order_pos];                           \
      if(!(current_layer & 0x04) && ((1 << current_layer) & enable_flags))    \
      {                                                                       \
        layer_renderers[current_layer].                                       \
         tile_alpha##_render_transparent(current_layer, start, end, dest);    \
      }                                                                       \
    }                                                                         \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    /* Find the first active layer, skip all of the inactive ones */          \
    while(!((current_layer & 0x04) || ((1 << current_layer) & enable_flags))) \
    {                                                                         \
      layer_order_pos++;                                                      \
      current_layer = layer_order[layer_order_pos];                           \
                                                                              \
      /* Oops, ran out of layers, render the background. */                   \
      if(layer_order_pos == layer_count)                                      \
      {                                                                       \
        fill_line_bg(tile_alpha, dest, start, end);                           \
        goto skip;                                                            \
      }                                                                       \
    }                                                                         \
                                                                              \
    if(current_layer & 0x04)                                                  \
    {                                                                         \
      /* If the first one is OBJ render the background then render it. */     \
      fill_line_bg(tile_alpha, dest, start, end);                             \
      render_obj_layer(obj_alpha, dest, start, end);                          \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      /* Otherwise render a base layer. */                                    \
      layer_renderers[current_layer].                                         \
       tile_alpha##_render_base(current_layer, start, end, dest);             \
    }                                                                         \
                                                                              \
    layer_order_pos++;                                                        \
                                                                              \
    /* Render the rest of the layers. */                                      \
    for(; layer_order_pos < layer_count; layer_order_pos++)                   \
    {                                                                         \
      current_layer = layer_order[layer_order_pos];                           \
      if(current_layer & 0x04)                                                \
      {                                                                       \
        render_obj_layer(obj_alpha, dest, start, end);                        \
      }                                                                       \
      else                                                                    \
      {                                                                       \
        if(enable_flags & (1 << current_layer))                               \
        {                                                                     \
          layer_renderers[current_layer].                                     \
           tile_alpha##_render_transparent(current_layer, start, end, dest);  \
        }                                                                     \
      }                                                                       \
    }                                                                         \
  }                                                                           \
                                                                              \
  skip:                                                                       \
    ;                                                                         \
}                                                                             \


// Render all of the BG and OBJ in a tiled scanline from start to end ONLY if
// enable_flag allows that layer/OBJ. Also conditionally render color effects.

static void render_scanline_conditional_tile(u32 start, u32 end, u16 *scanline,
 u32 enable_flags, u32 dispcnt, u32 bldcnt, const tile_layer_render_struct
 *layer_renderers)
{
  u32 current_layer;
  u32 layer_order_pos = 0;

  render_layers_color_effect(render_layers_conditional,
   (layer_count && (enable_flags & 0x1F)),
   ((enable_flags & 0x20) && render_condition_alpha),
   ((enable_flags & 0x20) && render_condition_fade), start, end);
}


// Render the BG and OBJ in a bitmap scanline from start to end ONLY if
// enable_flag allows that layer/OBJ. Also conditionally render color effects.

static void render_scanline_conditional_bitmap(u32 start, u32 end, u16 *scanline,
 u32 enable_flags, u32 dispcnt, u32 bldcnt, const bitmap_layer_render_struct
 *layer_renderers)
{
  u32 current_layer;
  u32 layer_order_pos;

  fill_line_bg(normal, scanline, start, end);

  for(layer_order_pos = 0; layer_order_pos < layer_count; layer_order_pos++)
  {
    current_layer = layer_order[layer_order_pos];
    if(current_layer & 0x04)
    {
      if(enable_flags & 0x10)
      {
        render_obj_layer(normal, scanline, start, end);
      }
    }
    else
    {
      if(enable_flags & 0x04)
        layer_renderers->normal_render(start, end, scanline);
    }
  }
}


#define window_x_coords(window_number)                                        \
  window_##window_number##_x1 =                                               \
   io_registers[REG_WIN##window_number##H] >> 8;                              \
  window_##window_number##_x2 =                                               \
   io_registers[REG_WIN##window_number##H] & 0xFF;                            \
  window_##window_number##_enable =                                           \
   (winin >> (window_number * 8)) & 0x3F;                                     \
                                                                              \
  if(window_##window_number##_x1 > 240)                                       \
    window_##window_number##_x1 = 240;                                        \
                                                                              \
  if(window_##window_number##_x2 > 240)                                       \
    window_##window_number##_x2 = 240                                         \

#define window_coords(window_number)                                          \
  u32 window_##window_number##_x1, window_##window_number##_x2;               \
  u32 window_##window_number##_y1, window_##window_number##_y2;               \
  u32 window_##window_number##_enable = 0;                                    \
  window_##window_number##_y1 =                                               \
   io_registers[REG_WIN##window_number##V] >> 8;                              \
  window_##window_number##_y2 =                                               \
   io_registers[REG_WIN##window_number##V] & 0xFF;                            \
                                                                              \
  if(window_##window_number##_y1 > window_##window_number##_y2)               \
  {                                                                           \
    if((((vcount <= window_##window_number##_y2) ||                           \
     (vcount > window_##window_number##_y1)) ||                               \
     (window_##window_number##_y2 > 227)) &&                                  \
     (window_##window_number##_y1 <= 227))                                    \
    {                                                                         \
      window_x_coords(window_number);                                         \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      window_##window_number##_x1 = 240;                                      \
      window_##window_number##_x2 = 240;                                      \
    }                                                                         \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    if((((vcount >= window_##window_number##_y1) &&                           \
     (vcount < window_##window_number##_y2)) ||                               \
     (window_##window_number##_y2 > 227)) &&                                  \
     (window_##window_number##_y1 <= 227))                                    \
    {                                                                         \
      window_x_coords(window_number);                                         \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      window_##window_number##_x1 = 240;                                      \
      window_##window_number##_x2 = 240;                                      \
    }                                                                         \
  }                                                                           \

#define render_window_segment(type, start, end, window_type)                  \
  if(start != end)                                                            \
  {                                                                           \
    render_scanline_conditional_##type(start, end, scanline,                  \
     window_##window_type##_enable, dispcnt, bldcnt, layer_renderers);        \
  }                                                                           \

#define render_window_segment_unequal(type, start, end, window_type)          \
  render_scanline_conditional_##type(start, end, scanline,                    \
   window_##window_type##_enable, dispcnt, bldcnt, layer_renderers)           \

#define render_window_segment_clip(type, clip_start, clip_end, start, end,    \
 window_type)                                                                 \
{                                                                             \
  if(start != end)                                                            \
  {                                                                           \
    if(start < clip_start)                                                    \
    {                                                                         \
      if(end > clip_start)                                                    \
      {                                                                       \
        if(end > clip_end)                                                    \
        {                                                                     \
          render_window_segment_unequal(type, clip_start, clip_end,           \
           window_type);                                                      \
        }                                                                     \
        else                                                                  \
        {                                                                     \
          render_window_segment_unequal(type, clip_start, end, window_type);  \
        }                                                                     \
      }                                                                       \
    }                                                                         \
    else                                                                      \
                                                                              \
    if(end > clip_end)                                                        \
    {                                                                         \
      if(start < clip_end)                                                    \
        render_window_segment_unequal(type, start, clip_end, window_type);    \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      render_window_segment_unequal(type, start, end, window_type);           \
    }                                                                         \
  }                                                                           \
}                                                                             \

#define render_window_clip_1(type, start, end)                                \
  if(window_1_x1 != 240)                                                      \
  {                                                                           \
    if(window_1_x1 > window_1_x2)                                             \
    {                                                                         \
      render_window_segment_clip(type, start, end, 0, window_1_x2, 1);        \
      render_window_segment_clip(type, start, end, window_1_x2, window_1_x1,  \
       out);                                                                  \
      render_window_segment_clip(type, start, end, window_1_x1, 240, 1);      \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      render_window_segment_clip(type, start, end, 0, window_1_x1, out);      \
      render_window_segment_clip(type, start, end, window_1_x1, window_1_x2,  \
       1);                                                                    \
      render_window_segment_clip(type, start, end, window_1_x2, 240, out);    \
    }                                                                         \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    render_window_segment(type, start, end, out);                             \
  }                                                                           \

#define render_window_clip_obj(type, start, end);                             \
  render_window_segment(type, start, end, out);                               \
  if(dispcnt & 0x40)                                                          \
    render_scanline_obj_copy_##type##_1D(4, start, end, scanline);            \
  else                                                                        \
    render_scanline_obj_copy_##type##_2D(4, start, end, scanline)             \


#define render_window_segment_clip_obj(type, clip_start, clip_end, start,     \
 end)                                                                         \
{                                                                             \
  if(start != end)                                                            \
  {                                                                           \
    if(start < clip_start)                                                    \
    {                                                                         \
      if(end > clip_start)                                                    \
      {                                                                       \
        if(end > clip_end)                                                    \
        {                                                                     \
          render_window_clip_obj(type, clip_start, clip_end);                 \
        }                                                                     \
        else                                                                  \
        {                                                                     \
          render_window_clip_obj(type, clip_start, end);                      \
        }                                                                     \
      }                                                                       \
    }                                                                         \
    else                                                                      \
                                                                              \
    if(end > clip_end)                                                        \
    {                                                                         \
      if(start < clip_end)                                                    \
      {                                                                       \
        render_window_clip_obj(type, start, clip_end);                        \
      }                                                                       \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      render_window_clip_obj(type, start, end);                               \
    }                                                                         \
  }                                                                           \
}                                                                             \


#define render_window_clip_1_obj(type, start, end)                            \
  if(window_1_x1 != 240)                                                      \
  {                                                                           \
    if(window_1_x1 > window_1_x2)                                             \
    {                                                                         \
      render_window_segment_clip(type, start, end, 0, window_1_x2, 1);        \
      render_window_segment_clip_obj(type, start, end, window_1_x2,           \
       window_1_x1);                                                          \
      render_window_segment_clip(type, start, end, window_1_x1, 240, 1);      \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      render_window_segment_clip_obj(type, start, end, 0, window_1_x1);       \
      render_window_segment_clip(type, start, end, window_1_x1, window_1_x2,  \
       1);                                                                    \
      render_window_segment_clip_obj(type, start, end, window_1_x2, 240);     \
    }                                                                         \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    render_window_clip_obj(type, start, end);                                 \
  }                                                                           \



#define render_window_single(type, window_number)                             \
  u32 winin = io_registers[REG_WININ];                                        \
  window_coords(window_number);                                               \
  if(window_##window_number##_x1 > window_##window_number##_x2)               \
  {                                                                           \
    render_window_segment(type, 0, window_##window_number##_x2,               \
     window_number);                                                          \
    render_window_segment(type, window_##window_number##_x2,                  \
     window_##window_number##_x1, out);                                       \
    render_window_segment(type, window_##window_number##_x1, 240,             \
     window_number);                                                          \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    render_window_segment(type, 0, window_##window_number##_x1, out);         \
    render_window_segment(type, window_##window_number##_x1,                  \
     window_##window_number##_x2, window_number);                             \
    render_window_segment(type, window_##window_number##_x2, 240, out);       \
  }                                                                           \

#define render_window_multi(type, front, back)                                \
  if(window_##front##_x1 > window_##front##_x2)                               \
  {                                                                           \
    render_window_segment(type, 0, window_##front##_x2, front);               \
    render_window_clip_##back(type, window_##front##_x2,                      \
     window_##front##_x1);                                                    \
    render_window_segment(type, window_##front##_x1, 240, front);             \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    render_window_clip_##back(type, 0, window_##front##_x1);                  \
    render_window_segment(type, window_##front##_x1, window_##front##_x2,     \
     front);                                                                  \
    render_window_clip_##back(type, window_##front##_x2, 240);                \
  }                                                                           \

#define render_scanline_window_builder(type)                                  \
static void render_scanline_window_##type(u16 *scanline, u32 dispcnt)         \
{                                                                             \
  u32 vcount = io_registers[REG_VCOUNT];                                      \
  u32 winout = io_registers[REG_WINOUT];                                      \
  u32 bldcnt = io_registers[REG_BLDCNT];                                      \
  u32 window_out_enable = winout & 0x3F;                                      \
                                                                              \
  render_scanline_layer_functions_##type();                                   \
                                                                              \
  switch(dispcnt >> 13)                                                       \
  {                                                                           \
    /* Just window 0 */                                                       \
    case 0x01:                                                                \
    {                                                                         \
      render_window_single(type, 0);                                          \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* Just window 1 */                                                       \
    case 0x02:                                                                \
    {                                                                         \
      render_window_single(type, 1);                                          \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* Windows 1 and 2 */                                                     \
    case 0x03:                                                                \
    {                                                                         \
      u32 winin = io_registers[REG_WININ];                                    \
      window_coords(0);                                                       \
      window_coords(1);                                                       \
      render_window_multi(type, 0, 1);                                        \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* Just OBJ windows */                                                    \
    case 0x04:                                                                \
    {                                                                         \
      render_window_clip_obj(type, 0, 240);                                   \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* Window 0 and OBJ window */                                             \
    case 0x05:                                                                \
    {                                                                         \
      u32 winin = io_registers[REG_WININ];                                    \
      window_coords(0);                                                       \
      render_window_multi(type, 0, obj);                                      \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* Window 1 and OBJ window */                                             \
    case 0x06:                                                                \
    {                                                                         \
      u32 winin = io_registers[REG_WININ];                                    \
      window_coords(1);                                                       \
      render_window_multi(type, 1, obj);                                      \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* Window 0, 1, and OBJ window */                                         \
    case 0x07:                                                                \
    {                                                                         \
      u32 winin = io_registers[REG_WININ];                                    \
      window_coords(0);                                                       \
      window_coords(1);                                                       \
      render_window_multi(type, 0, 1_obj);                                    \
      break;                                                                  \
    }                                                                         \
  }                                                                           \
}                                                                             \

render_scanline_window_builder(tile);
render_scanline_window_builder(bitmap);

static const u32 active_layers[6] = { 0x1F, 0x17, 0x1C, 0x14, 0x14, 0x14 };

u32 small_resolution_width = 240;
u32 small_resolution_height = 160;
u32 resolution_width, resolution_height;

void update_scanline()
{
  u32 pitch = get_screen_pitch();
  u32 dispcnt = io_registers[REG_DISPCNT];
  u32 vcount = io_registers[REG_VCOUNT];
  u16 *screen_offset = get_screen_pixels() + (vcount * pitch);
  u32 video_mode = dispcnt & 0x07;

  // If OAM has been modified since the last scanline has been updated then
  // reorder and reprofile the OBJ lists.
  if(oam_update)
  {
    order_obj(video_mode);
    oam_update = 0;
  }

  order_layers((dispcnt >> 8) & active_layers[video_mode]);

  if(skip_next_frame)
    return;

#ifdef WIZ_BUILD
  if (screen_scale == unscaled_rot || screen_scale == scaled_aspect_rot)
  {
    if (rot_line_count == rot_lines_total)
    {
      rot_line_count = 0;
      if (vcount - rot_lines_total < FONT_HEIGHT && rot_msg_buff[0])
      {
        print_string_ext(rot_msg_buff, 0xFFFF, 0x0000, 0, 0,
          rot_buffer, 240, 0, vcount - rot_lines_total, rot_lines_total);
        if (vcount >= FONT_HEIGHT)
          rot_msg_buff[0] = 0;
      }
      if (screen_scale == unscaled_rot)
        do_rotated_blit(gpsp_gp2x_screen, rot_buffer, vcount);
      else
        upscale_aspect_row(gpsp_gp2x_screen, rot_buffer, vcount/3);
    }
    screen_offset = &rot_buffer[rot_line_count++ * 240];
  }
#endif

  // If the screen is in in forced blank draw pure white.
  if(dispcnt & 0x80)
  {
    fill_line_color16(0xFFFF, screen_offset, 0, 240);
  }
  else
  {
    if(video_mode < 3)
    {
      if(dispcnt >> 13)
      {
        render_scanline_window_tile(screen_offset, dispcnt);
      }
      else
      {
        render_scanline_tile(screen_offset, dispcnt);
      }
    }
    else
    {
      if(dispcnt >> 13)
        render_scanline_window_bitmap(screen_offset, dispcnt);
      else
        render_scanline_bitmap(screen_offset, dispcnt);
    }
  }

  affine_reference_x[0] += (s16)io_registers[REG_BG2PB];
  affine_reference_y[0] += (s16)io_registers[REG_BG2PD];
  affine_reference_x[1] += (s16)io_registers[REG_BG3PB];
  affine_reference_y[1] += (s16)io_registers[REG_BG3PD];
}



/// Nearest neighboor optimized with possible out of screen coordinates (for cropping)
void flip_NNOptimized_AllowOutOfScreen(SDL_Surface *virtual_screen, SDL_Surface *hardware_screen, int new_w, int new_h){
  int w1=virtual_screen->w;
  //int h1=virtual_screen->h;
  int w2=new_w;
  int h2=new_h;
  int x_ratio = (int)((virtual_screen->w<<16)/w2);
  int y_ratio = (int)((virtual_screen->h<<16)/h2);
  int x2, y2 ;

  /// --- Compute padding for centering when out of bounds ---
  int y_padding = (RES_HW_SCREEN_VERTICAL-new_h)/2;
  int x_padding = 0;
  if(w2>RES_HW_SCREEN_HORIZONTAL){
    x_padding = (w2-RES_HW_SCREEN_HORIZONTAL)/2 + 1;
  }
  int x_padding_ratio = x_padding*w1/w2;
  //printf("virtual_screen->h=%d, h2=%d\n", virtual_screen->h, h2);

  for (int i=0;i<h2;i++)
  {
    if(i>=RES_HW_SCREEN_VERTICAL){
      continue;
    }

    uint16_t* t = (uint16_t*)(hardware_screen->pixels+((i+y_padding)* ((w2>RES_HW_SCREEN_HORIZONTAL)?RES_HW_SCREEN_HORIZONTAL:w2) )*sizeof(uint16_t));
    y2 = ((i*y_ratio)>>16);
    uint16_t* p = (uint16_t*)(virtual_screen->pixels + (y2*w1 + x_padding_ratio) *sizeof(uint16_t));
    int rat = 0;
    for (int j=0;j<w2;j++)
    {
      if(j>=RES_HW_SCREEN_HORIZONTAL){
        continue;
      }
      x2 = (rat>>16);
      *t++ = p[x2];
      rat += x_ratio;
      //printf("y=%d, x=%d, y2=%d, x2=%d, (y2*virtual_screen->w)+x2=%d\n", i, j, y2, x2, (y2*virtual_screen->w)+x2);
    }
  }
}

/// Nearest neighboor optimized with possible out of screen coordinates (for cropping)
void flip_Upscaling_Bilinear(SDL_Surface *virtual_screen, SDL_Surface *hardware_screen, int new_w, int new_h){
  int w1=virtual_screen->w;
  int h1=virtual_screen->h;
  int w2=new_w;
  int h2=new_h;
  int x_ratio = (int)((virtual_screen->w<<16)/w2);
  int y_ratio = (int)((virtual_screen->h<<16)/h2);
  uint32_t x_diff, y_diff;
  uint16_t red_comp, green_comp, blue_comp;
  uint32_t p_val_tl, p_val_tr, p_val_bl, p_val_br;
  int x, y ;
  //printf("virtual_screen->h=%d, h2=%d\n", virtual_screen->h, h2);

  uint16_t green_mask = 0x07E0;

  /// --- Compute padding for centering when out of bounds ---
  int y_padding = (RES_HW_SCREEN_VERTICAL-new_h)/2;
  int x_padding = 0;
  if(w2>RES_HW_SCREEN_HORIZONTAL){
    x_padding = (w2-RES_HW_SCREEN_HORIZONTAL)/2 + 1;
  }
  int x_padding_ratio = x_padding*w1/w2;

  for (int i=0;i<h2;i++)
  {
    if(i>=RES_HW_SCREEN_VERTICAL){
      continue;
    }

    uint16_t* t = (uint16_t*)(hardware_screen->pixels+((i+y_padding)*
              ((w2>RES_HW_SCREEN_HORIZONTAL)?RES_HW_SCREEN_HORIZONTAL:w2) )*sizeof(uint16_t));
    y = ((i*y_ratio)>>16);
    y_diff = (i*y_ratio) - (y<<16) ;
    uint16_t* p = (uint16_t*)(virtual_screen->pixels + (y*w1 + x_padding_ratio) *sizeof(uint16_t));
    int rat = 0;
    for (int j=0;j<w2;j++)
    {
      if(j>=RES_HW_SCREEN_HORIZONTAL){
        continue;
      }
      x = (rat>>16);
      x_diff = rat - (x<<16) ;

      /// --- Getting adjacent pixels ---
      p_val_tl = p[x] ;
      p_val_tr = (x+1<w1)?p[x+1]:p[x];
      p_val_bl = (y+1<h1)?p[x+w1]:p[x];
      p_val_br = (y+1<h1 && x+1<w1)?p[x+w1+1]:p[x];

      // red element
      // Yr = Ar(1-w)(1-h) + Br(w)(1-h) + Cr(h)(1-w) + Dr(wh)
      red_comp = (( ((p_val_tl&0xF800)>>11) * ((((1<<16)-x_diff) * ((1<<16)-y_diff))>>5) )>>27) +
          (( ((p_val_tr&0xF800)>>11) * ((x_diff * ((1<<16)-y_diff))>>5) )>>27) +
            (( ((p_val_bl&0xF800)>>11) * ((y_diff * ((1<<16)-x_diff))>>5) )>>27) +
            (( ((p_val_br&0xF800)>>11) * ((y_diff * x_diff)>>5) )>>27);

      // green element
      // Yg = Ag(1-w)(1-h) + Bg(w)(1-h) + Cg(h)(1-w) + Dg(wh)
      green_comp = (( ((p_val_tl&0x07E0)>>5) * ((((1<<16)-x_diff) * ((1<<16)-y_diff))>>6) )>>26) +
          (( ((p_val_tr&0x07E0)>>5) * ((x_diff * ((1<<16)-y_diff))>>6) )>>26) +
            (( ((p_val_bl&0x07E0)>>5) * ((y_diff * ((1<<16)-x_diff))>>6) )>>26) +
            (( ((p_val_br&0x07E0)>>5) * ((y_diff * x_diff)>>6) )>>26);

      // blue element
      // Yb = Ab(1-w)(1-h) + Bb(w)(1-h) + Cb(h)(1-w) + Db(wh)
      blue_comp = (( ((p_val_tl&0x001F)) * ((((1<<16)-x_diff) * ((1<<16)-y_diff))>>5) )>>27) +
          (( ((p_val_tr&0x001F)) * ((x_diff * ((1<<16)-y_diff))>>5) )>>27) +
            (( ((p_val_bl&0x001F)) * ((y_diff * ((1<<16)-x_diff))>>5) )>>27) +
            (( ((p_val_br&0x001F)) * ((y_diff * x_diff)>>5) )>>27);

      /// --- Write pixel value ---
      *t++ = ((red_comp<<11)&0xF800) + ((green_comp<<5)&0x07E0) + ((blue_comp)&0x001F);

      /// --- Update x ----
      rat += x_ratio;
    }
  }
}

/// Nearest neighboor optimized with possible out of screen coordinates (for cropping)
void flip_Upscaling_Bilinear_Xonly(SDL_Surface *virtual_screen, SDL_Surface *hardware_screen, int new_w, int new_h){
  int w1=virtual_screen->w;
  int h1=virtual_screen->h;
  int w2=new_w;
  int h2=new_h;
  int x_ratio = (int)((virtual_screen->w<<16)/w2);
  int y_ratio = (int)((virtual_screen->h<<16)/h2);
  uint32_t x_diff, y_diff;
  uint16_t red_comp, green_comp, blue_comp;
  uint32_t p_val_tl, p_val_tr;
  int x, y ;
  //printf("virtual_screen->h=%d, h2=%d\n", virtual_screen->h, h2);

  uint16_t green_mask = 0x07E0;

  /// --- Compute padding for centering when out of bounds ---
  int y_padding = (RES_HW_SCREEN_VERTICAL-new_h)/2;
  int x_padding = 0;
  if(w2>RES_HW_SCREEN_HORIZONTAL){
    x_padding = (w2-RES_HW_SCREEN_HORIZONTAL)/2 + 1;
  }
  int x_padding_ratio = x_padding*w1/w2;

  for (int i=0;i<h2;i++)
  {
    if(i>=RES_HW_SCREEN_VERTICAL){
      continue;
    }

    uint16_t* t = (uint16_t*)(hardware_screen->pixels+((i+y_padding)*
              ((w2>RES_HW_SCREEN_HORIZONTAL)?RES_HW_SCREEN_HORIZONTAL:w2) )*sizeof(uint16_t));
    y = ((i*y_ratio)>>16);
    y_diff = (i*y_ratio) - (y<<16) ;
    uint16_t* p = (uint16_t*)(virtual_screen->pixels + (y*w1 + x_padding_ratio) *sizeof(uint16_t));
    int rat = 0;
    for (int j=0;j<w2;j++)
    {
      if(j>=RES_HW_SCREEN_HORIZONTAL){
        continue;
      }
      x = (rat>>16);
      x_diff = rat - (x<<16) ;

      /// --- Getting adjacent pixels ---
      p_val_tl = p[x] ;
      p_val_tr = (x+1<w1)?p[x+1]:p[x];

      // red element
      // Yr = Ar(1-w)(1-h) + Br(w)(1-h) + Cr(h)(1-w) + Dr(wh)
      red_comp = (( (p_val_tl&0xF800) * ((1<<16)-x_diff) )>>16) +
          (( (p_val_tr&0xF800) * x_diff )>>16);

      // green element
      // Yg = Ag(1-w)(1-h) + Bg(w)(1-h) + Cg(h)(1-w) + Dg(wh)
      green_comp = (( (p_val_tl&0x07E0) * ((1<<16)-x_diff) )>>16) +
          (( (p_val_tr&0x07E0) * x_diff )>>16);

      // blue element
      // Yb = Ab(1-w)(1-h) + Bb(w)(1-h) + Cb(h)(1-w) + Db(wh)
      blue_comp = (( (p_val_tl&0x001F) * ((1<<16)-x_diff) )>>16) +
          (( (p_val_tr&0x001F) * x_diff )>>16);

      /// --- Write pixel value ---
      *t++ = (red_comp&0xF800) + (green_comp&0x07E0) + (blue_comp&0x001F);

      /// --- Update x ----
      rat += x_ratio;
    }
  }
}

/// Nearest neighboor optimized with possible out of screen coordinates (for cropping)
void flip_Upscaling_Bilinear_Yonly(SDL_Surface *virtual_screen, SDL_Surface *hardware_screen, int new_w, int new_h){
  int w1=virtual_screen->w;
  int h1=virtual_screen->h;
  int w2=new_w;
  int h2=new_h;
  int x_ratio = (int)((virtual_screen->w<<16)/w2);
  int y_ratio = (int)((virtual_screen->h<<16)/h2);
  uint32_t x_diff, y_diff;
  uint16_t red_comp, green_comp, blue_comp;
  uint32_t p_val_tl, p_val_bl;
  int x, y ;
  //printf("virtual_screen->h=%d, h2=%d\n", virtual_screen->h, h2);

  uint16_t green_mask = 0x07E0;

  /// --- Compute padding for centering when out of bounds ---
  int y_padding = (RES_HW_SCREEN_VERTICAL-new_h)/2;
  int x_padding = 0;
  if(w2>RES_HW_SCREEN_HORIZONTAL){
    x_padding = (w2-RES_HW_SCREEN_HORIZONTAL)/2 + 1;
  }
  int x_padding_ratio = x_padding*w1/w2;

  for (int i=0;i<h2;i++)
  {
    if(i>=RES_HW_SCREEN_VERTICAL){
      continue;
    }

    uint16_t* t = (uint16_t*)(hardware_screen->pixels+((i+y_padding)*
              ((w2>RES_HW_SCREEN_HORIZONTAL)?RES_HW_SCREEN_HORIZONTAL:w2) )*sizeof(uint16_t));
    y = ((i*y_ratio)>>16);
    y_diff = (i*y_ratio) - (y<<16) ;
    uint16_t* p = (uint16_t*)(virtual_screen->pixels + (y*w1 + x_padding_ratio) *sizeof(uint16_t));
    int rat = 0;
    for (int j=0;j<w2;j++)
    {
      if(j>=RES_HW_SCREEN_HORIZONTAL){
        continue;
      }
      x = (rat>>16);
      x_diff = rat - (x<<16) ;

      /// --- Getting adjacent pixels ---
      p_val_tl = p[x] ;
      p_val_bl = (y+1<h1)?p[x+w1]:p[x];

      // red element
      // Yr = Ar(1-w)(1-h) + Br(w)(1-h) + Cr(h)(1-w) + Dr(wh)
      red_comp = (( (p_val_tl&0xF800) * ((1<<16)-y_diff) )>>16) +
          (( (p_val_bl&0xF800) * y_diff )>>16);

      // green element
      // Yg = Ag(1-w)(1-h) + Bg(w)(1-h) + Cg(h)(1-w) + Dg(wh)
      green_comp = (( (p_val_tl&0x07E0) * ((1<<16)-y_diff) )>>16) +
          (( (p_val_bl&0x07E0) * y_diff )>>16);

      // blue element
      // Yb = Ab(1-w)(1-h) + Bb(w)(1-h) + Cb(h)(1-w) + Db(wh)
      blue_comp = (( (p_val_tl&0x001F) * ((1<<16)-y_diff) )>>16) +
          (( (p_val_bl&0x001F) * y_diff )>>16);

      /// --- Write pixel value ---
      *t++ = (red_comp&0xF800) + (green_comp&0x07E0) + (blue_comp&0x001F);

      /// --- Update x ----
      rat += x_ratio;
    }
  }
}


/// Nearest neighbor with 2D bilinear and interpolation with left and right pixels, pseudo gaussian weighting
void flip_NNOptimized_LeftAndRightBilinear(SDL_Surface *virtual_screen, SDL_Surface *hardware_screen, int new_w, int new_h){
  int w1=virtual_screen->w;
  int h1=virtual_screen->h;
  int w2=new_w;
  int h2=new_h;
  int y_padding = (RES_HW_SCREEN_VERTICAL-new_h)/2;
  //int x_ratio = (int)((w1<<16)/w2) +1;
  //int y_ratio = (int)((h1<<16)/h2) +1;
  int x_ratio = (int)((w1<<16)/w2);
  int y_ratio = (int)((h1<<16)/h2);
  int x1, y1;

  uint16_t green_mask = 0x07E0;

  /// --- Compute padding for centering when out of bounds ---
  int x_padding = 0;
  if(w2>RES_HW_SCREEN_HORIZONTAL){
    x_padding = (w2-RES_HW_SCREEN_HORIZONTAL)/2 + 1;
  }
  int x_padding_ratio = x_padding*w1/w2;

  /// --- Interp params ---
  int px_diff_prev_x = 0;
  int px_diff_next_x = 0;
  uint32_t ponderation_factor;
  uint16_t * cur_p;
  uint16_t * cur_p_left;
  uint16_t * cur_p_right;
  uint32_t red_comp, green_comp, blue_comp;
  //int cnt_interp = 0; int cnt_no_interp = 0;
  //printf("virtual_screen->w=%d, virtual_screen->w=%d\n", virtual_screen->w, virtual_screen->h);

  for (int i=0;i<h2;i++)
  {
    if(i>=RES_HW_SCREEN_VERTICAL){
      continue;
    }
    uint16_t* t = (uint16_t*)(hardware_screen->pixels+( (i+y_padding)*((w2>RES_HW_SCREEN_HORIZONTAL)?RES_HW_SCREEN_HORIZONTAL:w2))*sizeof(uint16_t));
    y1 = ((i*y_ratio)>>16);
    uint16_t* p = (uint16_t*)(virtual_screen->pixels + (y1*w1 + x_padding_ratio) *sizeof(uint16_t));
    int rat = 0;
    for (int j=0;j<w2;j++)
    {
      if(j>=RES_HW_SCREEN_HORIZONTAL){
        continue;
      }
      // ------ current x value ------
      x1 = (rat>>16);
      px_diff_next_x = ((rat+x_ratio)>>16) - x1;

      // ------ adapted bilinear with 3x3 gaussian blur -------
      cur_p = p+x1;
      if(px_diff_prev_x > 1 || px_diff_next_x > 1){
        red_comp=((*cur_p)&0xF800) << 1;
        green_comp=((*cur_p)&0x07E0) << 1;
        blue_comp=((*cur_p)&0x001F) << 1;
        ponderation_factor = 2;

        // ---- Interpolate current and left ----
        if(px_diff_prev_x > 1 && x1>0){
          cur_p_left = p+x1-1;

          red_comp += ((*cur_p_left)&0xF800);
          green_comp += ((*cur_p_left)&0x07E0);
          blue_comp += ((*cur_p_left)&0x001F);
          ponderation_factor++;
        }

        // ---- Interpolate current and right ----
        if(px_diff_next_x > 1 && x1+1<w1){
          cur_p_right = p+x1+1;

          red_comp += ((*cur_p_right)&0xF800);
          green_comp += ((*cur_p_right)&0x07E0);
          blue_comp += ((*cur_p_right)&0x001F);
          ponderation_factor++;
        }

        /// --- Compute new px value ---
        if(ponderation_factor==4){
          red_comp = (red_comp >> 2)&0xF800;
          green_comp = (green_comp >> 2)&green_mask;
          blue_comp = (blue_comp >> 2)&0x001F;
        }
        else if(ponderation_factor==2){
          red_comp = (red_comp >> 1)&0xF800;
          green_comp = (green_comp >> 1)&green_mask;
          blue_comp = (blue_comp >> 1)&0x001F;
        }
        else{
          red_comp = (red_comp / ponderation_factor )&0xF800;
          green_comp = (green_comp / ponderation_factor )&green_mask;
          blue_comp = (blue_comp / ponderation_factor )&0x001F;
        }

        /// --- write pixel ---
        *t++ = red_comp+green_comp+blue_comp;
      }
      else{
        /// --- copy pixel ---
        *t++ = (*cur_p);
      }

      /// save number of pixels to interpolate
      px_diff_prev_x = px_diff_next_x;

      // ------ next pixel ------
      rat += x_ratio;
    }
  }
  //printf("cnt_interp = %d, int cnt_no_interp = %d\n", cnt_interp, cnt_no_interp);
}


void upscale_160x240_to_240x240_bilinearish(SDL_Surface *src_surface, SDL_Surface *dst_surface)
{
  if(src_surface->w != 240){
    printf("src_surface->w (%d) != 240 \n", src_surface->w);
    return;
  }
  if(src_surface->h != 160){
    printf("src_surface->h (%d) != 160 \n", src_surface->h);
    return;
  }

  uint16_t* Src16 = (uint16_t*) src_surface->pixels;
  uint16_t* Dst16 = (uint16_t*) dst_surface->pixels;
  // There are 80 blocks of 2 pixels vertically, and 240 of 1 horizontally.
  // Each block of 2x1 becomes 3x1.
  uint32_t BlockX, BlockY;
  uint16_t* BlockSrc;
  uint16_t* BlockDst;
  for (BlockY = 0; BlockY < 80; BlockY++)
  {
    BlockSrc = Src16 + BlockY * 240 * 2;
    BlockDst = Dst16 + BlockY * 240 * 3;
    for (BlockX = 0; BlockX < 240; BlockX++)
    {
      /* Vertically:
       * Before(2):
       * (a)(b)
       * After(3):
       * (a)(ab)(b)
       */

      // -- Column 1 --
      uint16_t  _1 = *(BlockSrc               );
      *(BlockDst               ) = _1;
      uint16_t  _2 = *(BlockSrc            + 240*1);
      *(BlockDst            + 240*1) = Weight1_1( _1,  _2);
      *(BlockDst            + 240*2) = _2;

      BlockSrc += 1;
      BlockDst += 1;
    }
  }
}



void SDL_Rotate_270(SDL_Surface * hw_surface, SDL_Surface * virtual_hw_surface){
  int i, j;
    uint16_t *source_pixels = (uint16_t*) virtual_hw_surface->pixels;
    uint16_t *dest_pixels = (uint16_t*) hw_surface->pixels;

    /// --- Checking for right pixel format ---
    //MENU_DEBUG_PRINTF("Source bpb = %d, Dest bpb = %d\n", virtual_hw_surface->format->BitsPerPixel, hw_surface->format->BitsPerPixel);
    if(virtual_hw_surface->format->BitsPerPixel != 16){
      printf("Error in SDL_FastBlit, Wrong virtual_hw_surface pixel format: %d bpb, expected: 16 bpb\n", virtual_hw_surface->format->BitsPerPixel);
      return;
    }
    if(hw_surface->format->BitsPerPixel != 16){
      printf("Error in SDL_FastBlit, Wrong hw_surface pixel format: %d bpb, expected: 16 bpb\n", hw_surface->format->BitsPerPixel);
      return;
    }

    /// --- Checking if same dimensions ---
    if(hw_surface->w != virtual_hw_surface->w || hw_surface->h != virtual_hw_surface->h){
      printf("Error in SDL_FastBlit, hw_surface (%dx%d) and virtual_hw_surface (%dx%d) have different dimensions\n",
        hw_surface->w, hw_surface->h, virtual_hw_surface->w, virtual_hw_surface->h);
      return;
    }

  /// --- Pixel copy and rotation (270) ---
  uint16_t *cur_p_src, *cur_p_dst;
  for(i=0; i<virtual_hw_surface->h; i++){
    for(j=0; j<virtual_hw_surface->w; j++){
      cur_p_src = source_pixels + i*virtual_hw_surface->w + j;
      cur_p_dst = dest_pixels + (hw_surface->h-1-j)*hw_surface->w + i;
      *cur_p_dst = *cur_p_src;
    }
  }
}

void clear_hw_screen(uint16_t color)
{
  /*if(virtual_hw_screen){
    uint16_t *dest_ptr = (uint16_t *)virtual_hw_screen->pixels;
    uint32_t x, y;

    for(y = 0; y < virtual_hw_screen->h; y++)
    {
      for(x = 0; x < virtual_hw_screen->w; x++, dest_ptr++)
      {
        *dest_ptr = color;
      }
    }
  }*/

  memset(hw_screen->pixels, 0, hw_screen->w*hw_screen->h*sizeof(uint16_t));
}

void render_game()
{

  /// --------------Optimized Flip depending on aspect ratio -------------
  static int prev_aspect_ratio;
  if(prev_aspect_ratio != aspect_ratio || need_screen_cleared){
    clear_hw_screen(0);
    prev_aspect_ratio = aspect_ratio;
    need_screen_cleared = 0;
  }
  memcpy(hw_screen->pixels + (40 * hw_screen->pitch), screen->pixels, 159 * hw_screen->pitch + 240 * hw_screen->format->BytesPerPixel);
  #if (0)
  switch(aspect_ratio){
    case ASPECT_RATIOS_TYPE_STRETCHED:
    if(screen->w > RES_HW_SCREEN_HORIZONTAL){
      flip_NNOptimized_LeftAndRightBilinear(screen, hw_screen, RES_HW_SCREEN_HORIZONTAL, RES_HW_SCREEN_VERTICAL);
    }
    else{
        upscale_160x240_to_240x240_bilinearish(screen, hw_screen);
    }
    break;

    case ASPECT_RATIOS_TYPE_MANUAL:
    ;uint32_t h_scaled = MIN(screen->h*RES_HW_SCREEN_HORIZONTAL/screen->w,
                              RES_HW_SCREEN_VERTICAL);
    uint32_t h_zoomed = MIN(h_scaled + aspect_ratio_factor_percent*(RES_HW_SCREEN_VERTICAL - h_scaled)/100,
                              RES_HW_SCREEN_VERTICAL);
    if(aspect_ratio_factor_percent == 0){
      flip_NNOptimized_AllowOutOfScreen(screen, hw_screen,
        RES_HW_SCREEN_HORIZONTAL,
        MIN(screen->h*RES_HW_SCREEN_HORIZONTAL/screen->w, RES_HW_SCREEN_VERTICAL));
    }
    else if(screen->w > RES_HW_SCREEN_HORIZONTAL){
      flip_NNOptimized_LeftAndRightBilinear(screen, hw_screen,
          MAX(screen->w*h_zoomed/screen->h, RES_HW_SCREEN_HORIZONTAL),
          MIN(h_zoomed, RES_HW_SCREEN_VERTICAL));
    }
    else{
      /*flip_NNOptimized_AllowOutOfScreen(screen, hw_screen,
          MAX(screen->w*h_zoomed/screen->h, RES_HW_SCREEN_HORIZONTAL),
          MIN(h_zoomed, RES_HW_SCREEN_VERTICAL));*/
        flip_Upscaling_Bilinear_Yonly(screen, hw_screen,
          MAX(screen->w*h_zoomed/screen->h, RES_HW_SCREEN_HORIZONTAL),
          MIN(h_zoomed, RES_HW_SCREEN_VERTICAL));
    }
    break;

    case ASPECT_RATIOS_TYPE_CROPPED:
    if(screen->w > RES_HW_SCREEN_HORIZONTAL){
      flip_NNOptimized_LeftAndRightBilinear(screen, hw_screen,
        MAX(screen->w*RES_HW_SCREEN_VERTICAL/screen->h, RES_HW_SCREEN_HORIZONTAL),
        RES_HW_SCREEN_VERTICAL);
    }
    else{
      flip_Upscaling_Bilinear_Yonly(screen, hw_screen,
        MAX(screen->w*RES_HW_SCREEN_VERTICAL/screen->h, RES_HW_SCREEN_HORIZONTAL),
        RES_HW_SCREEN_VERTICAL);
    }
    break;

    case ASPECT_RATIOS_TYPE_SCALED:
    if(screen->w > RES_HW_SCREEN_HORIZONTAL){
      flip_NNOptimized_LeftAndRightBilinear(screen, hw_screen,
        RES_HW_SCREEN_HORIZONTAL,
        MIN(screen->h*RES_HW_SCREEN_HORIZONTAL/screen->w, RES_HW_SCREEN_VERTICAL));
    }
    else{
      flip_NNOptimized_AllowOutOfScreen(screen, hw_screen,
        RES_HW_SCREEN_HORIZONTAL,
        MIN(screen->h*RES_HW_SCREEN_HORIZONTAL/screen->w, RES_HW_SCREEN_VERTICAL));
    }
    break;

    default:
    printf("Wrong aspect ratio value: %d\n", aspect_ratio);
    aspect_ratio = ASPECT_RATIOS_TYPE_STRETCHED;
    flip_NNOptimized_AllowOutOfScreen(screen, hw_screen,
        RES_HW_SCREEN_HORIZONTAL, RES_HW_SCREEN_VERTICAL);
    break;
  }
  #endif
}


u32 frame_to_render;

void update_screen()
{
  if(!skip_next_frame)
    render_game();
}

void flip_screen()
{
  SDL_Flip(hw_screen);
}


void init_video()
{

    /* Set env var for no mouse */
    putenv(strdup("SDL_NOMOUSE=1"));

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_NOPARACHUTE);

    if(TTF_Init())
    {
        fprintf(stderr, "Error TTF_Init: %s\n", TTF_GetError());
        exit(EXIT_FAILURE);
    }

  hw_screen = SDL_SetVideoMode(RES_HW_SCREEN_HORIZONTAL, RES_HW_SCREEN_VERTICAL,
                                16, SDL_FULLSCREEN | SDL_HWSURFACE | SDL_DOUBLEBUF);
  printf("* init_video: Creating %dx%d surface\n", RES_HW_SCREEN_HORIZONTAL, RES_HW_SCREEN_VERTICAL);
  virtual_hw_screen = SDL_CreateRGBSurface(SDL_SWSURFACE, RES_HW_SCREEN_HORIZONTAL, RES_HW_SCREEN_VERTICAL, 16, 0xFFFF, 0xFFFF, 0xFFFF, 0);

  if (screen)
    SDL_FreeSurface(screen);
  screen = SDL_CreateRGBSurface(SDL_SWSURFACE, GBA_SCREEN_WIDTH, GBA_SCREEN_HEIGHT, 16, 0xFFFF, 0xFFFF, 0xFFFF, 0);

  screenRect.x = 0;
  screenRect.y = 40;
  screenRect.w = 240;
  screenRect.h = 160;
  gbaRect.x = 0;
  gbaRect.y = 0;
  gbaRect.w = 240;
  gbaRect.h = 160;

  SDL_ShowCursor(0);
}

void deinit_video()
{
  if (screen)
      SDL_FreeSurface(screen);
  SDL_FreeSurface(virtual_hw_screen);
  TTF_Quit();
}

video_scale_type screen_scale = scaled_aspect;
video_scale_type current_scale = scaled_aspect;
video_filter_type screen_filter = filter_bilinear;
video_filter_type2 screen_filter2 = filter2_none;


#ifdef PSP_BUILD

void video_resolution_large()
{
  if(video_direct != 1)
  {
    video_direct = 1;
    screen_pixels = psp_gu_vram_base;
    screen_pitch = 512;
    sceGuStart(GU_DIRECT, display_list);
    sceGuDispBuffer(PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT,
     (void*)0, PSP_LINE_SIZE);
    sceGuFinish();
  }
}

void set_gba_resolution(video_scale_type scale)
{
  u32 filter_linear = 0;
  screen_scale = scale;
  switch(scale)
  {
    case unscaled:
      screen_vertex[2] = 120 + 0.5;
      screen_vertex[3] = 56 + 0.5;
      screen_vertex[7] = GBA_SCREEN_WIDTH + 120 - 0.5;
      screen_vertex[8] = GBA_SCREEN_HEIGHT + 56 - 0.5;
      break;

    case scaled_aspect:
      screen_vertex[2] = 36 + 0.5;
      screen_vertex[3] = 0 + 0.5;
      screen_vertex[7] = 408 + 36 - 0.5;
      screen_vertex[8] = PSP_SCREEN_HEIGHT - 0.5;
      break;

    case fullscreen:
      screen_vertex[2] = 0;
      screen_vertex[3] = 0;
      screen_vertex[7] = PSP_SCREEN_WIDTH;
      screen_vertex[8] = PSP_SCREEN_HEIGHT;
      break;
  }

  sceGuStart(GU_DIRECT, display_list);
  if(screen_filter == filter_bilinear)
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
  else
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);

  sceGuFinish();
  sceGuSync(0, 0);

  clear_screen(0x0000);
}

void video_resolution_small()
{
  if(video_direct != 0)
  {
    set_gba_resolution(screen_scale);
    video_direct = 0;
    screen_pixels = screen_texture;
    screen_flip = 0;
    screen_pitch = 240;
    sceGuStart(GU_DIRECT, display_list);
    sceGuDispBuffer(PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT,
     (void*)0, PSP_LINE_SIZE);
    sceGuFinish();
  }
}

void clear_screen(u16 color)
{
  u32 i;
  u16 *src_ptr = get_screen_pixels();

  sceGuSync(0, 0);

  for(i = 0; i < (512 * 272); i++, src_ptr++)
  {
    *src_ptr = color;
  }

  // I don't know why this doesn't work.
/*  color = (((color & 0x1F) * 255 / 31) << 0) |
   ((((color >> 5) & 0x3F) * 255 / 63) << 8) |
   ((((color >> 11) & 0x1F) * 255 / 31) << 16) | (0xFF << 24);

  sceGuStart(GU_DIRECT, display_list);
  sceGuDrawBuffer(GU_PSM_5650, (void*)0, PSP_LINE_SIZE);
  //sceGuDispBuffer(PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT,
  // (void*)0, PSP_LINE_SIZE);
  sceGuClearColor(color);
  sceGuClear(GU_COLOR_BUFFER_BIT);
  sceGuFinish();
  sceGuSync(0, 0); */
}

#elif defined(POLLUX_BUILD)

void video_resolution_large()
{
  screen_offset = 0;
  resolution_width = 320;
  resolution_height = 240;

  fb_use_buffers(1);
  flip_screen();
  clear_screen(0);
  wiz_lcd_set_portrait(0);
}

void video_resolution_small()
{
  fb_use_buffers(4);

  switch (screen_scale)
  {
    case unscaled:
      screen_offset = 320*40 + 40;
      wiz_lcd_set_portrait(0);
      break;
    case scaled_aspect:
      screen_offset = 320*(80 - 14) + 80;
      wiz_lcd_set_portrait(0);
      break;
    case unscaled_rot:
      wiz_lcd_set_portrait(1);
      rot_lines_total = 4;
      rot_line_count = 0;
      break;
    case scaled_aspect_rot:
      wiz_lcd_set_portrait(1);
      rot_lines_total = 3;
      rot_line_count = 0;
      break;
  }

  flip_screen();
  clear_screen(0);

  resolution_width = 240;
  resolution_height = 160;
}

void set_gba_resolution(video_scale_type scale)
{
  screen_scale = scale;
}

void clear_screen(u16 color)
{
  u32 col = ((u32)color << 16) | color;
  u32 *p = gpsp_gp2x_screen;
  int c = 320*240/2;
  while (c-- > 0)
    *p++ = col;
}

#elif defined(PND_BUILD) || defined(RPI_BUILD)

void video_resolution_large()
{
#if defined (RPI_BUILD)
  resolution_width = 480;
#else
  resolution_width = 400;
#endif
  resolution_height = 272;

  fb_set_mode(resolution_width, resolution_height, 1, 15, screen_filter, screen_filter2);
  flip_screen();
  clear_screen(0);
}

void video_resolution_small()
{
  resolution_width = 240;
  resolution_height = 160;

  fb_set_mode(resolution_width, resolution_height, 3, screen_scale, screen_filter, screen_filter2);
  flip_screen();
  clear_screen(0);
}

void set_gba_resolution(video_scale_type scale)
{
  screen_scale = scale;
}

void clear_screen(u16 color)
{
  u32 col = ((u32)color << 16) | color;
  u32 *p = (u32 *)get_screen_pixels();
  int c = resolution_width * resolution_height / 2;
  while (c-- > 0)
    *p++ = col;
}

#else

void video_resolution_large()
{
  current_scale = unscaled;

#ifdef GP2X_BUILD
  SDL_FreeSurface(screen);
  SDL_GP2X_AllowGfxMemory(NULL, 0);
    hw_screen = SDL_SetVideoMode(320, 240, 16, SDL_HWSURFACE | SDL_DOUBLEBUF);
  screen = SDL_CreateRGBSurface(SDL_HWSURFACE, 320, 240, 16, 0xFFFF,
   0xFFFF, 0xFFFF, 0);
  resolution_width = 320;
    resolution_height = 240;
  SDL_ShowCursor(0);

  warm_change_cb_upper(WCB_C_BIT|WCB_B_BIT, 1);
#elif defined(CHIP_BUILD)
  hw_screen = SDL_SetVideoMode(RES_HW_SCREEN_HORIZONTAL, RES_HW_SCREEN_VERTICAL,
                                16, SDL_FULLSCREEN | SDL_HWSURFACE | SDL_DOUBLEBUF);
  printf("* init_video: Creating %dx%d surface\n", RES_HW_SCREEN_HORIZONTAL, RES_HW_SCREEN_VERTICAL);
  if (screen)
    SDL_FreeSurface(screen);
  screen = SDL_CreateRGBSurface(SDL_SWSURFACE, 320, 240, 16, 0xFFFF, 0xFFFF, 0xFFFF, 0);
  printf("* video_resolution_large: Creating 320x240 surface\n");
  resolution_width = 320;
  resolution_height = 240;
#else
  screen = SDL_SetVideoMode(480, 272, 16, SDL_FULLSCREEN | SDL_DOUBLEBUF);
  printf("* video_resolution_large: Setting resolution to 480x272\n");
  resolution_width = 480;
  resolution_height = 272;
#endif
}

void video_resolution_small()
{
  current_scale = screen_scale;

#ifdef GP2X_BUILD
  int w, h;
  SDL_FreeSurface(screen);
  SDL_GP2X_AllowGfxMemory(NULL, 0);

  w = 320; h = 240;
  if (screen_scale == scaled_aspect || screen_scale == fullscreen)
  {
    w = small_resolution_width * video_scale;
    h = small_resolution_height * video_scale;
  }
  if (screen_scale == scaled_aspect) h += 20;
  hw_screen = SDL_SetVideoMode(w, h, 16, SDL_HWSURFACE);

  w = small_resolution_width * video_scale;
  if (screen_scale == scaled_aspect_sw)
    w = 320;
  screen = SDL_CreateRGBSurface(SDL_HWSURFACE,
   w, small_resolution_height * video_scale,
   16, 0xFFFF, 0xFFFF, 0xFFFF, 0);

  SDL_ShowCursor(0);

  warm_change_cb_upper(WCB_C_BIT|WCB_B_BIT, 1);
#elif defined(CHIP_BUILD)
  hw_screen = SDL_SetVideoMode(RES_HW_SCREEN_HORIZONTAL, RES_HW_SCREEN_VERTICAL,
                                16, SDL_FULLSCREEN | SDL_HWSURFACE | SDL_DOUBLEBUF);
  printf("* init_video: Creating %dx%d surface\n", RES_HW_SCREEN_HORIZONTAL, RES_HW_SCREEN_VERTICAL);
  if (screen)
    SDL_FreeSurface(screen);
  screen = SDL_CreateRGBSurface(SDL_SWSURFACE, GBA_SCREEN_WIDTH, GBA_SCREEN_HEIGHT, 16, 0xFFFF, 0xFFFF, 0xFFFF, 0);
  printf("* video_resolution_small: Creating 240x160 surface\n");
#else
  screen = SDL_SetVideoMode(small_resolution_width * video_scale,
   small_resolution_height * video_scale, 16, 0);
  printf("* video_resolution_small: Setting resolution to (%d * %d)x(%d * %d).\n",
    small_resolution_width, video_scale, small_resolution_height, video_scale);
#endif
  resolution_width = GBA_SCREEN_WIDTH;
  resolution_height = GBA_SCREEN_HEIGHT;
}

void set_gba_resolution(video_scale_type scale)
{
  if(screen_scale != scale)
  {
    screen_scale = scale;
    small_resolution_width = 240 * video_scale;
    small_resolution_height = 160 * video_scale;
  }
}

void clear_screen(u16 color)
{
  u16 *dest_ptr = get_screen_pixels();
  u32 line_skip = get_screen_pitch() - screen->w;
  u32 x, y;

  for(y = 0; y < screen->h; y++)
  {
    for(x = 0; x < screen->w; x++, dest_ptr++)
    {
      *dest_ptr = color;
    }
    dest_ptr += line_skip;
  }
}

#endif

u16 *copy_screen()
{
  u16 *copy = malloc(240 * 160 * 2);
  memcpy(copy, get_screen_pixels(), 240 * 160 * 2);
  return copy;
}

void blit_to_screen(u16 *src, u32 w, u32 h, u32 dest_x, u32 dest_y)
{
  u32 pitch = get_screen_pitch();
  u16 *dest_ptr = get_screen_pixels() + dest_x + (dest_y * pitch);

  s32 w1 = dest_x + w > pitch ? pitch - dest_x : w;
  u16 *src_ptr = src;
  s32 x, y;

  memcpy(screen->pixels, src, (w * h * pitch));
  printf("BLIT?\n");
  /*
  for(y = 0; y < h; y++)
  {
    for(x = 0; x < w1; x++)
    {
      dest_ptr[x] = src_ptr[x];
    }
    src_ptr += w;
    dest_ptr += pitch;
  }
  */
}

/*void print_string_ext(const char *str, u16 fg_color, u16 bg_color,
 u32 x, u32 y, void *_dest_ptr, u32 pitch, u32 pad, u32 h_offset, u32 height)
{
  u16 *dest_ptr = (u16 *)_dest_ptr + (y * pitch) + x;
  u8 current_char = str[0];
  u32 current_row;
  u32 glyph_offset;
  u32 i = 0, i2, i3, h;
  u32 str_index = 1;
  u32 current_x = x;

  if(y + height > resolution_height)
      return;

  while(current_char)
  {
    if(current_char == '\n')
    {
      y += FONT_HEIGHT;
      current_x = x;
      dest_ptr = get_screen_pixels() + (y * pitch) + x;
    }
    else
    {
      glyph_offset = _font_offset[current_char];
      current_x += FONT_WIDTH;
      glyph_offset += h_offset;
      for(i2 = h_offset, h = 0; i2 < FONT_HEIGHT && h < height; i2++, h++, glyph_offset++)
      {
        current_row = _font_bits[glyph_offset];
        for(i3 = 0; i3 < FONT_WIDTH; i3++)
        {
          if((current_row >> (15 - i3)) & 0x01)
            *dest_ptr = fg_color;
          else
            *dest_ptr = bg_color;
          dest_ptr++;
        }
        dest_ptr += (pitch - FONT_WIDTH);
      }
      dest_ptr = dest_ptr - (pitch * h) + FONT_WIDTH;
    }

    i++;

    current_char = str[str_index];

    if((i < pad) && (current_char == 0))
    {
      current_char = ' ';
    }
    else
    {
      str_index++;
    }

    if(current_x + FONT_WIDTH > resolution_width)
    {
      while (current_char && current_char != '\n')
      {
        current_char = str[str_index++];
      }
    }
  }
}*/

void print_string_ext(const char *str, SDL_Surface *screen_surface, u16 fg_color, u16 bg_color,
 u32 x, u32 y, u32 pad, u32 h_offset, u32 height)
{
  u32 pitch = (screen_surface->pitch >> 1);
  u16 *dest_ptr = ((u16*)screen_surface->pixels) + (y * pitch) + x;
  u8 current_char = str[0];
  u32 current_row;
  u32 glyph_offset;
  u32 i = 0, i2, i3, h;
  u32 str_index = 1;
  u32 current_x = x;

  if(y + height > screen_surface->h)
      return;

  while(current_char)
  {
    if(current_char == '\n')
    {
      y += FONT_HEIGHT;
      current_x = x;
      dest_ptr = ((u16*)screen_surface->pixels) + (y * pitch) + x;
    }
    else
    {
      glyph_offset = _font_offset[current_char];
      current_x += FONT_WIDTH;
      glyph_offset += h_offset;
      for(i2 = h_offset, h = 0; i2 < FONT_HEIGHT && h < height; i2++, h++, glyph_offset++)
      {
        current_row = _font_bits[glyph_offset];
        for(i3 = 0; i3 < FONT_WIDTH; i3++)
        {
          if((current_row >> (15 - i3)) & 0x01)
            *dest_ptr = fg_color;
          else
            *dest_ptr = bg_color;
          dest_ptr++;
        }
        dest_ptr += (pitch - FONT_WIDTH);
      }
      dest_ptr = dest_ptr - (pitch * h) + FONT_WIDTH;
    }

    i++;

    current_char = str[str_index];

    if((i < pad) && (current_char == 0))
    {
      current_char = ' ';
    }
    else if(current_x + FONT_WIDTH > screen_surface->w /* EDIT */)
    {
      /*while (current_char && current_char != '\n')
      {
        current_char = str[str_index++];
      }*/
      current_char = '\n';
    }
    else
    {
      str_index++;
    }


  }
}

void print_string(const char *str, u16 fg_color, u16 bg_color,
 u32 x, u32 y)
{
  print_string_ext(str, virtual_hw_screen, fg_color, bg_color, x, y, 0, 0, FONT_HEIGHT);
}

void print_string_pad(const char *str, u16 fg_color, u16 bg_color,
 u32 x, u32 y, u32 pad)
{
  print_string_ext(str, virtual_hw_screen, fg_color, bg_color, x, y, pad, 0, FONT_HEIGHT);
}

void set_hud_msg(char *msg, int seconds_on_screen){
  hud_ms_at_launch = SDL_GetTicks();
  strncpy(hud_msg, msg, HUD_MSG_MAX_SIZE);
  hud_msg_secs_on_screen = seconds_on_screen;
}

int print_hud(){
  int print_hud = 0;

  if(!hud_msg[0]){
    return 0;
  }

  if(hud_msg_secs_on_screen == -1){
    print_hud = 1;
  }
  else if( hud_msg_secs_on_screen >= 0){
    if(SDL_GetTicks() - hud_ms_at_launch > 1000){
      hud_msg_secs_on_screen--;
      hud_ms_at_launch = SDL_GetTicks();
      if(!hud_msg_secs_on_screen){
        memset(hud_msg, 0, HUD_MSG_MAX_SIZE);
      }
    }
    print_hud = 1;
  }

  if(print_hud){
    print_string(hud_msg, 0xFFFF, 0x000, 0, 0);
  }

  return print_hud;
}

u32 debug_cursor_x = 0;
u32 debug_cursor_y = 0;

#ifdef STDIO_DEBUG

void debug_screen_clear()
{
}

void debug_screen_start()
{
}

void debug_screen_end()
{
}

void debug_screen_update()
{
}

void debug_screen_printf(const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  vprintf(format, ap);
  va_end(ap);
}

void debug_screen_newline(u32 count)
{
  printf("\n");
}


#else

void debug_screen_clear()
{
  debug_cursor_x = 0;
  debug_cursor_y = 0;
  clear_screen(0x0000);
}

void debug_screen_start()
{
  video_resolution_large();
  debug_screen_clear();
}

void debug_screen_end()
{
  video_resolution_small();
}

void debug_screen_update()
{
  flip_screen();
}

void debug_screen_printf(const char *format, ...)
{
  char str_buffer[512];
  u32 str_buffer_length;
  va_list ap;

  va_start(ap, format);
  str_buffer_length = vsnprintf(str_buffer, 512, format, ap);
  va_end(ap);

  printf("printing debug string %s at %d %d\n", str_buffer,
   debug_cursor_x, debug_cursor_y);

  print_string(str_buffer, 0xFFFF, 0x0000, debug_cursor_x, debug_cursor_y);
  debug_cursor_x += FONT_WIDTH * str_buffer_length;
}

void debug_screen_newline(u32 count)
{
  debug_cursor_x = 0;
  debug_cursor_y += FONT_HEIGHT * count;
}

#endif

void debug_screen_printl(const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  debug_screen_printf(format, ap);
  debug_screen_newline(1);
//  debug_screen_printf("\n");
  va_end(ap);
}


#define video_savestate_builder(type)                                         \
void video_##type##_savestate(file_tag_type savestate_file)                   \
{                                                                             \
  file_##type##_array(savestate_file, affine_reference_x);                    \
  file_##type##_array(savestate_file, affine_reference_y);                    \
}                                                                             \

video_savestate_builder(read);
video_savestate_builder(write_mem);


