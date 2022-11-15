/*
 * OMX Audio encoder
 * Copyright (c) 2022 MainConcept GmbH or its affiliates.
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

#include "libavutil/avutil.h"
#include "libavutil/log.h"
#include "libavutil/opt.h"

#include "omx_common.h"
#include "omx_audio_enc_common.h"

int omx_cvt_channels_to_channel_mapping(int channels, OMX_AUDIO_CHANNELTYPE channelMapping[OMX_AUDIO_MAXCHANNELS]) {
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

static int sample_fmt_to_omx_index(OMX_AUDIO_PARAM_PCMMODETYPE *audio_param, AVCodecContext *avctx, enum AVSampleFormat sample_fmt) {

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

int omx_set_audio_pcm_param(AVCodecContext *avctx)
{
    OMXComponentContext *s = avctx->priv_data;

    avctx->sample_rate = avctx->sample_rate == 0 ? 48000 : avctx->sample_rate;

    OMX_AUDIO_PARAM_PCMMODETYPE audio_param_pcm;
    INIT_STRUCT(audio_param_pcm);
    audio_param_pcm.nPortIndex = OMX_ALL;
    sample_fmt_to_omx_index(&audio_param_pcm, avctx, avctx->sample_fmt);
    audio_param_pcm.nChannels = avctx->channels;
    audio_param_pcm.nSamplingRate = avctx->sample_rate;
    // audio_param_pcm.eEndian;
    // audio_param_pcm.bInterleaved;
    // audio_param_pcm.ePCMMode;
    //audio_param_pcm.eChannelMapping[OMX_AUDIO_MAXCHANNELS];
    omx_cvt_channels_to_channel_mapping(audio_param_pcm.nChannels, &audio_param_pcm.eChannelMapping);

    return OMX_SetParameter(s->component, OMX_IndexParamAudioPcm, &audio_param_pcm);
}
