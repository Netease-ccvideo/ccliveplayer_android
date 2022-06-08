/*****************************************************************************
 * ijksdl_vout_overlay_ffmpeg.c
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

#include "ijksdl_vout_overlay_ffmpeg.h"

#include <stdbool.h>
#include <assert.h>
#include "../ijksdl_stdinc.h"
#include "../ijksdl_mutex.h"
#include "../ijksdl_vout_internal.h"
#include "../ijksdl_video.h"
#include "ijksdl_inc_ffmpeg.h"
#include "ijksdl_image_convert.h"

typedef struct SDL_VoutOverlay_Opaque {
    SDL_mutex *mutex;

    AVFrame *managed_frame;
    AVBufferRef *frame_buffer;
    int planes;
    int rotate_buffer_size;
    uint8_t* rotate_buffer;
    AVFrame *linked_frame;

    Uint16 pitches[AV_NUM_DATA_POINTERS];
    Uint8 *pixels[AV_NUM_DATA_POINTERS];

    int no_neon_warned;
    struct SwsContext *img_convert_ctx;
    int sws_flags;
} SDL_VoutOverlay_Opaque;

/* Always assume a linesize alignment of 1 here */
// TODO: 9 alignment to speed up memcpy when display
static AVFrame *opaque_setup_frame(SDL_VoutOverlay_Opaque* opaque, enum AVPixelFormat format, int width, int height)
{
    AVFrame *managed_frame = av_frame_alloc();
    if (!managed_frame) {
        return NULL;
    }

    AVFrame *linked_frame = av_frame_alloc();
    if (!linked_frame) {
        av_frame_free(&managed_frame);
        return NULL;
    }

    /*-
     * Lazily allocate frame buffer in opaque_obtain_managed_frame_buffer
     *
     * For refererenced frame management, we use buffer allocated by decoder
     *
    int frame_bytes = avpicture_get_size(format, width, height);
    AVBufferRef *frame_buffer_ref = av_buffer_alloc(frame_bytes);
    if (!frame_buffer_ref)
        return NULL;
    opaque->frame_buffer  = frame_buffer_ref;
     */

    AVPicture *pic = (AVPicture *) managed_frame;
    managed_frame->format = format;
    managed_frame->width  = width;
    managed_frame->height = height;
    avpicture_fill(pic, NULL, format, width, height);
    opaque->managed_frame = managed_frame;
    opaque->linked_frame  = linked_frame;
    return managed_frame;
}

static AVFrame *opaque_obtain_managed_frame_buffer(SDL_VoutOverlay_Opaque* opaque)
{
    if (opaque->frame_buffer != NULL)
        return opaque->managed_frame;

    AVFrame *managed_frame = opaque->managed_frame;
    int frame_bytes = avpicture_get_size(managed_frame->format, managed_frame->width, managed_frame->height);
    AVBufferRef *frame_buffer_ref = av_buffer_alloc(frame_bytes);
    if (!frame_buffer_ref)
        return NULL;

    AVPicture *pic = (AVPicture *) managed_frame;
    avpicture_fill(pic, frame_buffer_ref->data, managed_frame->format, managed_frame->width, managed_frame->height);
    opaque->frame_buffer  = frame_buffer_ref;
    return opaque->managed_frame;
}

static void overlay_free_l(SDL_VoutOverlay *overlay)
{
    ALOGI("SDL_Overlay(ffmpeg): overlay_free_l(%p)\n", overlay);
    if (!overlay)
        return;

    SDL_VoutOverlay_Opaque *opaque = overlay->opaque;
    if (!opaque)
        return;

    if (opaque->managed_frame)
        av_frame_free(&opaque->managed_frame);

    if (opaque->linked_frame) {
        av_frame_unref(opaque->linked_frame);
        av_frame_free(&opaque->linked_frame);
    }

    if(opaque->rotate_buffer)
        av_freep(&opaque->rotate_buffer);

    if (opaque->frame_buffer)
        av_buffer_unref(&opaque->frame_buffer);

    if (opaque->mutex)
        SDL_DestroyMutex(opaque->mutex);

    if (opaque->img_convert_ctx)
        sws_freeContext(opaque->img_convert_ctx);
    SDL_VoutOverlay_FreeInternal(overlay);
}

static void overlay_fill(SDL_VoutOverlay *overlay, AVFrame *frame, int planes)
{
    AVPicture *pic = (AVPicture *) frame;
    overlay->planes = planes;

    for (int i = 0; i < AV_NUM_DATA_POINTERS; ++i) {
        overlay->pixels[i] = pic->data[i];
        overlay->pitches[i] = pic->linesize[i];
    }
}

static int overlay_lock(SDL_VoutOverlay *overlay)
{
    SDL_VoutOverlay_Opaque *opaque = overlay->opaque;
    return SDL_LockMutex(opaque->mutex);
}

