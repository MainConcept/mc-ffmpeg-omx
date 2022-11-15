/*
 * OMX Video decoder
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

#include "omx_video_dec_hevc.h"
#include "profiles.h"

static int omx_receive_frame(AVCodecContext* avctx, AVFrame* frame) {
    OMXHEVCDecComponentContext *s_avc = avctx->priv_data;
    OMXComponentContext *s = &s_avc->base;

    return dec_omx_receive_frame(s, frame);
}

static av_cold int omx_cmpnt_decoder_init(AVCodecContext *avctx)
{
    OMXHEVCDecComponentContext *s_hevc = avctx->priv_data;
    OMXComponentContext *s = &s_hevc->base;

    s->avctx = avctx;
    s_hevc->in_cnt = s_hevc->out_cnt = 0;

    int ret = omx_cmpnt_init(s);
    if (ret) return ret;

    ret = omx_set_pic_param(avctx);
    if (ret) return ret;

    ret = omx_set_avc_param(avctx, s_hevc->level);
    if (ret) return ret;

    ret = omx_set_commandline(avctx);
    if (ret) return ret;

    ret = omx_cmpnt_start(s);
    if (ret) return ret;

    return 0;
}

#define OFFSET(x) offsetof(OMXHEVCDecComponentContext, x)
#define ED  AV_OPT_FLAG_AUDIO_PARAM | AV_OPT_FLAG_DECODING_PARAM | AV_OPT_FLAG_ENCODING_PARAM | AV_OPT_FLAG_VIDEO_PARAM

const AVOption hevc_dec_omx_options[] = {
        { "omx_core" , "OMX Core library name"   ,  OFFSET(base.core_libname)   ,        AV_OPT_TYPE_STRING, { .str = "" }, 0, 0, ED },
        { "omx_name" , "OMX component name"      ,  OFFSET(base.component_name) ,        AV_OPT_TYPE_STRING, { .str = "" }, 0, 0, ED },
        { "omx_param_dec", "OMX component parameters", OFFSET(base.component_param),     AV_OPT_TYPE_STRING, { .str = NULL }, 0, 0, ED },
        { NULL }
};

static const AVClass omx_hevc_decoder_class = {
        .class_name = "omx_dec_hevc",
        .item_name  = av_default_item_name,
        .option     = hevc_dec_omx_options,
        .version    = LIBAVUTIL_VERSION_INT,
};

AVCodec ff_hevc_omx_decoder = {
        .name             = "omx_dec_hevc",
        .long_name        = NULL_IF_CONFIG_SMALL("OMX IL HEVC Decoder"),
        .type             = AVMEDIA_TYPE_VIDEO,
        .id               = AV_CODEC_ID_H265,
        .priv_data_size   = sizeof(OMXHEVCDecComponentContext),
        .init             = omx_cmpnt_decoder_init,
        .close            = omx_cmpnt_codec_end,
        .receive_frame    = omx_receive_frame,
        .capabilities     = AV_CODEC_CAP_DELAY | AV_CODEC_CAP_DR1,
        .bsfs             = "hevc_mp4toannexb",
        .priv_class       = &omx_hevc_decoder_class,
        .pix_fmts         = (const enum AVPixelFormat[]){AV_PIX_FMT_YUV420P, AV_PIX_FMT_YUV422P, AV_PIX_FMT_YUV420P10LE, AV_PIX_FMT_YUV422P10LE, AV_PIX_FMT_NONE}
};
