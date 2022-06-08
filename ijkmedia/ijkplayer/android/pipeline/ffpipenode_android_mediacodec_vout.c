/*
 * ffpipenode_android_mediacodec_vout.c
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

#include "ffpipenode_android_mediacodec_vout.h"
#include "../../ff_ffpipenode.h"
#include "../../ff_ffplay.h"

typedef struct IJKFF_Pipenode_Opaque {
    FFPlayer *ffp;
} IJKFF_Pipenode_Opaque;

static void func_destroy(IJKFF_Pipenode *node)
{
    // do nothing
}

static int func_run_sync(IJKFF_Pipenode *node)
{
    //IJKFF_Pipenode_Opaque *opaque = node->opaque;
    // do nothing not use;
    return -1;
}

IJKFF_Pipenode *ffpipenode_create_video_output_from_android_mediacodec(FFPlayer *ffp)
{
    ALOGD("ffpipenode_create_video_output_from_android_mediacodec()\n");
    IJKFF_Pipenode *node = ffpipenode_alloc(sizeof(IJKFF_Pipenode_Opaque));
    if (!node)
        return node;

    IJKFF_Pipenode_Opaque *opaque = node->opaque;
    opaque->ffp         = ffp;

    node->func_destroy  = func_destroy;
    node->func_run_sync = func_run_sync;

    return node;
}