static int overlay_unlock(SDL_VoutOverlay *overlay)
{
    SDL_VoutOverlay_Opaque *opaque = overlay->opaque;
    return SDL_UnlockMutex(opaque->mutex);
}

static void rotate_frame(SDL_VoutOverlay *overlay, AVFrame *frame) {
    assert(overlay);
    SDL_VoutOverlay_Opaque *opaque = overlay->opaque;
    AVFrame *frame_rotate = av_frame_alloc();
    if (overlay->rotate == 90 || overlay->rotate == 270) {
        frame_rotate->width = frame->height;
        frame_rotate->height = frame->width;
    } else {
        frame_rotate->width = frame->width;
        frame_rotate->height = frame->height;
    }
    frame_rotate->format = frame->format;
    int pic_size = avpicture_get_size(frame_rotate->format, frame_rotate->width,
                                      frame_rotate->height);
    if (opaque->rotate_buffer_size != pic_size) {
        if (opaque->rotate_buffer)
            av_freep(&opaque->rotate_buffer);
        opaque->rotate_buffer = (uint8_t *) av_malloc(pic_size);
        opaque->rotate_buffer_size = pic_size;
    }

    avpicture_fill((AVPicture *) frame_rotate, opaque->rotate_buffer, frame_rotate->format,
                   frame_rotate->width, frame_rotate->height);

    I420Rotate(frame->data[0], frame->linesize[0],
               frame->data[1], frame->linesize[0] >> 1,
               frame->data[2], frame->linesize[0] >> 1,
               frame_rotate->data[0], frame_rotate->width,
               frame_rotate->data[1], frame_rotate->width >> 1,
               frame_rotate->data[2], frame_rotate->width >> 1,
               frame->width, frame->height, overlay->rotate);
    frame->data[0] = frame_rotate->data[0];
    frame->data[1] = frame_rotate->data[1];
    frame->data[2] = frame_rotate->data[2];
    frame->linesize[0] = frame_rotate->linesize[0];
    frame->linesize[1] = frame_rotate->linesize[1];
    frame->linesize[2] = frame_rotate->linesize[2];
    frame->width = frame_rotate->width;
    frame->height = frame_rotate->height;
    av_frame_free(&frame_rotate);
}

static int func_fill_frame(SDL_VoutOverlay *overlay, AVFrame *frame)
{
    assert(overlay);
    SDL_VoutOverlay_Opaque *opaque = overlay->opaque;
    AVFrame swscale_dst_pic = { { 0 } };
    
    av_frame_unref(opaque->linked_frame);

    int need_swap_uv = 0;
    int use_linked_frame = 0;
    enum AVPixelFormat dst_format = AV_PIX_FMT_NONE;
    switch (overlay->format) {
        case SDL_FCC_YV12:
            need_swap_uv = 1;
            // no break;
        case SDL_FCC_I420:
            if (frame->format == AV_PIX_FMT_YUV420P || frame->format == AV_PIX_FMT_YUVJ420P) {
                // ALOGE("direct draw frame");
                use_linked_frame = 1;
                dst_format = frame->format;
            } else {
                // ALOGE("copy draw frame");
                dst_format = AV_PIX_FMT_YUV420P;
            }
            break;
        case SDL_FCC_I444P10LE:
            if (frame->format == AV_PIX_FMT_YUV444P10LE) {
                // ALOGE("direct draw frame");
                use_linked_frame = 1;
                dst_format = frame->format;
            } else {
                // ALOGE("copy draw frame");
                dst_format = AV_PIX_FMT_YUV444P10LE;
            }
            break;
        case SDL_FCC_RV32:
            dst_format = AV_PIX_FMT_0BGR32;
            break;
        case SDL_FCC_RV24:
            dst_format = AV_PIX_FMT_RGB24;
            break;
        case SDL_FCC_RV16:
            dst_format = AV_PIX_FMT_RGB565;
            break;
        default:
            ALOGE("SDL_VoutFFmpeg_ConvertPicture: unexpected overlay format %s(%d)",
                  (char*)&overlay->format, overlay->format);
            return -1;
    }
    
    
    // setup frame
    if (use_linked_frame) {
        // linked frame
        // decode raw data一般是vulkan模式，由接入方进行旋转
        if(!overlay->decode_out_raw_data
           && (frame->format == AV_PIX_FMT_YUV420P || frame->format == AV_PIX_FMT_YUVJ420P)
           && overlay->rotate != 0) {
            rotate_frame(overlay, frame);
        }
        av_frame_ref(opaque->linked_frame, frame);
        
        overlay_fill(overlay, opaque->linked_frame, opaque->planes);
        
        if (need_swap_uv)
            FFSWAP(Uint8*, overlay->pixels[1], overlay->pixels[2]);
    } else {
        // managed frame
        AVFrame* managed_frame = opaque_obtain_managed_frame_buffer(opaque);
        if (!managed_frame) {
            ALOGE("OOM in opaque_obtain_managed_frame_buffer");
            return -1;
        }
        
        overlay_fill(overlay, opaque->managed_frame, opaque->planes);
        
        // setup frame managed
        for (int i = 0; i < overlay->planes; ++i) {
            swscale_dst_pic.data[i] = overlay->pixels[i];
            swscale_dst_pic.linesize[i] = overlay->pitches[i];
        }
        
        if (need_swap_uv)
            FFSWAP(Uint8*, swscale_dst_pic.data[1], swscale_dst_pic.data[2]);
    }
    
    
    // swscale / direct draw
    /*
     ALOGE("ijk_image_convert w=%d, h=%d, df=%d, dd=%d, dl=%d, sf=%d, sd=%d, sl=%d",
     (int)frame->width,
     (int)frame->height,
     (int)dst_format,
     (int)swscale_dst_pic.data[0],
     (int)swscale_dst_pic.linesize[0],
     (int)frame->format,
     (int)(const uint8_t**) frame->data,
     (int)frame->linesize);
     */
    if (use_linked_frame) {
        // do nothing
    } else if (ijk_image_convert(frame->width, frame->height,
                                 dst_format, swscale_dst_pic.data, swscale_dst_pic.linesize,
                                 frame->format, (const uint8_t**) frame->data, frame->linesize)) {
        opaque->img_convert_ctx = sws_getCachedContext(opaque->img_convert_ctx,
                                                       frame->width, frame->height, frame->format, frame->width, frame->height,
                                                       dst_format, opaque->sws_flags, NULL, NULL, NULL);
        if (opaque->img_convert_ctx == NULL) {
            ALOGE("sws_getCachedContext failed");
            return -1;
        }
        
        sws_scale(opaque->img_convert_ctx, (const uint8_t**) frame->data, frame->linesize,
                  0, frame->height, swscale_dst_pic.data, swscale_dst_pic.linesize);
        
        if (!opaque->no_neon_warned) {
            opaque->no_neon_warned = 1;
            ALOGE("non-neon image convert %s -> %s", av_get_pix_fmt_name(frame->format), av_get_pix_fmt_name(dst_format));
        }
    }
    
    // TODO: 9 draw black if overlay is larger than screen
    return 0;
}

