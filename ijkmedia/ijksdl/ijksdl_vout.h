/*****************************************************************************
 * ijksdl_vout.h
 *****************************************************************************
 *
 * copyright (c) 2013 Zhang Rui <bbcallen@gmail.com>
 *
 * This file is part of ijkPlayer.
 *
 * ijkPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * ijkPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with ijkPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef IJKSDL__IJKSDL_VOUT_H
#define IJKSDL__IJKSDL_VOUT_H

#include "ijksdl_stdinc.h"
#include "ijksdl_class.h"
#include "ijksdl_mutex.h"
#include "ijksdl_video.h"
#include "ffmpeg/ijksdl_inc_ffmpeg.h"

typedef struct SDL_VoutOverlay_Opaque SDL_VoutOverlay_Opaque;
typedef struct SDL_VoutOverlay SDL_VoutOverlay;
typedef struct SDL_VoutOverlay {
    bool crop;
    bool reset_padding;
    int wanted_display_width;
    int wanted_display_height;
    int crop_padding_horizontal;
    int crop_padding_vertical;
    int buff_w;
    int buff_h;
    int w; /**< Read-only */
    int h; /**< Read-only */
    Uint32 format; /**< Read-only */
    int planes; /**< Read-only */
    Uint16 *pitches; /**< in bytes, Read-only */
    Uint8 **pixels; /**< Read-write */
    char* subtitle;
	long subDuration;
    int sar_num;
    int sar_den;
    int rotate;
    int decode_out_raw_data;
    int is_private;

    SDL_Class               *opaque_class;
    SDL_VoutOverlay_Opaque  *opaque;
    void                    (*free_l)(SDL_VoutOverlay *overlay);
    int                     (*lock)(SDL_VoutOverlay *overlay);
    int                     (*unlock)(SDL_VoutOverlay *overlay);
    void                    (*unref)(SDL_VoutOverlay *overlay);
    
    int                     (*func_fill_frame)(SDL_VoutOverlay *overlay, const AVFrame *frame);
} SDL_VoutOverlay;

typedef struct SDL_Vout_Opaque SDL_Vout_Opaque;
typedef struct SDL_Vout SDL_Vout;
typedef struct SDL_Vout {
    SDL_mutex *mutex;
	SDL_cond  *con;

    SDL_Class       *opaque_class;
    SDL_Vout_Opaque *opaque;
    SDL_VoutOverlay *(*create_overlay)(int width, int height, Uint32 format, SDL_Vout *vout, int crop, int surface_width, int surface_height);
    void (*free_l)(SDL_Vout *vout);
    void (*clear_buffer)(SDL_Vout *vout);//for ios player sdk only
    void (*display_init)(SDL_Vout *vout);
    int (*display_overlay)(SDL_Vout *vout, SDL_VoutOverlay *overlay);
    void (*set_decode_raw_data_enable)(SDL_Vout *vout, int enable);
	int (*get_decode_raw_data_enable)(SDL_Vout *vout);
    int (*on_bind_frame_buffer)(SDL_Vout *vout, SDL_VoutOverlay *overlay);
    int (*on_pre_render_frame)(SDL_Vout *vout, SDL_VoutOverlay *overlay);
    int (*on_post_render_frame)(SDL_Vout *vout, SDL_VoutOverlay *overlay);
    Uint32 overlay_format;
    int panorama;
    int decode_out_raw_data;
    int nYTexture;

    int nWidth;
    int nHeight;
    int useLibYuv;
    unsigned char *rgb;
    int rgb_size;

    char* subtitle;
    long subDuration;
} SDL_Vout;

void SDL_VoutFree(SDL_Vout *vout);
void SDL_VoutFreeP(SDL_Vout **pvout);
void SDL_VoutDisplayInit(SDL_Vout *vout);
int SDL_VoutDisplayYUVOverlay(SDL_Vout *vout, SDL_VoutOverlay *overlay);
int  SDL_VoutSetOverlayFormat(SDL_Vout *vout, Uint32 overlay_format);

SDL_VoutOverlay *SDL_Vout_CreateOverlay(int width, int height, Uint32 format, SDL_Vout *vout, int crop, int surface_width, int surface_height);
int SDL_VoutLockYUVOverlay(SDL_VoutOverlay *overlay);
int SDL_VoutUnlockYUVOverlay(SDL_VoutOverlay *overlay);
void SDL_VoutFreeYUVOverlay(SDL_VoutOverlay *overlay);
void SDL_VoutUnrefYUVOverlay(SDL_VoutOverlay *overlay);
int     SDL_VoutFillFrameYUVOverlay(SDL_VoutOverlay *overlay, const AVFrame *frame);
void SDL_VoutAndroid_DecoderRawData(SDL_Vout *vout, int enable);

#endif
