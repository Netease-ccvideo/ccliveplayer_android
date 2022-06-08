//
// Created by 宋怡君 on 1/10/21.
//

#include "ff_file_saver.h"

extern void IjkMediaPlayer_sendFFplayerState(void* weak_thiz, int event, int state, const char *info);

int ffp_init_save(FFPlayer *ffp)
{
    VideoState *is = ffp->is;
    FileSaver *saver = ffp->saver;
    int ret = 0;
    saver->m_ofmt_ctx = NULL;
    saver->m_ofmt = NULL;
    saver->is_save = 0;
    saver->save_state = 0;
    saver->videosave_index_out = -1;
    saver->audiosave_index_out = -1;
    avformat_alloc_output_context2(&saver->m_ofmt_ctx, NULL, NULL, saver->save_path);
    if (!saver->m_ofmt_ctx) {
        ALOGE("[file_saver] Could not create output context filename is %s\n", saver->save_path);
        return -1;
    }
    saver->m_ofmt = saver->m_ofmt_ctx->oformat;
    for (int i = 0; i < is->ic->nb_streams; i++) {
        if (i == saver->videosave_index || i == saver->audiosave_index) {
            AVStream *in_stream = is->ic->streams[i];
            AVCodecParameters *codecpar = in_stream->codecpar;
            enum AVMediaType mediaType = codecpar->codec_type;

            AVCodec *dec = avcodec_find_decoder(codecpar->codec_id);
            AVStream *out_stream = avformat_new_stream(saver->m_ofmt_ctx, dec);
            if (!out_stream) {
                ALOGE("[file_saver] Could not allocate output stream\n");
                return -1;
            }
            if (i == saver->videosave_index) {
                saver->videosave_index_out = out_stream->index;
            } else if (i == saver->audiosave_index) {
                saver->audiosave_index_out = out_stream->index;
            } else {

            }
            codecpar->codec_tag = 0;
            AVCodecContext *context = avcodec_alloc_context3(dec);
            avcodec_parameters_to_context(context, codecpar);
            out_stream->codec = context;
        } else {
            continue;
        }
    }

    av_dump_format(saver->m_ofmt_ctx, 0, saver->save_path, 1);

    if (!(saver->m_ofmt->flags & AVFMT_NOFILE)) {
        if (ret = avio_open(&saver->m_ofmt_ctx->pb, saver->save_path, AVIO_FLAG_WRITE) < 0) {
            ALOGE("[file_saver] Could not open output file '%s' (%d)", saver->save_path, ret);
            return ret;
        }
    }
    if (ret = avformat_write_header(saver->m_ofmt_ctx, NULL) < 0) {
        ALOGE("[file_saver] Could not write header (%d)", ret);
        return ret;
    }
    saver->is_save = 1;
    return ret;
}

void ffp_stop_save(FFPlayer *ffp, int state)
{
    assert(ffp);
    FileSaver *saver = ffp->saver;
    saver->is_save = 0;
    if (saver->m_ofmt_ctx != NULL) {
        av_write_trailer(saver->m_ofmt_ctx);
        if (saver->m_ofmt_ctx && !(saver->m_ofmt->flags & AVFMT_NOFILE)) {
            avio_close(saver->m_ofmt_ctx->pb);
        }
        avformat_free_context(saver->m_ofmt_ctx);
    }
    if(state != 0) {
        saver->save_state = state;
        if(remove(saver->save_path)!=0)
            saver->save_state = -5;
    }
    //考虑ffplayer stop时也需给出file_saver的结果，因此直接抛出
    IjkMediaPlayer_sendFFplayerState(ffp->weak_thiz, FFP_EVENT_SAVE_STATE, saver->save_state, saver->save_path);
    av_free(ffp->saver);
    ffp->saver = NULL;
}

int ffp_save_file(FFPlayer* ffp, AVPacket* packet)
{
    assert(ffp);
    VideoState* is = ffp->is;
    FileSaver *saver = ffp->saver;
    int ret = 0;
    int stream_index = 0;
    AVStream* in_stream;
    AVStream* out_stream;

    int pkt_index = packet->stream_index;
    if (pkt_index == saver->videosave_index || pkt_index == saver->audiosave_index) {
        if (saver->is_save) {
            AVPacket* pkt = (AVPacket*)av_malloc(sizeof(AVPacket));
            av_new_packet(pkt, 0);
            if (0 == av_packet_ref(pkt, packet)) {
                in_stream = is->ic->streams[pkt->stream_index];
                if (pkt->stream_index == saver->videosave_index) {
                    out_stream = saver->m_ofmt_ctx->streams[saver->videosave_index_out];
                    stream_index = saver->videosave_index_out;
                }
                else {
                    out_stream = saver->m_ofmt_ctx->streams[saver->audiosave_index_out];
                    stream_index=saver->audiosave_index_out;
                }
                pkt->pts = av_rescale_q(pkt->pts, in_stream->time_base, out_stream->time_base);
                pkt->dts = av_rescale_q(pkt->dts, in_stream->time_base, out_stream->time_base);
                pkt->duration = av_rescale_q(pkt->duration, in_stream->time_base, out_stream->time_base);
                pkt->pos = -1;
                pkt->stream_index = stream_index;
                if (ret = av_interleaved_write_frame(saver->m_ofmt_ctx, pkt) < 0) {
                    ALOGE("[file_saver] Cound not write packet ( %d )\n", ret);
                }
                av_packet_unref(pkt);
            }
            else {
                ALOGE("[file_saver] av_packet_ref == NULL");
            }
        }
    }
    return ret;
}