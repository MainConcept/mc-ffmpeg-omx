/*
 * OMX Video encoder
 * Copyright (c) 2022 MainConcept GmbH or its affiliates.
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

#include "omx_video_enc_vvc.h"
#include "profiles.h"

static av_cold int omx_cmpnt_encoder_init(AVCodecContext *avctx)
{
    OMXVVCEncComponentContext *s_vvc = avctx->priv_data;
    OMXComponentContext *s = &s_vvc->base;

    s->avctx = avctx;

    int ret = omx_cmpnt_init(s);
    if (ret) return ret;

    ret = omx_set_pic_param(avctx);
    if (ret) return ret;

    ret = omx_set_avc_param(avctx, s_vvc->level);
    if (ret) return ret;

    ret = omx_set_commandline(avctx);
    if (ret) return ret;

    ret = omx_cmpnt_start(s);
    if (ret) return ret;

    return 0;
}

#define OFFSET(x) offsetof(OMXVVCEncComponentContext, x)
#define ED  AV_OPT_FLAG_AUDIO_PARAM | AV_OPT_FLAG_DECODING_PARAM | AV_OPT_FLAG_ENCODING_PARAM | AV_OPT_FLAG_VIDEO_PARAM

const AVOption vvc_enc_omx_options[] = {
        { "omx_core" , "OMX Core library name"   ,  OFFSET(base.core_libname)   ,        AV_OPT_TYPE_STRING, { .str = "" }, 0, 0, ED },
        { "omx_name" , "OMX component name"      ,  OFFSET(base.component_name) ,        AV_OPT_TYPE_STRING, { .str = "" }, 0, 0, ED },
        { "omx_param", "OMX component parameters",  OFFSET(base.component_param),        AV_OPT_TYPE_STRING, { .str = NULL }, 0, 0, ED },

        { NULL }
};

static const AVCodecDefault vvc_enc_omx_defaults[] = {
        { "b",                "0"  }, // bitrate
        { "bf",               "-1" }, // B-frames between non-B-frames
        { "g",                "-1" }, // the group of picture (GOP) size
        { "refs",             "-1" }, // reference frames
        { NULL },
};

static const AVClass omx_vvc_encoder_class = {
        .class_name = "omx_enc_vvc",
        .item_name  = av_default_item_name,
        .option     = vvc_enc_omx_options,
        .version    = LIBAVUTIL_VERSION_INT,
};

AVCodec ff_vvc_omx_encoder = {
        .name             = "omx_enc_vvc",
        .long_name        = NULL_IF_CONFIG_SMALL("OMX IL VVC Encoder"),
        .type             = AVMEDIA_TYPE_VIDEO,
        .id               = AV_CODEC_ID_VVC,
        .priv_data_size   = sizeof(OMXVVCEncComponentContext),
        .init             = omx_cmpnt_encoder_init,
        .close            = omx_cmpnt_codec_end,
        .receive_packet   = omx_receive_packet,
        .capabilities     = AV_CODEC_CAP_DELAY | AV_CODEC_CAP_DR1,
        .profiles         = NULL_IF_CONFIG_SMALL(ff_h264_profiles),
        .defaults         = vvc_enc_omx_defaults,
        .priv_class       = &omx_vvc_encoder_class,
        .pix_fmts         = (const enum AVPixelFormat[]){AV_PIX_FMT_YUV420P, AV_PIX_FMT_YUV422P, AV_PIX_FMT_YUV420P10LE, AV_PIX_FMT_YUV422P10LE, AV_PIX_FMT_NONE}
};
