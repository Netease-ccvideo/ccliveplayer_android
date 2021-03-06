/*
 * ffpipeline_android.c
 *
 * Copyright (c) 2014 Zhang Rui <bbcallen@gmail.com>
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

#include "ffpipeline_android.h"
#include <jni.h>
#include "ffpipenode_android_mediacodec_vdec.h"
#include "ffpipenode_android_mediacodec_vout.h"
#include "../../pipeline/ffpipenode_ffplay_vdec.h"
#include "../../ff_ffplay.h"
#include "ijksdl/android/ijksdl_android_jni.h"
#include <time.h>
static SDL_Class g_pipeline_class = {
    .name = "ffpipeline_android_media",
};


static void func_destroy(IJKFF_Pipeline *pipeline)
{
    IJKFF_Pipeline_Opaque *opaque = pipeline->opaque;
    JNIEnv *env = NULL;

    SDL_DestroyMutexP(&opaque->surface_mutex);

    if (JNI_OK != SDL_JNI_SetupThreadEnv(&env)) {
        ALOGE("amediacodec-pipeline:destroy: SetupThreadEnv failed\n");
        goto fail;
    }

    SDL_JNI_DeleteGlobalRefP(env, &opaque->jsurface);
    SDL_JNI_DeleteGlobalRefP(env, &opaque->mediacodec_select_callback_opaque);
    SDL_JNI_DeleteGlobalRefP(env, &opaque->video_docoder_select_opaque);
fail:
    return;
}

static IJKFF_Pipenode *func_open_video_decoder(IJKFF_Pipeline *pipeline, FFPlayer *ffp)
{
    IJKFF_Pipeline_Opaque *opaque = pipeline->opaque;
    IJKFF_Pipenode        *node = NULL;
    if (opaque->mediacodec_enabled)
        node = ffpipenode_create_video_decoder_from_android_mediacodec(ffp, pipeline, opaque->weak_vout);
    if (!node)
        node = ffpipenode_create_video_decoder_from_ffplay(ffp);
    ffpipeline_onselect_videodecoder(pipeline,ffp->video_codec_info,opaque->weak_vout->get_decode_raw_data_enable(opaque->weak_vout));
    return node;
}


inline static bool check_ffpipeline(IJKFF_Pipeline* pipeline, const char *func_name)
{
    if (!pipeline || !pipeline->opaque || !pipeline->opaque_class) {
        ALOGE("%s.%s: invalid pipeline\n", pipeline->opaque_class->name, func_name);
        return false;
    }

    if (pipeline->opaque_class != &g_pipeline_class) {
        ALOGE("%s.%s: unsupported method\n", pipeline->opaque_class->name, func_name);
        return false;
    }

    return true;
}

IJKFF_Pipeline *ffpipeline_create_from_android(FFPlayer *ffp)
{
    ALOGD("ffpipeline_create_from_android()\n");
    IJKFF_Pipeline *pipeline = ffpipeline_alloc(&g_pipeline_class, sizeof(IJKFF_Pipeline_Opaque));
    if (!pipeline)
        return pipeline;

    IJKFF_Pipeline_Opaque *opaque = pipeline->opaque;
    opaque->ffp                   = ffp;
    opaque->surface_mutex         = SDL_CreateMutex();
    if (!opaque->surface_mutex) {
        ALOGE("ffpipeline-android:create SDL_CreateMutex failed\n");
        goto fail;
    }

    pipeline->func_destroy            = func_destroy;
    pipeline->func_open_video_decoder = func_open_video_decoder;
    //pipeline->func_open_video_output  = func_open_video_output;

    return pipeline;
fail:
    ffpipeline_free_p(&pipeline);
    return NULL;
}

jobject ffpipeline_get_surface_as_global_ref(JNIEnv *env, IJKFF_Pipeline* pipeline)
{
    if (!check_ffpipeline(pipeline, __func__))
        return NULL;

    IJKFF_Pipeline_Opaque *opaque = pipeline->opaque;
    if (!opaque->surface_mutex)
        return NULL;

    jobject global_ref = NULL;
    SDL_LockMutex(opaque->surface_mutex);
    {
        if (opaque->jsurface)
            global_ref = (*env)->NewGlobalRef(env, opaque->jsurface);
    }
    SDL_UnlockMutex(opaque->surface_mutex);

    return global_ref;
}

void ffpipeline_set_vout(IJKFF_Pipeline* pipeline, SDL_Vout *vout)
{
    if (!check_ffpipeline(pipeline, __func__))
        return;

    IJKFF_Pipeline_Opaque *opaque = pipeline->opaque;
    opaque->weak_vout = vout;
}

int ffpipeline_set_surface(JNIEnv *env, IJKFF_Pipeline* pipeline, jobject surface)
{
    ALOGD("%s()\n", __func__);
    if (!check_ffpipeline(pipeline, __func__))
        return -1;

    IJKFF_Pipeline_Opaque *opaque = pipeline->opaque;
    if (!opaque->surface_mutex)
        return -1;

    SDL_LockMutex(opaque->surface_mutex);
    {
        jobject prev_surface = opaque->jsurface;

        if ((surface == prev_surface) ||
            (surface && prev_surface && (*env)->IsSameObject(env, surface, prev_surface))) {
            // same object, no need to reconfigure
        } else {
            if (surface) {
                opaque->jsurface = (*env)->NewGlobalRef(env, surface);
            } else {
                opaque->jsurface = NULL;
            }
//
            int64_t  cur = av_gettime_relative();
            if(opaque->ffp&&opaque->ffp->is)
                ALOGI("stream_open--->setsurface time = %lld ms, video packet count = %d\n", (cur - opaque->ffp->is->stream_open_time) / 1000, 11);

            SDL_JNI_DeleteGlobalRefP(env, &prev_surface);
            opaque->is_surface_need_reconfigure = true;
        }
    }

    SDL_UnlockMutex(opaque->surface_mutex);

    return 0;
}

int ffpipeline_lock_surface(IJKFF_Pipeline* pipeline)
{
    IJKFF_Pipeline_Opaque *opaque = pipeline->opaque;
    return SDL_LockMutex(opaque->surface_mutex);
}

int ffpipeline_unlock_surface(IJKFF_Pipeline* pipeline)
{
    IJKFF_Pipeline_Opaque *opaque = pipeline->opaque;
    return SDL_UnlockMutex(opaque->surface_mutex);
}

bool ffpipeline_is_surface_need_reconfigure(IJKFF_Pipeline* pipeline)
{
    if (!check_ffpipeline(pipeline, __func__))
        return false;

    return pipeline->opaque->is_surface_need_reconfigure;
}

void ffpipeline_set_surface_need_reconfigure(IJKFF_Pipeline* pipeline, bool need_reconfigure)
{
    ALOGD("%s(%d)\n", __func__, (int)need_reconfigure);
    if (!check_ffpipeline(pipeline, __func__))
        return;

    pipeline->opaque->is_surface_need_reconfigure = need_reconfigure;
}

void ffpipeline_set_video_select_callback(IJKFF_Pipeline* pipeline, bool (*callback)(void *opaque, const char *type, int rawdata), void *opaque)
{
    ALOGD("%s\n", __func__);
    if (!check_ffpipeline(pipeline, __func__))
        return;

    pipeline->opaque->video_docoder_select       = callback;
    pipeline->opaque->video_docoder_select_opaque= opaque;
}

void ffpipeline_set_mediacodec_select_callback(IJKFF_Pipeline* pipeline, bool (*callback)(void *opaque, ijkmp_mediacodecinfo_context *mcc), void *opaque)
{
    ALOGD("%s\n", __func__);
    if (!check_ffpipeline(pipeline, __func__))
        return;

    pipeline->opaque->mediacodec_select_callback        = callback;
    pipeline->opaque->mediacodec_select_callback_opaque = opaque;
}

void ffpipeline_set_mediacodec_enabled(IJKFF_Pipeline* pipeline, bool enabled)
{
    ALOGD("%s\n", __func__);
    if (!check_ffpipeline(pipeline, __func__))
        return;

    pipeline->opaque->mediacodec_enabled = enabled;
}

bool ffpipeline_select_mediacodec(IJKFF_Pipeline* pipeline, ijkmp_mediacodecinfo_context *mcc)
{
    ALOGD("%s\n", __func__);
    if (!check_ffpipeline(pipeline, __func__))
        return false;

    if (!mcc || !pipeline->opaque->mediacodec_select_callback)
        return false;

    return pipeline->opaque->mediacodec_select_callback(pipeline->opaque->mediacodec_select_callback_opaque, mcc);
}

bool ffpipeline_onselect_videodecoder(IJKFF_Pipeline* pipeline, const char  * type, int rawdata)
{
    ALOGD("%s\n", __func__);
    if (!check_ffpipeline(pipeline, __func__))
        return false;

    if (!type || !pipeline->opaque->video_docoder_select)
        return false;

    return pipeline->opaque->video_docoder_select(pipeline->opaque->video_docoder_select_opaque, type, rawdata);
}


