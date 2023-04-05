/*
 * OMX Common
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

#include "config.h"

#ifdef _WIN32
#include "compat/w32dlfcn.h"
#else
#include <dlfcn.h>
#endif

#include <time.h>

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


#include "encode.h"
#include "atsc_a53.h"

#include "decode.h"
#include "avcodec.h"
#include "internal.h"
#include "omx_common.h"

#include <float.h>

static int parse_extradata(OMX_BUFFERHEADERTYPE* buf, int64_t *dts_omx, int64_t* duration_omx)
{
    uint64_t offset = (buf->nOffset + buf->nFilledLen + 0x03L) & ~0x03L;

    if (buf->nFlags & OMX_BUFFERFLAG_EXTRADATA && buf->nAllocLen - (offset + sizeof(OMX_OTHER_EXTRADATATYPE)) > 0) {
        OMX_OTHER_EXTRADATATYPE *pExtra = (OMX_OTHER_EXTRADATATYPE *)(buf->pBuffer + offset);
        while (pExtra->eType != OMX_ExtraDataNone && buf->nAllocLen - (offset + pExtra->nSize) > 0)
        {
            switch (pExtra->eType) {
                case OMX_ExtraDataDTS:
                    *dts_omx = from_omx_ticks(((TIMESTAMP_PARAM*)pExtra->data)->DTS);
                    *duration_omx = from_omx_ticks(((TIMESTAMP_PARAM*)pExtra->data)->duration);
                default:
                    break;
            }
            offset += (pExtra->nSize + 0x03L) & ~0x03L;
            pExtra = (OMX_OTHER_EXTRADATATYPE *)(buf->pBuffer + offset);
        }
    }
    return 0;
}


static int buffer_to_packet(AVCodecContext *avctx, AVPacket *avpkt, OMX_BUFFERHEADERTYPE* buf)
{
    OMXComponentContext *s = avctx->priv_data;

    int ret = 0;

    if (buf->nFlags & OMX_BUFFERFLAG_CODECCONFIG) { // Codecconfig will be attached to the next buffer with data. Otherwise ffmpeg will report error about NOPTS and TS muxer won't work due to empty packet.
        av_free(s->codec_config);
        s->codec_config = av_malloc(buf->nFilledLen);
        memcpy(s->codec_config, buf->pBuffer + buf->nOffset, buf->nFilledLen);
        s->codec_config_len = buf->nFilledLen;

        return AVERROR(EAGAIN);
    }

    ret = av_new_packet(avpkt, buf->nFilledLen);
    avpkt->flags &= ~AV_PKT_DATA_NEW_EXTRADATA;
    memcpy(avpkt->data, buf->pBuffer + buf->nOffset, buf->nFilledLen);
    avpkt->size = buf->nFilledLen;

    if (s->codec_config && s->codec_config_len) {
        uint8_t* p = av_packet_new_side_data(avpkt, AV_PKT_DATA_NEW_EXTRADATA, s->codec_config_len); // adding padding for extradata isn't neceessary here as it is added by packet allocator
        avpkt->flags |= AV_PKT_DATA_NEW_EXTRADATA;
        memcpy(p, s->codec_config, s->codec_config_len);
        av_free(s->codec_config);
        s->codec_config = NULL;
        s->codec_config_len = 0;
    }

    if (buf->nFlags & OMX_BUFFERFLAG_TIMESTAMPINVALID) {
        avpkt->pts = AV_NOPTS_VALUE;
        avpkt->dts = AV_NOPTS_VALUE;
    } else {
        const int64_t getting_pts =
                (from_omx_ticks(buf->nTimeStamp) * (int64_t)avctx->time_base.den + 1000000LL * (int64_t)avctx->time_base.num / 2) /
                (1000000LL * avctx->time_base.num);
        int64_t getting_dts = AV_NOPTS_VALUE;
        int64_t getting_duration = 0;

        int64_t dts_omx = AV_NOPTS_VALUE;
        int64_t duration_omx = AV_NOPTS_VALUE;
        parse_extradata(buf, &dts_omx, &duration_omx);

        if (dts_omx != AV_NOPTS_VALUE) {
            getting_dts = (llabs(dts_omx) * (int64_t) avctx->time_base.den + 1000000LL * avctx->time_base.num / 2) /
                          (1000000LL * avctx->time_base.num);
            getting_dts = dts_omx > 0 ? getting_dts : -getting_dts;
        }

        if (duration_omx != AV_NOPTS_VALUE) {
            getting_duration = (duration_omx * (int64_t)avctx->time_base.den + 1000000LL * avctx->time_base.num / 2) /
                               (1000000LL * avctx->time_base.num);
        }

        avpkt->pts = getting_pts;
        avpkt->dts = getting_dts;
        avpkt->duration = getting_duration;
    }

    if (buf->nFlags & OMX_BUFFERFLAG_SYNCFRAME) {
        avpkt->audioframe_flags |= AV_PKT_AUDIOFRAME_FLAG_IPF;
        avpkt->flags |= AV_PKT_FLAG_KEY;
    } else
        avpkt->flags &= ~AV_PKT_FLAG_KEY;

    if (buf->nFlags & OMX_BUFFERFLAG_INDEPENDENT_FRAME)
        avpkt->audioframe_flags |= AV_PKT_AUDIOFRAME_FLAG_IF;

    if (buf->nFlags & OMX_BUFFERFLAG_AAC_RAP_SWITCHABLE)
        avpkt->audioframe_flags |= AV_PKT_AUDIOFRAME_FLAG_AAC_RAP;

    return ret;
}

static int convert_buffer(AVCodecContext* avctx, OMX_BUFFERHEADERTYPE** out_buf, AVPacket* avpkt, int* buffer_eos_flag)
{
    OMXComponentContext *s = avctx->priv_data;

    int ret = 0;

    ret = buffer_to_packet(avctx, avpkt, *out_buf);

    *buffer_eos_flag = (*out_buf)->nFlags & OMX_BUFFERFLAG_EOS;
    (*out_buf)->nFilledLen = 0;
    OMX_FillThisBuffer(s->component, *out_buf);
    *out_buf = NULL;

    return ret;
}

static int fill_extradata_sei_buf(OMX_BUFFERHEADERTYPE* buf, uint8_t* sei_data, uint64_t sei_size)
{
    uint64_t offset = av_omx_get_ext_pos(buf->pBuffer, buf->nOffset + buf->nFilledLen);

    OMX_OTHER_EXTRADATATYPE* seicc_ext = buf->pBuffer + offset;
    INIT_STRUCT(*seicc_ext);

    seicc_ext->nSize += sei_size;
    seicc_ext->nDataSize = sei_size;
    seicc_ext->eType = OMX_ExtraDataA53CC;

    memcpy(seicc_ext->data, sei_data, sei_size);

    offset += av_omx_get_ext_pos(buf->pBuffer + offset, seicc_ext->nSize);

    OMX_OTHER_EXTRADATATYPE last_empty_extra;
    INIT_STRUCT(last_empty_extra);
    last_empty_extra.eType = OMX_ExtraDataNone;

    memcpy(buf->pBuffer + offset, (uint8_t*)&last_empty_extra, last_empty_extra.nSize);
    buf->nFlags |= OMX_BUFFERFLAG_EXTRADATA;

    return 0;
}

static int frame_to_buffer_video(AVCodecContext *avctx, OMX_BUFFERHEADERTYPE* buf, const AVFrame *fr)
{
    OMXComponentContext *s = avctx->priv_data;

    const AVPixFmtDescriptor * pix_desc = av_pix_fmt_desc_get((enum AVPixelFormat)fr->format);
    size_t luma_width = 0;
    size_t chroma_width = 0;

    size_t luma_sz = 0;
    size_t chroma_sz = 0;

    size_t U_off = 0;
    size_t V_off = 0;

    buf->nFlags = 0;

    if (!(pix_desc->flags & AV_PIX_FMT_FLAG_PLANAR))
        return AVERROR_INVALIDDATA; // Only planar formats are supported

    luma_width = fr->width * pix_desc->comp->step;
    chroma_width = (fr->width >> pix_desc->log2_chroma_w) * pix_desc->comp->step;

    luma_sz = fr->height * luma_width;
    chroma_sz = luma_sz >> (pix_desc->log2_chroma_h + pix_desc->log2_chroma_w);

    for (int i = 0; i < fr->height; i++)
        memcpy(buf->pBuffer + buf->nOffset + i * luma_width, fr->data[0] + i * fr->linesize[0], luma_width);

    U_off = luma_sz;
    V_off = luma_sz + chroma_sz;

    for (int i = 0; i<fr->height >> pix_desc->log2_chroma_h; i++) {
        memcpy(buf->pBuffer + buf->nOffset + U_off + i * chroma_width, fr->data[1] + i * fr->linesize[1], chroma_width);
        memcpy(buf->pBuffer + buf->nOffset + V_off + i * chroma_width, fr->data[2] + i * fr->linesize[2], chroma_width);
    }

#ifdef DUMP_INPUT_DATA
    static FILE* out = NULL;
    if (!out)
        out = fopen("file.yuv", "wb");
    fwrite(buf->pBuffer + buf->nOffset, 1, luma_sz + chroma_sz + chroma_sz, out);
#endif // #ifdef DUMP_INPUT_DATA

    buf->nTimeStamp = to_omx_ticks(fr->pts * 1000000 * avctx->time_base.num / avctx->time_base.den);
    buf->nFilledLen = luma_sz + 2 * chroma_sz;

    if (fr->pict_type == AV_PICTURE_TYPE_I) // ffmpeg set this type on input frame when -force-key_frames is used ("-force_key_frames expr:gte\(t,n_forced*2\)")
        buf->nFlags |= OMX_BUFFERFLAG_SYNCFRAME;

    if (s->a53_cc) {
        void* sei_data;
        size_t sei_size = 0;
        int ret = ff_alloc_a53_sei(fr, 0, &sei_data, &sei_size);
        if(sei_size)
            fill_extradata_sei_buf(buf, (uint8_t*)sei_data, sei_size);
    }

    return 0;
}

static int frame_to_buffer_audio(AVCodecContext *avctx, OMX_BUFFERHEADERTYPE* buf, const AVFrame *fr)
{
    int new_data_size = fr->nb_samples * fr->channels * av_get_bytes_per_sample(fr->format);
    memcpy(buf->pBuffer + buf->nOffset, fr->extended_data[0], new_data_size);

#ifdef DUMP_INPUT_DATA
    static FILE* out = NULL;

    if (!out)
        out = fopen("file.pcm", "wb");

    fwrite(buf->pBuffer + buf->nOffset, 1, new_data_size, out);
#endif // #ifdef DUMP_INPUT_DATA

    buf->nTimeStamp = to_omx_ticks(fr->pts * 1000000 * avctx->time_base.num / avctx->time_base.den);
    buf->nFilledLen = new_data_size;
    buf->nFlags = 0;

    return 0;
}

int omx_receive_packet(AVCodecContext *avctx, AVPacket *avpkt)
{
    OMXComponentContext *s = avctx->priv_data;
    int ret = 0;
    int buffer_eos_flag = 0;

    OMX_BUFFERHEADERTYPE *out_buf = NULL;
    OMX_BUFFERHEADERTYPE *in_buf  = NULL;

    ret = av_omx_wait_any_buffer(s, &out_buf, &in_buf);
    if (ret == AVERROR(EINVAL))
        return ret;

    if (in_buf) {
        ret = ff_encode_get_frame(avctx, s->frame);
        if (ret == AVERROR_EOF)
            s->eos_flag = OMX_TRUE;
        else if (ret >= 0) {
            ret = avctx->codec_type == AVMEDIA_TYPE_VIDEO ? frame_to_buffer_video(avctx, in_buf, s->frame)
                                                          : frame_to_buffer_audio(avctx, in_buf, s->frame);

            av_frame_unref(s->frame);

            OMX_EmptyThisBuffer(s->component, in_buf);
            in_buf = NULL;
        } else {
            av_omx_put_input_buffer(s, in_buf);
        }

        if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
            return ret;
    }

    if (s->eos_flag) {
        if (in_buf) { // Send buffer back, if eos flag is set, we won't have anything to proceed.
            in_buf->nFlags = OMX_BUFFERFLAG_EOS;
            in_buf->nFilledLen = 0;
            OMX_EmptyThisBuffer(s->component, in_buf);
            in_buf = NULL;
        }

        if (!out_buf)
            out_buf = av_omx_wait_output_buffer(s);
    }

    // It's called from do_audio_out() line 910 of ffmpeg.c. While this function returns 0, it's called in loop. And as it returns EAGAIN only when there is
    // input buffer in the queue, it will not block in omx_send_frame() because when it's called there should be at least single input buffer in queue.

    if (out_buf) {
        if (out_buf->nFlags & OMX_BUFFERFLAG_EOS) { // The last output buffer from OMX plugin.
            out_buf->nFilledLen = 0;
            OMX_FillThisBuffer(s->component, out_buf);
            return AVERROR_EOF;
        }

        ret = convert_buffer(avctx, &out_buf, avpkt, &buffer_eos_flag);

// Idea is: If input stream was ended, but there are no output frames yet (stream is short, and encoder buffer is bigger than stream) there is a chance that
// the first buffer we received while flushing is codec config. But it has no payload. So we have to wait for the next buffer (and we expect it will be payload buffer,
// otherwise it won't work). But we can't wait for output payload buffer when stream wasn't ended because encoder could send codec config before any input frames were consumed.
        if (avpkt->size == 0 && s->eos_flag) {
            out_buf = av_omx_wait_output_buffer(s);

            ret = convert_buffer(avctx, &out_buf, avpkt, &buffer_eos_flag);
        }

        return buffer_eos_flag ? AVERROR_EOF : ret;
    } else {
        return AVERROR(EAGAIN);
    }
}

int omx_cmpnt_codec_end(AVCodecContext *avctx)
{
    OMXComponentContext *s = avctx->priv_data;

    av_omx_cmpnt_end(s);

    avctx->extradata_size = 0;
    av_free(avctx->extradata);
    avctx->extradata = NULL;

    return 0;
}
 