static SDL_Class g_vout_overlay_ffmpeg_class = {
    .name = "FFmpegVoutOverlay",
};

#ifndef __clang_analyzer__
SDL_VoutOverlay *SDL_VoutFFmpeg_CreateOverlay(int width, int height, Uint32 frame_format, SDL_Vout *display, int crop, int surface_width, int surface_height)
{
    Uint32 overlay_format = display->overlay_format;
    switch (overlay_format) {
        case SDL_FCC__GLES2: {
            switch (frame_format) {
                case AV_PIX_FMT_YUV444P10LE:
                    overlay_format = SDL_FCC_I444P10LE;
                    break;
                case AV_PIX_FMT_YUV420P:
                case AV_PIX_FMT_YUVJ420P:
                default:
#if defined(__ANDROID__)
                    overlay_format = SDL_FCC_YV12;
#else
                    overlay_format = SDL_FCC_I420;
#endif
                    break;
            }
            break;
        }
    }
    SDLTRACE("SDL_VoutFFmpeg_CreateOverlay(w=%d, h=%d, fmt=%.4s(0x%x, dp=%p)\n",
        width, height, (const char*) &frame_format, frame_format, display);
    SDL_VoutOverlay *overlay = SDL_VoutOverlay_CreateInternal(sizeof(SDL_VoutOverlay_Opaque));
    if (!overlay) {
        ALOGE("overlay allocation failed");
        return NULL;
    }

    SDL_VoutOverlay_Opaque *opaque = overlay->opaque;
    opaque->mutex         = SDL_CreateMutex();
    opaque->sws_flags     = SWS_BILINEAR;

    overlay->opaque_class = &g_vout_overlay_ffmpeg_class;
    overlay->format       = overlay_format;
    overlay->pitches      = opaque->pitches;
    overlay->pixels       = opaque->pixels;
    overlay->w            = width;
    overlay->h            = height;
    overlay->free_l       = overlay_free_l;
    overlay->lock         = overlay_lock;
    overlay->unlock       = overlay_unlock;
    overlay->func_fill_frame = func_fill_frame;
    overlay->crop = crop;
    overlay->reset_padding = true;
    overlay->wanted_display_width = surface_width;
    overlay->wanted_display_height = surface_height;
    overlay->decode_out_raw_data = display->decode_out_raw_data;
    enum AVPixelFormat ff_format = AV_PIX_FMT_NONE;
    int buf_width = width;
    int buf_height = height;
    switch (overlay_format) {
    case SDL_FCC_I420:
    case SDL_FCC_YV12: {
        ff_format = AV_PIX_FMT_YUV420P;
        // FIXME: need runtime config
#if defined(__ANDROID__)
        // 16 bytes align pitch for arm-neon image-convert
        buf_width = IJKALIGN(width, 16); // 1 bytes per pixel for Y-plane
#elif defined(__APPLE__)
        // 2^n align for width
        buf_width = width;
        if (width > 0)
            buf_width = 1 << (sizeof(int) * 8 - __builtin_clz(width));
#else
        buf_width = IJKALIGN(width, 16); // unknown platform
#endif
        opaque->planes = 3;
        break;
    }
    case SDL_FCC_RV16: {
        ff_format = AV_PIX_FMT_RGB565;
        buf_width = IJKALIGN(width, 8); // 2 bytes per pixel
        opaque->planes = 1;
        break;
    }
    case SDL_FCC_RV24: {
        ff_format = AV_PIX_FMT_RGB24;
#if defined(__ANDROID__)
        // 16 bytes align pitch for arm-neon image-convert
        buf_width = IJKALIGN(width, 16); // 1 bytes per pixel for Y-plane
#elif defined(__APPLE__)
        buf_width = width;
#else
        buf_width = IJKALIGN(width, 16); // unknown platform
#endif
        opaque->planes = 1;
        break;
    }
    case SDL_FCC_RV32: {
        ff_format = AV_PIX_FMT_0BGR32;
        buf_width = IJKALIGN(width, 4); // 4 bytes per pixel
        opaque->planes = 1;
        break;
    }
    default:
        ALOGE("SDL_VoutFFmpeg_CreateOverlay(...): unknown format %.4s(0x%x)\n", (char*)&overlay_format, overlay_format);
        goto fail;
    }

    opaque->managed_frame = opaque_setup_frame(opaque, ff_format, buf_width, buf_height);
    if (!opaque->managed_frame) {
        ALOGE("overlay->opaque->frame allocation failed\n");
        goto fail;
    }
    overlay_fill(overlay, opaque->managed_frame, opaque->planes);

    return overlay;

fail:
    overlay_free_l(overlay);
    return NULL;
}
#endif//__clang_analyzer__


