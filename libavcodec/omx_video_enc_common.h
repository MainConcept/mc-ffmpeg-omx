/*
 * OMX Video encoder
 * Copyright (c) 2021 MainConcept GmbH or its affiliates.
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

#ifndef AVCODEC_OMX_VIDEO_ENC_COMMON_H
#define AVCODEC_OMX_VIDEO_ENC_COMMON_H

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

//#define DUMP_INPUT_DATA
#define MAX_ARG_STRLEN 32000

int parse_extradata(OMX_BUFFERHEADERTYPE* buf, int64_t *dts_omx, int64_t *duration_omx);

int frame_to_buffer(AVCodecContext *avctx, OMX_BUFFERHEADERTYPE* buf, const AVFrame *fr);

int omx_send_frame(AVCodecContext *avctx, const AVFrame *frame);

int buffer_to_packet(AVCodecContext *avctx, AVPacket *avpkt, OMX_BUFFERHEADERTYPE* buf);
int omx_receive_packet(AVCodecContext *avctx, AVPacket *avpkt);

av_cold void omx_append_parameter(char* dst, int dst_len, const char* format, ...);

OMX_COLOR_FORMATTYPE pix_format_to_OMX(enum AVPixelFormat fmt);

int omx_populate_extradata(AVCodecContext *avctx, OMX_BUFFERHEADERTYPE* buf);
int omx_set_pic_param(AVCodecContext *avctx);
int profile_to_omx(int ten_bit, int profile);
int level_to_omx(const char* level);
int omx_set_avc_param(AVCodecContext *avctx, const char* level);
int omx_set_commandline(AVCodecContext *avctx);
int omx_cmpnt_codec_end(AVCodecContext *avctx);

#endif // AVCODEC_OMX_VIDEO_ENC_COMMON_H