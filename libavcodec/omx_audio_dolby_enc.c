/*
 * OMX Audio Dolby encoder
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

#include "libavutil/thread.h"

#include <stdio.h>
#include <stdlib.h>

#include "libavutil/internal.h"
#include "libavutil/avstring.h"
#include "libavutil/avutil.h"
#include "libavutil/common.h"
#include "libavutil/imgutils.h"
#include "libavutil/log.h"
#include "libavutil/opt.h"
#include "codec_internal.h"

#include "decode.h"
#include "avcodec.h"
#include "internal.h"
#include "omx_common.h"
#include "omx_audio_enc_common.h"

static int omx_set_audio_ddp_param(AVCodecContext *avctx)
{
    OMXComponentContext *s = avctx->priv_data;

    OMX_AUDIO_PARAM_DDPTYPE audio_param_ddpp;
    INIT_STRUCT(audio_param_ddpp);
    audio_param_ddpp.nPortIndex = OMX_ALL;
    audio_param_ddpp.nChannels = avctx->ch_layout.nb_channels;
    audio_param_ddpp.nBitRate = avctx->bit_rate;
    audio_param_ddpp.nSampleRate = avctx->sample_rate;
    omx_cvt_channels_to_channel_mapping(audio_param_ddpp.nChannels, audio_param_ddpp.eChannelMapping);

    return OMX_SetParameter(s->component, OMX_IndexParamAudioDdp, &audio_param_ddpp);
}

static av_cold int omx_cmpnt_encoder_init(AVCodecContext *avctx)
{
    OMXComponentContext *s = avctx->priv_data;
    int ret = 0;

    s->avctx = avctx;

    ret = av_omx_cmpnt_init(s);
    if (ret) return ret;

    ret = omx_set_audio_pcm_param(avctx);
    if (ret) return ret;

    ret = omx_set_audio_ddp_param(avctx);
    if (ret) return ret;

    ret = av_omx_set_commandline(s);
    if (ret) return ret;

    return av_omx_cmpnt_start(s);
}

static const FFCodecDefault ddpp_enc_omx_defaults[] = {
        { "ab",                "0" },
        { NULL },
};

static const AVClass omx_dolby_encoder_class = {
        .class_name = "omx_enc_ddpp",
        .item_name  = av_default_item_name,
        .option     = av_omx_options,
        .version    = LIBAVUTIL_VERSION_INT,
};

FFCodec ff_dolby_omx_encoder = {
        .p.name             = "omx_enc_ddpp",
        .p.long_name        = NULL_IF_CONFIG_SMALL("OpenMAX IL Dolby Encoder"),
        .p.type             = AVMEDIA_TYPE_AUDIO,
        .p.id               = AV_CODEC_ID_EAC3,
        .priv_data_size     = sizeof(OMXComponentContext),
        .init               = omx_cmpnt_encoder_init,
        .close              = omx_cmpnt_codec_end,
        .cb_type            = FF_CODEC_CB_TYPE_RECEIVE_PACKET,
        .cb.receive_packet  = omx_receive_packet,
        .p.capabilities     = AV_CODEC_CAP_VARIABLE_FRAME_SIZE | AV_CODEC_CAP_DELAY | AV_CODEC_CAP_DR1,
        .defaults           = ddpp_enc_omx_defaults,
        .p.priv_class       = &omx_dolby_encoder_class,
        .p.sample_fmts      = (const enum AVSampleFormat[]){ AV_SAMPLE_FMT_S16,
                                                             AV_SAMPLE_FMT_S32,
                                                             AV_SAMPLE_FMT_FLT,
                                                             AV_SAMPLE_FMT_DBL,
                                                             AV_SAMPLE_FMT_NONE },
};
