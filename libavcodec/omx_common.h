/*
 * OMX Common
 * Copyright (c) 2023 MainConcept GmbH or its affiliates.
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

#ifndef AVCODEC_OMX_COMMON_H
#define AVCODEC_OMX_COMMON_H
#include "libavutil/omx_common.h"

// Functions above are about pure OMX and have nothing to do with FFMPEG
// Functions below are about OMX-FFMPEG structures relations
// May be they should be separated among different files
int omx_receive_packet(AVCodecContext *avctx, AVPacket *avpkt);
int omx_cmpnt_codec_end(AVCodecContext *avctx);

OMX_COLOR_MATRIX_COEFFS AV_to_OMX_colorspace(enum AVColorSpace colorspace);
OMX_COLOR_RANGE AV_to_OMX_color_range(enum AVColorRange fmt);
OMX_COLOR_PRIMARIES AV_to_OMX_color_primaries(enum AVColorPrimaries fmt);
OMX_COLOR_TRANSFER AV_to_OMX_color_trc(enum AVColorTransferCharacteristic fmt);

#endif // #ifndef AVCODEC_OMX_COMMON_H
