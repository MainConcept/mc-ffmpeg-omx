/*
 * OMX Audio encoder
 * Copyright (c) 2021 MainConcept GmbH or its affiliates.
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
#include "omx_video_enc_common.h"

//#define DUMP_INPUT_DATA

static int channels_to_ChannelMapping(int channels, OMX_AUDIO_CHANNELTYPE channelMapping[OMX_AUDIO_MAXCHANNELS])
{
    OMX_AUDIO_CHANNELTYPE res[][8] = {
        { OMX_AUDIO_ChannelCF },
        { OMX_AUDIO_ChannelLF, OMX_AUDIO_ChannelRF },
        { OMX_AUDIO_ChannelLF, OMX_AUDIO_ChannelRF, OMX_AUDIO_ChannelCF },
        { OMX_AUDIO_ChannelLF, OMX_AUDIO_ChannelRF, OMX_AUDIO_ChannelCF, OMX_AUDIO_ChannelLS },
        { OMX_AUDIO_ChannelLF, OMX_AUDIO_ChannelRF, OMX_AUDIO_ChannelCF, OMX_AUDIO_ChannelLS, OMX_AUDIO_ChannelRS },
        { OMX_AUDIO_ChannelLF, OMX_AUDIO_ChannelRF, OMX_AUDIO_ChannelCF, OMX_AUDIO_ChannelLS, OMX_AUDIO_ChannelRS },
        { OMX_AUDIO_ChannelLF, OMX_AUDIO_ChannelRF, OMX_AUDIO_ChannelCF, OMX_AUDIO_ChannelLS, OMX_AUDIO_ChannelRS, OMX_AUDIO_ChannelLR, OMX_AUDIO_ChannelRR },
        { OMX_AUDIO_ChannelLF, OMX_AUDIO_ChannelRF, OMX_AUDIO_ChannelCF, OMX_AUDIO_ChannelLS, OMX_AUDIO_ChannelRS, OMX_AUDIO_ChannelLR, OMX_AUDIO_ChannelRR }
    };

    if (channels >= 1 && channels <= 8) {
        memcpy(channelMapping, res[channels - 1], sizeof(res[channels - 1]));
        return 0;
    }
    return 1;
}

static int sample_fmt_to_omx_index(OMX_AUDIO_PARAM_PCMMODETYPE *audio_param, AVCodecContext *avctx, enum AVSampleFormat sample_fmt)
{
    av_log(avctx, AV_LOG_TRACE, "Input sample format : %x.\n", avctx->sample_fmt);
    switch (sample_fmt) {
        case AV_SAMPLE_FMT_S16 : audio_param->nBitPerSample = 16; audio_param->eNumData = OMX_NumericalDataSigned; return 0;
        case AV_SAMPLE_FMT_S32 : audio_param->nBitPerSample = 32; audio_param->eNumData = OMX_NumericalDataSigned; return 0;
        case AV_SAMPLE_FMT_FLT : audio_param->nBitPerSample = 32; audio_param->eNumData = OMX_NumericalDataFloat; return 0;
        case AV_SAMPLE_FMT_DBL : audio_param->nBitPerSample = 64; audio_param->eNumData = OMX_NumericalDataFloat; return 0;
        default : av_log(avctx, AV_LOG_ERROR, "Unsupported sample format : %x.\n", sample_fmt);
    }

    return AVERROR(EINVAL);
}

static int omx_set_audio_pcm_param(AVCodecContext *avctx)
{
    OMXComponentContext *s = avctx->priv_data;

    avctx->sample_rate = avctx->sample_rate == 0 ? 48000 : avctx->sample_rate;

    OMX_AUDIO_PARAM_PCMMODETYPE audio_param_pcm;
    INIT_STRUCT(audio_param_pcm);
    audio_param_pcm.nPortIndex = OMX_ALL;
    sample_fmt_to_omx_index(&audio_param_pcm, avctx, avctx->sample_fmt);
    audio_param_pcm.nChannels = avctx->channels;
    audio_param_pcm.nSamplingRate = avctx->sample_rate;
    channels_to_ChannelMapping(audio_param_pcm.nChannels, &audio_param_pcm.eChannelMapping);

    return OMX_SetParameter(s->component, OMX_IndexParamAudioPcm, &audio_param_pcm);
}

static int omx_set_audio_heaac_param(AVCodecContext *avctx)
{
    OMXComponentContext *s = avctx->priv_data;

    // Only bitrate is used from this structure now.

    OMX_AUDIO_PARAM_AACPROFILETYPE audio_param_aac;
    INIT_STRUCT(audio_param_aac);
    audio_param_aac.nPortIndex = OMX_ALL;
    switch (avctx->profile) {
        case FF_PROFILE_AAC_LOW:    audio_param_aac.eAACProfile = OMX_AUDIO_AACObjectLC;    break;
        case FF_PROFILE_AAC_HE:     audio_param_aac.eAACProfile = OMX_AUDIO_AACObjectHE;    break;
        case FF_PROFILE_AAC_HE_V2:  audio_param_aac.eAACProfile = OMX_AUDIO_AACObjectHE_PS; break;
        case FF_PROFILE_AAC_XHE:    audio_param_aac.eAACProfile = OMX_AUDIO_AACObjectXHE;   break;
        default:
            av_log(avctx, AV_LOG_ERROR, "Unsupported audio profile: %d\n", avctx->profile);
            return -1;
    }
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

    ret = omx_cmpnt_init(s);
    if (ret) return ret;

    ret = omx_set_audio_pcm_param(avctx);
    if (ret) return ret;

    ret = omx_set_audio_heaac_param(avctx);
    if (ret) return ret;

    if (!(avctx->flags & AV_CODEC_FLAG_GLOBAL_HEADER))
        avctx->codec_id = AV_CODEC_ID_AAC_LATM;

    ret = omx_set_commandline(avctx);
    if (ret) return ret;

    ret = omx_cmpnt_start(s);
    if (ret) return ret;

    const size_t codec_config_max_size = 1024;
    const size_t struct_size = sizeof(OMX_AUDIO_PARAM_XHEAACTYPE) + codec_config_max_size;
    OMX_AUDIO_PARAM_XHEAACTYPE* audio_param_xheaac = malloc(struct_size);
    INIT_STRUCT(*audio_param_xheaac);

    audio_param_xheaac->nCodecConfigMaxSize = codec_config_max_size;

    OMX_GetParameter(s->component, OMX_IndexParamAudioXheaac, audio_param_xheaac);

    AVCPBProperties *cpb_props = ff_add_cpb_side_data(avctx);
    cpb_props->max_bitrate = audio_param_xheaac->nMaxBitRate;
    cpb_props->avg_bitrate = audio_param_xheaac->nAvgBitRate;
    cpb_props->buffer_size = audio_param_xheaac->nBufferSize * 8;

    avctx->frame_size = audio_param_xheaac->nFrameSamples;
    avctx->initial_padding = audio_param_xheaac->nPriming;
    avctx->rap_interval = (int64_t)audio_param_xheaac->nRapInterval * 1000000 / avctx->sample_rate;
    avctx->roll_distance = (audio_param_xheaac->nStandardDelay + avctx->frame_size - 1) / avctx->frame_size;

    if (avctx->bit_rate <= 0)
        avctx->bit_rate = audio_param_xheaac->nAvgBitRate;
    avctx->extradata_size = audio_param_xheaac->nCodecConfigSizeUsed;
    avctx->extradata = av_mallocz(avctx->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
    memcpy(avctx->extradata, audio_param_xheaac->CodecConfig, avctx->extradata_size);

    return ret;
}

static const AVCodecDefault heaac_enc_omx_defaults[] = {
        { "ab",                "0" },
        { "profile",           "41" },
        { NULL },
};

static const AVClass omx_xheaac_encoder_class = {
        .class_name = "omx_enc_xheaac",
        .item_name  = av_default_item_name,
        .option     = omx_options,
        .version    = LIBAVUTIL_VERSION_INT,
};

AVCodec ff_xheaac_omx_encoder = {
        .name             = "omx_enc_xheaac",
        .long_name        = NULL_IF_CONFIG_SMALL("OpenMAX IL HE-AAC Encoder"),
        .type             = AVMEDIA_TYPE_AUDIO,
        .id               = AV_CODEC_ID_AAC,
        .priv_data_size   = sizeof(OMXComponentContext),
        //.init           = omx_cmpnt_init,
        .init             = omx_cmpnt_encoder_init,
        .close            = omx_cmpnt_codec_end,
        .receive_packet   = omx_receive_packet,
        .capabilities     = AV_CODEC_CAP_VARIABLE_FRAME_SIZE | AV_CODEC_CAP_DELAY | AV_CODEC_CAP_DR1,
        .defaults         = heaac_enc_omx_defaults,
        .priv_class       = &omx_xheaac_encoder_class,
        .sample_fmts    = (const enum AVSampleFormat[]){ AV_SAMPLE_FMT_S16,
                                                         AV_SAMPLE_FMT_NONE },
};
