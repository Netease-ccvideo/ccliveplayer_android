/*****************************************************************************
 * ijksdl_vout_android_nativewindow.c
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

#include "ijksdl_vout_android_nativewindow.h"

#include <assert.h>
#include <android/native_window.h>
#include "ijkutil/ijkutil.h"
#include "ijksdl_vout_overlay_android_mediacodec.h"
#include "android_nativewindow.h"
#include "../ijksdl_vout.h"
#include "../ijksdl_vout_internal.h"
#include "../ffmpeg/ijksdl_vout_overlay_ffmpeg.h"
#include "android_vout_gles.h"
#include "ijksdl_egl.h"
#include "ijksdl_codec_android_mediadef.h"
#include "ijksdl_codec_android_mediacodec.h"

typedef struct SDL_Vout_Opaque {
    ANativeWindow   *native_window;
    SDL_AMediaCodec *weak_acodec;
    SDL_GLSurfaceViewRender gl_render;
    int enable_decoder_out_raw_data;
    void *mp;
    bool(*post_redraw_msg_func)(void*);
    bool(*on_raw_image_available)(void*, unsigned char**, int *, int, int, int, int);
    int(*gl_frame_callback)(void*, int);
    IJK_EGL         *egl;
} SDL_Vout_Opaque;


typedef struct SDL_AMediaCodecBufferProxy
{
    int buffer_index;
    SDL_AMediaCodecBufferInfo buffer_info;
} SDL_AMediaCodecBufferProxy ;

static SDL_VoutOverlay *vout_create_overlay_l(int width, int height, Uint32 format, SDL_Vout *vout, int crop, int surface_width, int surface_height)
{
    ALOGI("overlayer format is %d", format);
    switch (format) {
    case IJK_AV_PIX_FMT__ANDROID_MEDIACODEC:
        return SDL_VoutAMediaCodec_CreateOverlay(width, height, vout);
    default:
        return SDL_VoutFFmpeg_CreateOverlay(width, height, format, vout, crop, surface_width, surface_height);
    }
}

static SDL_VoutOverlay *vout_create_overlay(int width, int height, Uint32 format, SDL_Vout *vout, int crop, int surface_width, int surface_height)
{
    SDL_LockMutex(vout->mutex);
    SDL_VoutOverlay *overlay = vout_create_overlay_l(width, height, format, vout, crop, surface_width, surface_height);
    SDL_UnlockMutex(vout->mutex);
    return overlay;
}

static void vout_free_l(SDL_Vout *vout)
{
    if (!vout)
        return;

    SDL_Vout_Opaque *opaque = vout->opaque;
    if (opaque) {
        if (opaque->native_window) {
            ANativeWindow_release(opaque->native_window);
            opaque->native_window = NULL;
        }
    }
    if(vout->rgb)
    {
        free(vout->rgb);
        vout->rgb = NULL;
    }
    if(vout->panorama == 1)
        IJK_EGL_freep(&opaque->egl);
    SDL_Vout_FreeInternal(vout);
}

static int post_redraw(SDL_Vout *vout, SDL_VoutOverlay *overlay) {
//    while(vout->opaque->gl_render.target_overlay) {
//        SDL_CondWait(vout->con, vout->mutex);
//    }
//    vout->opaque->gl_render.target_overlay = overlay;
//    vout->post_redraw_msg_func(vout->mp);

    if(vout->opaque->enable_decoder_out_raw_data == 1) {
        
        uint8_t* data[3] = {0};
        data[0] = overlay->pixels[0];
        data[1] = overlay->pixels[1];
        data[2] = overlay->pixels[2];
        
        int strides[3] = {0};
        strides[0] = overlay->pitches[0];
        strides[1] = overlay->pitches[1];
        strides[2] = overlay->pitches[2];
        
        if (vout->opaque->on_raw_image_available(vout->opaque->mp, data, strides, overlay->w, overlay->h, overlay->format, overlay->rotate)) {
            SDL_CondWaitTimeout(vout->con, vout->mutex, 2);
        }
        
    } else {
        vout->opaque->gl_render.target_overlay = overlay;
        if (vout->opaque->post_redraw_msg_func(vout->opaque->mp)) {
            SDL_CondWaitTimeout(vout->con, vout->mutex, 100);
        }    
    }
    return 0;
}

static int voud_display_overlay_l(SDL_Vout *vout, SDL_VoutOverlay *overlay)
{
    SDL_Vout_Opaque *opaque = vout->opaque;
    ANativeWindow *native_window = opaque->native_window;
//
//    if (!native_window) {
//        ALOGE("voud_display_overlay_l: NULL native_window");
//        return -1;
//    }
//
//    if (!overlay) {
//        ALOGE("voud_display_overlay_l: NULL overlay");
//        return -1;
//    }
//
//    if (overlay->w <= 0 || overlay->h <= 0) {
//        ALOGE("voud_display_overlay_l: invalid overlay dimensions(%d, %d)", overlay->w, overlay->h);
//        return -1;
//    }

    switch(overlay->format) {
    case SDL_FCC__AMC:
        if(vout->panorama == 1)
           IJK_EGL_terminate(opaque->egl);
        if(vout->opaque->enable_decoder_out_raw_data == 1) {
            return post_redraw(vout, overlay);
        } else{
            return SDL_VoutOverlayAMediaCodec_releaseFrame(overlay, vout, true);
        }
       // ALOGD("release out put buffer");
        //return ret;
    case SDL_FCC_I420:
       // return sdl_android_display_I420_l(&vout->opaque->gl_render, overlay);
       //  post_redraw(vout, overlay);
        if(vout->panorama == 1)
        {
            if(opaque->egl)
            {
               IJK_EGL_display(opaque->egl, native_window, overlay);
               return 0;
            }
        }
        else
            return post_redraw(vout, overlay);
         break;
    default:
        return SDL_Android_NativeWindow_display_l(native_window, overlay);
    }
    if(vout->panorama == 1)
        IJK_EGL_terminate(opaque->egl);
       return SDL_Android_NativeWindow_display_l(native_window, overlay);
}

static int voud_display_overlay(SDL_Vout *vout, SDL_VoutOverlay *overlay)
{
    SDL_LockMutex(vout->mutex);
    int retval = voud_display_overlay_l(vout, overlay);
    SDL_UnlockMutex(vout->mutex);
    return retval;
}

static void voud_set_decode_raw_data_enable(SDL_Vout *vout, int enable)
{
    SDL_LockMutex(vout->mutex);
    vout->opaque->enable_decoder_out_raw_data = enable;
    ALOGI("[raw] voud decode raw data %d", enable);
    SDL_UnlockMutex(vout->mutex);
}

static int voud_get_decode_raw_data_enable(SDL_Vout *vout)
{
    return vout->opaque->enable_decoder_out_raw_data;
}

static SDL_Class g_nativewindow_class = {
    .name = "ANativeWindow_Vout",
};

static void vout_on_glsurfaceview_created(SDL_GLSurfaceViewRender *render) {
    sdl_android_on_glsurfaceview_created(render);
}

static void vout_on_glsurfaceview_changed(SDL_GLSurfaceViewRender *render, int width, int height)
{
    sdl_android_on_glsurfaceview_changed(render, width, height);
}

static int vout_on_bind_frame_buffer(SDL_Vout *vout, SDL_VoutOverlay *overlay)
{
    if (vout && vout->opaque && vout->opaque->gl_frame_callback) {
        return vout->opaque->gl_frame_callback(vout->opaque->mp, GL_FRAME_EVENT_ON_BIND_BUFFER);
    }
    return 0;
}

static int vout_on_pre_render_buffer(SDL_Vout *vout, SDL_VoutOverlay *overlay)
{
    if (vout && vout->opaque && vout->opaque->gl_frame_callback) {
        return vout->opaque->gl_frame_callback(vout->opaque->mp, GL_FRAME_EVENT_ON_PRE_RENDER_BUFFER);
    }
    return 0;
}

static int vout_on_post_render_buffer(SDL_Vout *vout, SDL_VoutOverlay *overlay)
{
    if (vout && vout->opaque && vout->opaque->gl_frame_callback) {
        return vout->opaque->gl_frame_callback(vout->opaque->mp, GL_FRAME_EVENT_ON_POST_RENDER_BUFFER);
    }
    return 0;
}

SDL_Vout *SDL_VoutAndroid_CreateForANativeWindow(void *mp, bool(*post_redraw_msg)(void*), bool(*OnRawImageAvailable)(void *arg, unsigned char**, int*, int, int, int, int), int nPanorama)
{
    SDL_Vout *vout = SDL_Vout_CreateInternal(sizeof(SDL_Vout_Opaque));
    if (!vout)
        return NULL;

    SDL_Vout_Opaque *opaque = vout->opaque;
    opaque->gl_render.scale_mode = ScalingAspectFill;
    opaque->gl_render.vertices[0] = -1.0f;
    opaque->gl_render.vertices[1] = -1.0f;
    opaque->gl_render.vertices[2] =  1.0f;
    opaque->gl_render.vertices[3] = -1.0f;
    opaque->gl_render.vertices[4] = -1.0f;
    opaque->gl_render.vertices[5] =  1.0f;
    opaque->gl_render.vertices[6] =  1.0f;
    opaque->gl_render.vertices[7] =  1.0f;

    opaque->gl_render.texCoords[0] = 0.0f;
    opaque->gl_render.texCoords[1] = 1.0f;
    opaque->gl_render.texCoords[2] = 1.0f;
    opaque->gl_render.texCoords[3] = 1.0f;
    opaque->gl_render.texCoords[4] = 0.0f;
    opaque->gl_render.texCoords[5] = 0.0f;
    opaque->gl_render.texCoords[6] = 1.0f;
    opaque->gl_render.texCoords[7] = 0.0f;
    
    opaque->enable_decoder_out_raw_data = 0;
    opaque->native_window = NULL;
    opaque->mp = mp;
    opaque->post_redraw_msg_func = post_redraw_msg;
    opaque->on_raw_image_available = OnRawImageAvailable;

    vout->opaque_class    = &g_nativewindow_class;
    vout->create_overlay  = vout_create_overlay;
    vout->free_l          = vout_free_l;
    vout->display_overlay = voud_display_overlay;
    vout->set_decode_raw_data_enable = voud_set_decode_raw_data_enable;
    vout->get_decode_raw_data_enable = voud_get_decode_raw_data_enable;
    vout->on_bind_frame_buffer = vout_on_bind_frame_buffer;
    vout->on_pre_render_frame = vout_on_pre_render_buffer;
    vout->on_post_render_frame = vout_on_post_render_buffer;
    vout->panorama = nPanorama;
    vout->useLibYuv = 0;
    if(vout->nYTexture == 0)
    {
        int nTexture;
        glGenTextures(1, &nTexture);
        vout->nYTexture = nTexture;
    }

    ALOGI("===================player texture is=============%d", vout->nYTexture);
    if(nPanorama == 1)
        opaque->egl = IJK_EGL_create();

    return vout;
}

static void SDL_VoutAndroid_SetNativeWindow_l(SDL_Vout *vout, ANativeWindow *native_window)
{
    SDL_Vout_Opaque *opaque = vout->opaque;

    if (opaque->native_window == native_window)
        return;

    if (opaque->native_window)
        ANativeWindow_release(opaque->native_window);

    if (native_window)
        ANativeWindow_acquire(native_window);

    opaque->native_window = native_window;
}

void SDL_VoutAndroid_SetNativeWindow(SDL_Vout *vout, ANativeWindow *native_window)
{
    SDL_LockMutex(vout->mutex);
    SDL_VoutAndroid_SetNativeWindow_l(vout, native_window);
    SDL_UnlockMutex(vout->mutex);
}

void SDL_VoutAndroid_setAMediaCodecWithoutLock(SDL_Vout *vout, SDL_AMediaCodec *acodec)
{

    SDL_Vout_Opaque *opaque = vout->opaque;
    opaque->weak_acodec = acodec;

}

void SDL_VoutAndroid_setAMediaCodec(SDL_Vout *vout, SDL_AMediaCodec *acodec)
{
    SDL_LockMutex(vout->mutex);
    SDL_Vout_Opaque *opaque = vout->opaque;
    opaque->weak_acodec = acodec;
    SDL_UnlockMutex(vout->mutex);
}

SDL_AMediaCodec *SDL_VoutAndroid_peekAMediaCodec(SDL_Vout *vout)
{
    SDL_Vout_Opaque *opaque = vout->opaque;
    SDL_AMediaCodec *acodec = NULL;

    SDL_LockMutex(vout->mutex);
    acodec = opaque->weak_acodec;
    //ALOGE("SDL_VoutAndroid_peekAMediaCodec == %d ",acodec==NULL );
    SDL_UnlockMutex(vout->mutex);
    return acodec;
}

void SDL_VoutAndroid_SetGLFrameCallback(SDL_Vout *vout, int (*gl_frame_callback)(void*, int))
{
    SDL_LockMutex(vout->mutex);
    SDL_Vout_Opaque *opaque = vout->opaque;
    opaque->gl_frame_callback = gl_frame_callback;
    SDL_UnlockMutex(vout->mutex);
}

void SDL_VoutAndroid_OnGLSurfaceViewCreated(SDL_Vout *vout)
{
    SDL_LockMutex(vout->mutex);
    SDL_Vout_Opaque *opaque = vout->opaque;
    vout_on_glsurfaceview_created(&opaque->gl_render);
    SDL_UnlockMutex(vout->mutex);
}

void SDL_VoutAndroid_OnGLSurfaceViewChanged(SDL_Vout *vout, int width, int height)
{
    SDL_LockMutex(vout->mutex);
    SDL_Vout_Opaque *opaque = vout->opaque;
    vout_on_glsurfaceview_changed(&opaque->gl_render, width, height);
    SDL_UnlockMutex(vout->mutex);
}

static SDL_AMediaCodecBufferProxy *SDL_VoutAndroid_obtainBufferProxy_l(SDL_Vout *vout, int buffer_index, SDL_AMediaCodecBufferInfo *buffer_info)
{
    //SDL_Vout_Opaque *opaque = vout->opaque;
    SDL_AMediaCodecBufferProxy *proxy = NULL;

    proxy=(SDL_AMediaCodecBufferProxy *)mallocz(sizeof(SDL_AMediaCodecBufferProxy));
    if(!proxy)
        return NULL;
    proxy->buffer_index  = buffer_index;
    proxy->buffer_info   = *buffer_info;

    return proxy;
}

SDL_AMediaCodecBufferProxy *SDL_VoutAndroid_obtainBufferProxy(SDL_Vout *vout, int buffer_index, SDL_AMediaCodecBufferInfo *buffer_info)
{
    SDL_AMediaCodecBufferProxy *proxy = NULL;
    SDL_LockMutex(vout->mutex);
    proxy = SDL_VoutAndroid_obtainBufferProxy_l(vout, buffer_index, buffer_info);
    SDL_UnlockMutex(vout->mutex);
    return proxy;
}

void SDL_VoutAndroid_redraw(SDL_Vout *vout)
{
    SDL_LockMutex(vout->mutex);
    SDL_Vout_Opaque *opaque = vout->opaque;
    if (opaque) {
        SDL_VoutOverlay *overlay = opaque->gl_render.target_overlay;
        if (overlay) {
            sdl_android_display_I420_l(vout, &opaque->gl_render);
            opaque->gl_render.target_overlay = NULL;
            SDL_CondSignal(vout->con);
        } else {
            vout->on_bind_frame_buffer(vout, NULL);
            vout->on_pre_render_frame(vout, NULL);
            vout->on_post_render_frame(vout, NULL);
            opaque->gl_render.target_overlay = NULL;
            SDL_CondSignal(vout->con);
        }
    }
    SDL_UnlockMutex(vout->mutex);
}

void SDL_VoutAndroid_redrawTexture(SDL_Vout *vout, int width, int height)
{
    SDL_LockMutex(vout->mutex);
    SDL_Vout_Opaque *opaque = vout->opaque;
    if (opaque) {
        SDL_VoutOverlay *overlay = opaque->gl_render.target_overlay;
        if (overlay) {
            sdl_android_display_I420_l_hw(vout, &opaque->gl_render, width, height);
            opaque->gl_render.target_overlay = NULL;
            SDL_CondSignal(vout->con);
        } else {
            vout->on_bind_frame_buffer(vout, NULL);
            vout->on_pre_render_frame(vout, NULL);
            vout->on_post_render_frame(vout, NULL);
            opaque->gl_render.target_overlay = NULL;
            SDL_CondSignal(vout->con);
        }
    }
    SDL_UnlockMutex(vout->mutex);
}

static int SDL_VoutAndroid_releaseBufferProxy_l(SDL_AMediaCodecBufferProxy * proxy, SDL_AMediaCodec *acodec, bool render)
{

    if (!proxy)
        return 0;

    if (proxy->buffer_index < 0) {
        ALOGE(" buffer index is not right : %d ",proxy->buffer_index);
        return 0;
    } else if (proxy->buffer_info.flags & AMEDIACODEC__BUFFER_FLAG_FAKE_FRAME) {
        return 0;
    }
    av_free(proxy->buffer_info.buffer_change_format_ptr);
    sdl_amedia_status_t amc_ret = SDL_AMediaCodec_releaseOutputBuffer(acodec, proxy->buffer_index, render);

    if (amc_ret != 0) {
        return -1;
    }
    return 0;
}

int SDL_VoutAndroid_releaseBufferProxy_Vout_Wihtoutlock(SDL_AMediaCodecBufferProxy ** proxy, SDL_Vout *vout, bool render)
{
    int ret = 0;
    SDL_AMediaCodec *acodec =vout->opaque->weak_acodec;
    if(acodec){
        ret = SDL_VoutAndroid_releaseBufferProxy_l(*proxy, acodec, render);
    }
    freep(proxy);
    return ret;
}


int SDL_VoutAndroid_releaseBufferProxy_Vout(SDL_AMediaCodecBufferProxy ** proxy, SDL_Vout *vout, bool render)
{
    int ret = 0;

    SDL_LockMutex(vout->mutex);

    SDL_AMediaCodec *acodec =vout->opaque->weak_acodec;

    if(acodec==NULL){
        SDL_UnlockMutex(vout->mutex);
        return -1;
    }

    ret = SDL_VoutAndroid_releaseBufferProxy_l(*proxy, acodec, render);
    freep(proxy);
    SDL_UnlockMutex(vout->mutex);


    return ret;
}

