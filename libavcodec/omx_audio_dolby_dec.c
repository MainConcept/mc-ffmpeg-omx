/*
 * OMX Audio Dolby decoder
 * Copyright (c) 2024 MainConcept GmbH or its affiliates.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "config.h"

#ifdef _WIN32
#include "compat/w32dlfcn.h"
#else
#include <dlfcn.h>
#endif

#include "libavutil/internal.h"
#include "libavutil/avstring.h"
#include "libavutil/avutil.h"
#include "libavutil/common.h"
#include "libavutil/log.h"
#include "libavutil/opt.h"
#include "codec_internal.h"

#include "decode.h"
#include "avcodec.h"
#include "internal.h"
#include "omx_common.h"

static int fill_next_input_buffer(AVCodecContext *avctx, OMX_BUFFERHEADERTYPE* buf)
{
    AVPacket pkt = {0};
    int ret = ff_decode_get_packet(avctx, &pkt);

    buf->nFlags = 0;

    if (ret < 0 || pkt.size == 0) {
        av_packet_unref(&pkt);
        buf->nFlags |= ret == AVERROR_EOF ? OMX_BUFFERFLAG_EOS : 0;
        return ret < 0 ? ret : AVERROR_INVALIDDATA; // INVALIDDATA when pkt.size == 0 it was taken from binkaudio.c
    }

    memcpy(buf->pBuffer, pkt.data, pkt.size);
    buf->nFilledLen = (OMX_U32)pkt.size;
    buf->nTimeStamp = to_omx_ticks(pkt.pts);

    av_packet_unref(&pkt);

    return 0;
}

const int omx_channel_to_ffmpeg[] = { 0, AV_CH_FRONT_LEFT, AV_CH_FRONT_RIGHT, AV_CH_FRONT_CENTER, AV_CH_SIDE_LEFT, AV_CH_SIDE_RIGHT, AV_CH_LOW_FREQUENCY, AV_CH_BACK_CENTER, AV_CH_BACK_LEFT, AV_CH_BACK_RIGHT};

static int audio_format(int bps, int data_type)
{
    if (data_type == OMX_NumericalDataSigned)
        switch (bps) {
            case 16 : return AV_SAMPLE_FMT_S16;
            case 32 : return AV_SAMPLE_FMT_S32;
            default : return AV_SAMPLE_FMT_NONE;
        }
    else if (data_type == OMX_NumericalDataFloat)
        switch (bps) {
            case 32 : return AV_SAMPLE_FMT_FLT;
            case 64 : return AV_SAMPLE_FMT_DBL;
            default : return AV_SAMPLE_FMT_NONE;
        }

    return AV_SAMPLE_FMT_NONE;
}

static void update_context(AVCodecContext *avctx, const OMX_AUDIO_PARAM_PCMMODETYPE *audio_param)
{

    av_channel_layout_default(&avctx->ch_layout, audio_param->nChannels);

    avctx->sample_fmt       = audio_format(audio_param->nBitPerSample, audio_param->eNumData);
    avctx->sample_rate      = audio_param->nSamplingRate;
}

static int buffer_to_frame(AVCodecContext *avctx, AVFrame *fr, OMX_BUFFERHEADERTYPE* buf)
{
    OMXComponentContext *s = avctx->priv_data;
    int ret = 0;

    OMX_AUDIO_PARAM_PCMMODETYPE audio_param;

    static const AVRational Microseconds = {1, 1000000};
    int64_t pts;

    INIT_STRUCT(audio_param);
    audio_param.nPortIndex = OMX_ALL;
    OMX_GetParameter(s->component, OMX_IndexParamAudioPcm, &audio_param);

    update_context(avctx, &audio_param);

    fr->ch_layout       = avctx->ch_layout;

    fr->format          = avctx->sample_fmt;
    fr->sample_rate     = avctx->sample_rate;

    fr->nb_samples = (int)buf->nFilledLen / (audio_param.nBitPerSample / 8) / audio_param.nChannels;
    avctx->frame_size = buf->nFilledLen / (audio_param.nBitPerSample / 8) / audio_param.nChannels;

    if ((ret = ff_get_buffer(avctx, fr, 0)) < 0)
        return ret;

    memcpy(fr->extended_data[0], buf->pBuffer, buf->nFilledLen);

    fr->best_effort_timestamp = AV_NOPTS_VALUE;
    pts = av_rescale_q(from_omx_ticks(buf->nTimeStamp), Microseconds, avctx->pkt_timebase); // TODO
    fr->pkt_dts = pts;
    fr->pts     = pts;

    return 0;
}

static int omx_receive_frame(AVCodecContext *avctx, AVFrame *frame)
{
    OMXComponentContext *s = avctx->priv_data;
    int ret = 0;
    OMX_BUFFERHEADERTYPE* in_buf  = NULL;
    OMX_BUFFERHEADERTYPE* out_buf = NULL;

    if (s->eos_flag)
        return AVERROR_EOF;

    in_buf = av_omx_pick_input_buffer(s);

    if (!in_buf) {
        // If there is no input buffer, we should wait for output one, because we can't return EAGAIN here (see line 2243 ffmpeg.c)
        ret = av_omx_wait_any_buffer(s, &out_buf, &in_buf);
        if (ret == AVERROR(EINVAL))
            return ret;

        if (out_buf) { // Frame is ready, so just returns immediately
            ret = buffer_to_frame(avctx, frame, out_buf);

            s->eos_flag = out_buf->nFlags & OMX_BUFFERFLAG_EOS ? 1 : 0; // out_buf->nFlags should be checked before FillThisBuffer is called

            out_buf->nFilledLen = 0;
            OMX_FillThisBuffer(s->component, out_buf);

            return ret;
        }

        assert(in_buf);
        assert(!out_buf);
    }

    ret = fill_next_input_buffer(avctx, in_buf);
    OMX_EmptyThisBuffer(s->component, in_buf);

    if (ret != AVERROR_EOF && ret < 0)
        return ret;

    out_buf = av_omx_pick_output_buffer(s);

    if (in_buf->nFlags & OMX_BUFFERFLAG_EOS && !out_buf) // If EOS was set on input buffer, FFMPEG flushes the codec. In this case codec cannot return EAGAIN as there will be no more input data. So, EAGAIN will cause FFMPEG to report decoding error and stop
        out_buf = av_omx_wait_output_buffer(s);

    if (out_buf) {

        if (out_buf->nFilledLen)
            ret = buffer_to_frame(avctx, frame, out_buf);

        s->eos_flag = out_buf->nFlags & OMX_BUFFERFLAG_EOS ? 1 : 0;

        out_buf->nFilledLen = 0;
        OMX_FillThisBuffer(s->component, out_buf);

        return ret;
    }

    return AVERROR(EAGAIN);
}

static av_cold int omx_decoder_init( AVCodecContext *avctx )
{
    OMXComponentContext *s = avctx->priv_data;
    int ret = 0;

    s->avctx = avctx;
    avctx->frame_size = 1024;

    ret = av_omx_cmpnt_init(s);
    if (ret) return ret;

    av_omx_set_commandline(s);

    return av_omx_cmpnt_start(s);
}

static const AVClass omx_dolby_decoder_class = {
        .class_name = "omx_dec_ddpp",
        .item_name  = av_default_item_name,
        .option     = av_omx_options,
        .version    = LIBAVUTIL_VERSION_INT,
};

FFCodec ff_dolby_omx_decoder = {
    .p.name            = "omx_dec_ddpp",
    .p.long_name       = NULL_IF_CONFIG_SMALL("OpenMAX IL Dolby Decoder"),
    .p.type            = AVMEDIA_TYPE_AUDIO,
    .p.id              = AV_CODEC_ID_AC3,
    .priv_data_size    = sizeof(OMXComponentContext),
    .init              = omx_decoder_init,
    .close             = omx_cmpnt_codec_end,
    .cb_type           = FF_CODEC_CB_TYPE_RECEIVE_FRAME,
    .cb.receive_frame  = omx_receive_frame,
    .p.capabilities    = AV_CODEC_CAP_VARIABLE_FRAME_SIZE | AV_CODEC_CAP_CHANNEL_CONF | AV_CODEC_CAP_DELAY | AV_CODEC_CAP_DR1,
    .p.priv_class      = &omx_dolby_decoder_class,
};
