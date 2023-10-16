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

#ifndef AVCODEC_OMX_AUDIO_ENC_COMMON_H
#define AVCODEC_OMX_AUDIO_ENC_COMMON_H

int omx_set_audio_pcm_param(AVCodecContext *avctx);
int omx_cvt_channels_to_channel_mapping(int channels, OMX_AUDIO_CHANNELTYPE channelMapping[OMX_AUDIO_MAXCHANNELS]);

#endif // #define AVCODEC_OMX_AUDIO_ENC_COMMON_H
