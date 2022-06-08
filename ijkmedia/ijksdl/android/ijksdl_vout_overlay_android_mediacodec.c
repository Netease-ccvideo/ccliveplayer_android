/*****************************************************************************
 * ijksdl_vout_overlay_android_mediacodec.c
 *****************************************************************************
 *
 * copyright (c) 2014 Zhang Rui <bbcallen@gmail.com>
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

#include "ijksdl_vout_overlay_android_mediacodec.h"
#include "ijksdl_codec_android_mediacodec.h"
#include "../ijksdl_stdinc.h"
#include "../ijksdl_mutex.h"
#include "../ijksdl_vout_internal.h"
#include "../ijksdl_video.h"
#include "ijkutil/ijklog.h"
#include "ijksdl_vout_android_nativewindow.h"

typedef struct SDL_VoutOverlay_Opaque {
    SDL_mutex *mutex;
    SDL_AMediaCodecBufferProxy * buffer_proxy;
    //SDL_AMediaCodec          *acodec;
    volatile bool             is_buffer_own;
    Uint16 pitches[AV_NUM_DATA_POINTERS];
    Uint8 *pixels[AV_NUM_DATA_POINTERS];
    SDL_Vout                   *vout;
} SDL_VoutOverlay_Opaque;
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

static void overlay_unref(SDL_VoutOverlay *overlay)
{
    // TODO: error handle

    SDL_VoutOverlay_Opaque *opaque = overlay->opaque;
    SDL_VoutAndroid_releaseBufferProxy_Vout(&opaque->buffer_proxy,opaque->vout, false);
}

static void overlay_free_l(SDL_VoutOverlay *overlay)
{

    ALOGD("SDL_Overlay(MediaCodec) start: overlay_free_l(%p)\n", overlay);
    if (!overlay)
        return;

    SDL_VoutOverlay_Opaque *opaque = overlay->opaque;
    if (!opaque)
        return;

    overlay_unref(overlay);

    if (opaque->mutex)
        SDL_DestroyMutex(opaque->mutex);

    SDL_VoutOverlay_FreeInternal(overlay);
    ALOGD("SDL_Overlay(MediaCodec): overlay_free_l(%p)\n", overlay);
}

static SDL_Class g_vout_overlay_amediacodec_class = {
    .name = "AndroidMediaCodecVoutOverlay",
};

inline static bool check_object(SDL_VoutOverlay* object, const char *func_name)
{
    if (!object || !object->opaque || !object->opaque_class) {
        ALOGE("%s.%s: invalid pipeline\n", object->opaque_class->name, func_name);
        return false;
    }

    if (object->opaque_class != &g_vout_overlay_amediacodec_class) {
        ALOGE("%s.%s: unsupported method\n", object->opaque_class->name, func_name);
        return false;
    }

    return true;
}

static int func_fill_frame(SDL_VoutOverlay *overlay, const AVFrame *frame )
{
    //ALOGD(" create acodec overlay ");
    SDL_VoutOverlay_Opaque *opaque = overlay->opaque;

    if (!check_object(overlay, __func__))
        return -1;

    if (opaque->buffer_proxy){
        ALOGD(" ConvertFrame use MediaCodec real output buffer ");
        SDL_VoutAndroid_releaseBufferProxy_Vout(&opaque->buffer_proxy,opaque->vout,  false);
    }

    //opaque->acodec       = SDL_VoutAndroid_peekAMediaCodec(opaque->vout);
    // TODO: ref-count buffer_proxy?
    opaque->buffer_proxy = (SDL_AMediaCodecBufferProxy *)frame->opaque;

    opaque->is_buffer_own=true;

    overlay->opaque_class = &g_vout_overlay_amediacodec_class;
    overlay->format     = SDL_FCC__AMC;
    overlay->planes     = 1;
    overlay->w = (int)frame->width;
    overlay->h = (int)frame->height;
    overlay->planes = 3;
    for (int i = 0; i < 3; ++i) {
        overlay->pixels[i] = frame->data[i];
        overlay->pitches[i] = frame->linesize[i];
    }
    //ALOGE("ConvertFrame success and index is  ");
    return 0;
}

SDL_VoutOverlay *SDL_VoutAMediaCodec_CreateOverlay(int width, int height, SDL_Vout *vout)
{
    SDLTRACE("[rotate] SDL_VoutAMediaCode_CreateOverlay width(%d) height(%d)", width, height);
    SDL_VoutOverlay *overlay = SDL_VoutOverlay_CreateInternal(sizeof(SDL_VoutOverlay_Opaque));
    if (!overlay) {
        ALOGE("overlay allocation failed");
        return NULL;
    }

    SDL_VoutOverlay_Opaque *opaque = overlay->opaque;
    opaque->mutex         = SDL_CreateMutex();
    opaque->vout          = vout;
    overlay->opaque_class = &g_vout_overlay_amediacodec_class;
    overlay->format       = SDL_FCC__AMC;
    overlay->pitches      = opaque->pitches;
    overlay->pixels       = opaque->pixels;
    overlay->w            = width;
    overlay->h            = height;
    overlay->free_l       = overlay_free_l;
    overlay->lock         = overlay_lock;
    overlay->unlock       = overlay_unlock;
    overlay->unref        = overlay_unref;
    overlay->func_fill_frame=func_fill_frame;
    overlay->is_private   = 1;//android media codec
//    switch (format) {
//    case SDL_FCC__AMC: {
//        break;
//    }
//    default:
//        ALOGE("SDL_VoutAMediaCodec_CreateOverlay(...): unknown format %.4s(0x%x)\n", (char*)&format, format);
//        goto fail;
//    }
    SDLTRACE("SDL_VoutAMediaCode_CreateOverlay Success");
    return overlay;

}

bool SDL_VoutOverlayAMediaCodec_isKindOf(SDL_VoutOverlay *overlay)
{
    return check_object(overlay, __func__);
}

//int SDL_VoutOverlayAMediaCodec_attachFrame(
//    SDL_VoutOverlay *overlay,
//    SDL_AMediaCodec *acodec,
//    int output_buffer_index,
//    SDL_AMediaCodecBufferInfo *buffer_info)
//{
//    if (!check_object(overlay, __func__))
//        return -1;
//
//    SDL_VoutOverlay_Opaque *opaque = overlay->opaque;
//    opaque->acodec        = acodec;
//    opaque->buffer_index  = output_buffer_index;
//    opaque->buffer_info   = *buffer_info;
//    opaque->is_buffer_own = true;
//
//    SDL_AMediaCodec_increaseReference(acodec);
//    return 0;
//}

int SDL_VoutOverlayAMediaCodec_releaseFrame(SDL_VoutOverlay *overlay, SDL_Vout *vout, bool render)
{

    if (!check_object(overlay, __func__))
            return -1;

    SDL_VoutOverlay_Opaque *opaque = overlay->opaque;

    int ret=0;

    ret=SDL_VoutAndroid_releaseBufferProxy_Vout_Wihtoutlock (&(opaque->buffer_proxy),vout,render);

    return ret;
}
