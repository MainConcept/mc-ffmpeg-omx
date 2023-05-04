/*
 * OMX Audio MPEG-H encoder
 * Copyright (c) 2023 MainConcept GmbH or its affiliates.
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
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

#include "decode.h"
#include "avcodec.h"
#include "internal.h"
#include "omx_common.h"
#include "omx_audio_enc_common.h"

static int omx_set_audio_mpegh_param(AVCodecContext *avctx)
{
    OMXComponentContext *s = avctx->priv_data;

    // Only bitrate is used from this structure now.

    OMX_AUDIO_PARAM_AACPROFILETYPE audio_param_aac;
    INIT_STRUCT(audio_param_aac);
    audio_param_aac.nPortIndex = OMX_ALL;
    audio_param_aac.eAACProfile = OMX_AUDIO_AACObjectNull;
    audio_param_aac.nChannels = avctx->channels;
    audio_param_aac.nSampleRate = avctx->sample_rate;
    audio_param_aac.nBitRate = avctx->bit_rate;
    audio_param_aac.eAACStreamFormat = avctx->flags & AV_CODEC_FLAG_GLOBAL_HEADER ? OMX_AUDIO_AACStreamFormatRAW : OMX_AUDIO_AACStreamFormatMP4LATM;
    audio_param_aac.nAudioBandWidth = 0;
    audio_param_aac.nFrameLength = 0;
    audio_param_aac.nAACtools = 0;
    audio_param_aac.nAACERtools = 0;
    audio_param_aac.eChannelMode = OMX_AUDIO_ChannelModeVendorStartUnused;

    return OMX_SetParameter(s->component, OMX_IndexParamAudioAac, &audio_param_aac);
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

    ret = omx_set_audio_mpegh_param(avctx);
    if (ret) return ret;

    ret = av_omx_set_commandline(s);
    if (ret) return ret;

    ret = av_omx_cmpnt_start(s);
    if (ret) return ret;

    return ret;
}

static const AVCodecDefault heaac_enc_omx_defaults[] = {
        { NULL },
};

static const AVClass omx_mpegh_encoder_class = {
        .class_name = "omx_enc_mpegh",
        .item_name  = av_default_item_name,
        .option     = av_omx_options,
        .version    = LIBAVUTIL_VERSION_INT,
};

AVCodec ff_mpegh_omx_encoder = {
        .name             = "omx_enc_mpegh",
        .long_name        = NULL_IF_CONFIG_SMALL("OMX IL MPEG-H Encoder"),
        .type             = AVMEDIA_TYPE_AUDIO,
        .id               = AV_CODEC_ID_MPEGH_3D_AUDIO,
        .priv_data_size   = sizeof(OMXComponentContext),
        .init             = omx_cmpnt_encoder_init,
        .close            = omx_cmpnt_codec_end,
        .receive_packet   = omx_receive_packet,
        .capabilities     = AV_CODEC_CAP_VARIABLE_FRAME_SIZE | AV_CODEC_CAP_DELAY | AV_CODEC_CAP_DR1 | AV_CODEC_CAP_PARAM_CHANGE,
        .defaults         = heaac_enc_omx_defaults,
        .priv_class       = &omx_mpegh_encoder_class,
        .sample_fmts    = (const enum AVSampleFormat[]){ AV_SAMPLE_FMT_S16,
                                                         AV_SAMPLE_FMT_NONE },
};

