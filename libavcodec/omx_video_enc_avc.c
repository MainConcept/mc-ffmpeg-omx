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

#include "omx_video_enc_avc.h"
#include "profiles.h"

static av_cold int omx_cmpnt_encoder_init(AVCodecContext *avctx)
{
    OMXAVCEncComponentContext *s_avc = avctx->priv_data;
    OMXComponentContext *s = &s_avc->base;

    s->avctx = avctx;

    int ret = omx_cmpnt_init(s);
    if (ret) return ret;

    ret = omx_set_pic_param(avctx);
    if (ret) return ret;

    ret = omx_set_avc_param(avctx, s_avc->level);
    if (ret) return ret;

    ret = omx_set_commandline(avctx);
    if (ret) return ret;

    ret = omx_cmpnt_start(s);
    if (ret) return ret;

    return 0;
}

#define OFFSET(x) offsetof(OMXAVCEncComponentContext, x)
#define ED  AV_OPT_FLAG_AUDIO_PARAM | AV_OPT_FLAG_DECODING_PARAM | AV_OPT_FLAG_ENCODING_PARAM | AV_OPT_FLAG_VIDEO_PARAM

const AVOption avc_enc_omx_options[] = {
        { "omx_core" , "OMX Core library name"   , OFFSET(base.core_libname)   , AV_OPT_TYPE_STRING, { .str = NULL }, 0, 0, ED },
        { "omx_name" , "OMX component name"      , OFFSET(base.component_name) , AV_OPT_TYPE_STRING, { .str = NULL }, 0, 0, ED },

        { "omx_param", "OMX component parameters", OFFSET(base.component_param), AV_OPT_TYPE_STRING, { .str = NULL }, 0, 0, ED },

        { "level"      , "Specify level",          OFFSET(level),                AV_OPT_TYPE_STRING, { .str=NULL   }, 0, 0, ED },

        { "profile"    , NULL, OFFSET(base.profile), AV_OPT_TYPE_INT, {.i64 = FF_PROFILE_UNKNOWN }, INT_MIN, INT_MAX, ED, "profile" },
        { "baseline"   , NULL, 0,      AV_OPT_TYPE_CONST , { .i64 = FF_PROFILE_H264_BASELINE     }, INT_MIN, INT_MAX, ED, "profile" },
        { "main"       , NULL, 0,      AV_OPT_TYPE_CONST , { .i64 = FF_PROFILE_H264_MAIN         }, INT_MIN, INT_MAX, ED, "profile" },
        { "extended"   , NULL, 0,      AV_OPT_TYPE_CONST , { .i64 = FF_PROFILE_H264_EXTENDED     }, INT_MIN, INT_MAX, ED, "profile" },
        { "high"       , NULL, 0,      AV_OPT_TYPE_CONST , { .i64 = FF_PROFILE_H264_HIGH         }, INT_MIN, INT_MAX, ED, "profile" },
        { "high_10"    , NULL, 0,      AV_OPT_TYPE_CONST , { .i64 = FF_PROFILE_H264_HIGH_10      }, INT_MIN, INT_MAX, ED, "profile" },
        { "high_422"   , NULL, 0,      AV_OPT_TYPE_CONST , { .i64 = FF_PROFILE_H264_HIGH_422     }, INT_MIN, INT_MAX, ED, "profile" },
        { "high_444"   , NULL, 0,      AV_OPT_TYPE_CONST , { .i64 = FF_PROFILE_H264_HIGH_444     }, INT_MIN, INT_MAX, ED, "profile" },

        { "a53cc"      , "Use A53 Closed Captions", OFFSET(base.a53_cc), AV_OPT_TYPE_BOOL , { .i64 = 1 },  0, 1, ED },
        { "coder"      , "1 - cabac, 0 - cavlc"   , OFFSET(coder) , AV_OPT_TYPE_INT  , { .i64 =-1 }, -1, 1, ED },
        { "default"    , NULL                     , 0             , AV_OPT_TYPE_CONST, { .i64 =-1 }, INT_MIN, INT_MAX, ED, "coder" },
        { "cavlc"      , NULL                     , 0             , AV_OPT_TYPE_CONST, { .i64 = 0 }, INT_MIN, INT_MAX, ED, "coder" },
        { "cabac"      , NULL                     , 0             , AV_OPT_TYPE_CONST, { .i64 = 1 }, INT_MIN, INT_MAX, ED, "coder" },

        { NULL }
};

static const AVCodecDefault avc_enc_omx_defaults[] = {
        { "b",                "0" }, // bitrate
        { "bf",               "-1" }, // B-frames between non-B-frames
        { "g",                "-1" }, // the group of picture (GOP) size
        { "refs",             "-1" }, // reference frames
        { NULL },
};

static const AVClass omx_avc_encoder_class = {
        .class_name = "omx_enc_avc",
        .item_name  = av_default_item_name,
        .option     = avc_enc_omx_options,
        .version    = LIBAVUTIL_VERSION_INT,
};

AVCodec ff_avc_omx_encoder = {
        .name             = "omx_enc_avc",
        .long_name        = NULL_IF_CONFIG_SMALL("OMX IL AVC Encoder"),
        .type             = AVMEDIA_TYPE_VIDEO,
        .id               = AV_CODEC_ID_H264,
        .priv_data_size   = sizeof(OMXAVCEncComponentContext),
        .init             = omx_cmpnt_encoder_init,
        .close            = omx_cmpnt_codec_end,
        .send_frame       = omx_send_frame,
        .receive_packet   = omx_receive_packet,
        .capabilities     = AV_CODEC_CAP_DELAY | AV_CODEC_CAP_DR1,
        .profiles         = NULL_IF_CONFIG_SMALL(ff_h264_profiles),
        .defaults         = avc_enc_omx_defaults,
        .priv_class       = &omx_avc_encoder_class,
        .pix_fmts         = (const enum AVPixelFormat[]) { AV_PIX_FMT_YUV420P, AV_PIX_FMT_YUV422P, AV_PIX_FMT_YUV420P10LE, AV_PIX_FMT_YUV422P10LE, AV_PIX_FMT_NONE }
};