int SDL_VoutFFmpeg_ConverI420ToARGB(SDL_VoutOverlay *overlay, struct SwsContext **p_sws_ctx, int sws_flags, uint8_t **data, int *pitch)
{
    if (overlay->format == SDL_FCC_RV32) {
        return 0;
    }
    AVPicture swscale_dst_pic = { { 0 } };
    SDL_VoutOverlay_Opaque *opaque = overlay->opaque;
    AVFrame *managed_frame = opaque->managed_frame;
    AVPicture *pic = (AVPicture *) managed_frame;
    enum AVPixelFormat ff_format = AV_PIX_FMT_0BGR32;
    //allocate memory for AV_PIX_FMT_0BGR32 managed_frame
    if (opaque->frame_buffer == NULL) {
        AVFrame *managed_frame = opaque->managed_frame;
        managed_frame->format = ff_format;
        avpicture_fill(pic, NULL, ff_format, managed_frame->width, managed_frame->height);

        int frame_bytes = avpicture_get_size(managed_frame->format, managed_frame->width, managed_frame->height);
        AVBufferRef *frame_buffer_ref = av_buffer_alloc(frame_bytes);
        if (!frame_buffer_ref)
            return -1;
        avpicture_fill(pic, frame_buffer_ref->data, managed_frame->format, managed_frame->width, managed_frame->height);
        opaque->frame_buffer  = frame_buffer_ref;
    }
    swscale_dst_pic.data[0] = pic->data[0];
    swscale_dst_pic.linesize[0] = pic->linesize[0];
    AVFrame *frame = opaque->linked_frame;
    if (ijk_image_convert(frame->width, frame->height,
        ff_format, swscale_dst_pic.data, swscale_dst_pic.linesize,
        frame->format, (const uint8_t**) frame->data, frame->linesize)) {
        *p_sws_ctx = sws_getCachedContext(*p_sws_ctx,
            frame->width, frame->height, frame->format, frame->width, frame->height,
            ff_format, sws_flags, NULL, NULL, NULL);
        if (*p_sws_ctx == NULL) {
            ALOGE("sws_getCachedContext failed");
            return -1;
        }
        sws_scale(*p_sws_ctx, (const uint8_t**) frame->data, frame->linesize,
            0, frame->height, swscale_dst_pic.data, swscale_dst_pic.linesize);
    }
    if (data) {
        *data = pic->data[0];
    }
    if (pitch) {
        *pitch = pic->linesize[0];
    }
    return 0;
}
