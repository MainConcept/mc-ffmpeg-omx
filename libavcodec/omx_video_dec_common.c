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

#include "encode.h"
#include "atsc_a53.h"
#include "libavutil/eval.h"
#include "omx_video_dec_common.h"

static int fill_extradata_dts(OMX_BUFFERHEADERTYPE* buf, int64_t dts, int64_t duration)
{
    uint64_t offset = get_ext_pos(buf->pBuffer, buf->nOffset + buf->nFilledLen);

    OMX_OTHER_EXTRADATATYPE* dts_ext = buf->pBuffer + offset;
    INIT_STRUCT(*dts_ext);

    TIMESTAMP_PARAM current_time_param;
    current_time_param.DTS = dts;
    current_time_param.duration = duration;

    dts_ext->eType = OMX_ExtraDataDTS;
    dts_ext->nDataSize = sizeof(TIMESTAMP_PARAM);

    memcpy(dts_ext->data, &current_time_param, dts_ext->nDataSize);

    offset += get_ext_pos(buf->pBuffer + offset, dts_ext->nSize);

    OMX_OTHER_EXTRADATATYPE last_empty_extra;
    INIT_STRUCT(last_empty_extra);
    last_empty_extra.eType = OMX_ExtraDataNone;

    memcpy(buf->pBuffer + offset, (uint8_t*)&last_empty_extra, last_empty_extra.nSize);
    buf->nFlags |= OMX_BUFFERFLAG_EXTRADATA;

    return 0;
}

int dec_fill_next_input_buffer(AVCodecContext* avctx, OMX_BUFFERHEADERTYPE* buf)
{
    AVPacket pkt = { 0 };
    int ret = ff_decode_get_packet(avctx, &pkt);
    buf->nFlags = 0;

    if (ret < 0 || pkt.size == 0) {
        av_packet_unref(&pkt);
        buf->nFlags |= ret == AVERROR_EOF ? OMX_BUFFERFLAG_EOS : 0;
        return ret < 0 ? ret : AVERROR_INVALIDDATA; // INVALIDDATA when pkt.size == 0 it was taken from binkaudio.c
    }

    av_log(avctx, AV_LOG_TRACE,"fill_next_input_buffer: %d size %d pts: %lld dts: %lld duration: %d %s\n", pkt.pos, pkt.size, pkt.pts, pkt.dts, pkt.duration, av_err2str(ret));

    memcpy(buf->pBuffer, pkt.data, pkt.size);
    buf->nFilledLen = (OMX_U32)pkt.size;
    buf->nTimeStamp = to_omx_ticks(pkt.pts);

    fill_extradata_dts(buf, pkt.dts, pkt.duration);

    av_packet_unref(&pkt);

    return 0;
}
