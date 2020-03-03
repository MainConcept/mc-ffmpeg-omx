/*
 * OMX Video encoder
 * Copyright (c) 2020 MainConcept GmbH or its affiliates.
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

#ifndef AVCODEC_OMX_VIDEO_ENC_HEVC_H
#define AVCODEC_OMX_VIDEO_ENC_HEVC_H

#include "omx_video_enc_common.h"

typedef struct OMXHEVCEncComponentContext {
    struct OMXComponentContext base;

    char* cfgFilePath;

    int   perf_level;
    int   perf_level1;
    int   preset;

    int quality_mode;
    int acc_type;
    int acc_mode;

    char* level;

    int mpass;
} OMXHEVCEncComponentContext;

#endif // AVCODEC_OMX_VIDEO_ENC_HEVC_H
