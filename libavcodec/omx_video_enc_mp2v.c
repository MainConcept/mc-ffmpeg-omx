/*
 * OMX Video MP2V encoder
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

#include "omx_video_enc_mp2v.h"
#include "profiles.h"

static av_cold int omx_cmpnt_encoder_init(AVCodecContext *avctx)
{
    OMXMP2VEncComponentContext *s_mp2v = avctx->priv_data;
    OMXComponentContext *s = &s_mp2v->base;

    s->avctx = avctx;

    int ret = av_omx_cmpnt_init(s);
    if (ret) return ret;

    ret = omx_set_pic_param(avctx);
    if (ret) return ret;

    ret = omx_set_mpeg2_param(avctx, s_mp2v->level);
    if (ret) return ret;

    ret = av_omx_set_commandline(s);
    if (ret) return ret;

    ret = av_omx_cmpnt_start(s);
    if (ret) return ret;

    ret = omx_get_codec_config(avctx, s->component);
    if (ret) return ret;

    return 0;
}

#define OFFSET(x) offsetof(OMXMP2VEncComponentContext, x)
#define ED  AV_OPT_FLAG_AUDIO_PARAM | AV_OPT_FLAG_DECODING_PARAM | AV_OPT_FLAG_ENCODING_PARAM | AV_OPT_FLAG_VIDEO_PARAM
#define VE AV_OPT_FLAG_ENCODING_PARAM | AV_OPT_FLAG_VIDEO_PARAM

const AVOption mp2v_enc_omx_options[] = {
        { "omx_core" , "OMX Core library name"   ,  OFFSET(base.core_libname)   ,        AV_OPT_TYPE_STRING, { .str = "" }, 0, 0, ED },
        { "omx_name" , "OMX component name"      ,  OFFSET(base.component_name) ,        AV_OPT_TYPE_STRING, { .str = "" }, 0, 0, ED },
        { "omx_param", "OMX component parameters",  OFFSET(base.component_param),        AV_OPT_TYPE_STRING, { .str = NULL }, 0, 0, ED },

#define LEVEL(name, value) name, NULL, 0, AV_OPT_TYPE_CONST, { .i64 = value }, 0, 0, VE, "avctx.level"
        { LEVEL("high",     OMX_VIDEO_MPEG2LevelHL) },
        { LEVEL("high1440", OMX_VIDEO_MPEG2LevelH14) },
        { LEVEL("main",     OMX_VIDEO_MPEG2LevelML) },
        { LEVEL("low",      OMX_VIDEO_MPEG2LevelLL) },
#undef LEVEL

        FF_AVCTX_PROFILE_OPTION("422",           NULL, VIDEO, FF_PROFILE_MPEG2_422)
        FF_AVCTX_PROFILE_OPTION("high",          NULL, VIDEO, FF_PROFILE_MPEG2_HIGH)
        FF_AVCTX_PROFILE_OPTION("ss",            NULL, VIDEO, FF_PROFILE_MPEG2_SS)
        FF_AVCTX_PROFILE_OPTION("snr",           NULL, VIDEO, FF_PROFILE_MPEG2_SNR_SCALABLE)
        FF_AVCTX_PROFILE_OPTION("main",          NULL, VIDEO, FF_PROFILE_MPEG2_MAIN)
        FF_AVCTX_PROFILE_OPTION("simple",        NULL, VIDEO, FF_PROFILE_MPEG2_SIMPLE)

        { NULL }
};

static const FFCodecDefault mp2v_enc_omx_defaults[] = {
        { "b",                "0"  }, // bitrate
        { "bf",               "-1" }, // B-frames between non-B-frames
        { "g",                "-1" }, // the group of picture (GOP) size
        { NULL },
};

static const AVClass omx_mp2v_encoder_class = {
        .class_name = "omx_enc_mp2v",
        .item_name  = av_default_item_name,
        .option     = mp2v_enc_omx_options,
        .version    = LIBAVUTIL_VERSION_INT,
};

FFCodec ff_mp2v_omx_encoder = {
        .p.name            = "omx_enc_mp2v",
        .p.long_name       = NULL_IF_CONFIG_SMALL("OMX IL MP2V Encoder"),
        .p.type            = AVMEDIA_TYPE_VIDEO,
        .p.id              = AV_CODEC_ID_MPEG2VIDEO,
        .priv_data_size    = sizeof(OMXMP2VEncComponentContext),
        .init              = omx_cmpnt_encoder_init,
        .close             = omx_cmpnt_codec_end,
        .cb_type           = FF_CODEC_CB_TYPE_RECEIVE_PACKET,
        .cb.receive_packet = omx_receive_packet,
        .p.capabilities    = AV_CODEC_CAP_DELAY | AV_CODEC_CAP_DR1 | AV_CODEC_FLAG_GLOBAL_HEADER,
        .p.profiles        = NULL_IF_CONFIG_SMALL(ff_mpeg2_video_profiles),
        .defaults          = mp2v_enc_omx_defaults,
        .p.priv_class      = &omx_mp2v_encoder_class,
        .p.pix_fmts        = (const enum AVPixelFormat[]){AV_PIX_FMT_YUV420P, AV_PIX_FMT_YUV422P, AV_PIX_FMT_YUV420P10LE, AV_PIX_FMT_YUV422P10LE, AV_PIX_FMT_NONE}
};
