//
// Created by 宋怡君 on 1/10/21.
//

#ifndef FFPLAY__FF_CACHE_H
#define FFPLAY__FF_CACHE_H

#define SAVE_SUCESS                        0
#define SAVE_ERROR_FOR_UNFINISH            -1
#define SAVE_ERROR_FOR_SEEK                -2
#define SAVE_ERROR_FOR_INIT_CACHE          -3
#define SAVE_ERROR_FOR_CACHE_FILE          -4

#include "ff_ffplay_def.h"
#include "ff_fferror.h"

int ffp_init_save(FFPlayer *ffp);

void ffp_stop_save(FFPlayer *ffp, int state);

int ffp_save_file(FFPlayer *ffp, AVPacket *packet);

#endif