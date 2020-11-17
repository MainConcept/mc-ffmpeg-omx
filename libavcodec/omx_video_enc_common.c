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

#include "libavutil/eval.h"
#include "omx_video_enc_common.h"

#define MAX(x, y) ((x)>(y) ? (x) : (y))

int frame_to_buffer(AVCodecContext *avctx, OMX_BUFFERHEADERTYPE* buf, const AVFrame *fr)
{
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
    if (!(pix_desc->log2_chroma_h <= 1 && pix_desc->log2_chroma_w == 1))
        return AVERROR_INVALIDDATA; // Only 4:2:0 and 4:2:2 formats are supported

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

    buf->nTimeStamp = to_omx_ticks(fr->pts * 1000000 * avctx->time_base.num / avctx->time_base.den); //TODO: check it
    buf->nFilledLen = luma_sz + 2 * chroma_sz;

    if (fr->pict_type == AV_PICTURE_TYPE_I) // ffmpeg set this type on input frame when -force-key_frames is used ("-force_key_frames expr:gte\(t,n_forced*2\)")
        buf->nFlags |= OMX_BUFFERFLAG_SYNCFRAME;

    //    printf("buf_ts %i, frame pts %i\n", from_omx_ticks(buf->nTimeStamp), fr->pts);

    return 0;
}

int omx_send_frame(AVCodecContext *avctx, const AVFrame *frame)
{
    OMXComponentContext *s = avctx->priv_data;

    /*    // This wait shouldn't block. See explanation in omx_receive_packet()
        OMX_BUFFERHEADERTYPE* buf = omx_wait_input_buffer(s);//if !buf and return 0 frame will lost*/

        // Code below is for debug purpose. Code above is correct one.
    OMX_BUFFERHEADERTYPE* buf = omx_pick_input_buffer(s);

    if (!buf) {
        //av_log(avctx, AV_LOG_INFO, "There is no buffer in input queue.\n");
        buf = omx_wait_input_buffer(s);
    }

    if (!frame) { // TODO : Stop encoding or flush flag
        //av_log(avctx, AV_LOG_INFO, "Empty input frame\n");
        buf->nFlags |= OMX_BUFFERFLAG_EOS;
        s->eos_flag = OMX_TRUE;
    } else {
        int ret = frame_to_buffer(avctx, buf, frame);
        if (ret < 0) {
            OMX_EmptyThisBuffer(s->component, buf);
            return ret;
        }
    }

    OMX_EmptyThisBuffer(s->component, buf);

    return 0;
}


int parse_extradata(OMX_BUFFERHEADERTYPE* buf, int64_t *dts_omx)
{
    uint64_t offset = (buf->nOffset + buf->nFilledLen + 0x03L) & ~0x03L;
    if (buf->nFlags & OMX_BUFFERFLAG_EXTRADATA && buf->nAllocLen - (offset + sizeof(OMX_OTHER_EXTRADATATYPE)) > 0) {
        OMX_OTHER_EXTRADATATYPE *pExtra = (OMX_OTHER_EXTRADATATYPE *)(buf->pBuffer + offset);
        while (pExtra->eType != OMX_ExtraDataNone && buf->nAllocLen - (offset + pExtra->nSize) > 0)
        {
            switch (pExtra->eType) {
                case OMX_ExtraDataDTS:
                    *dts_omx = from_omx_ticks(((TIMESTAMP_PARAM*)pExtra->data)->DTS);
                default:
                    break;
            }
            offset += (pExtra->nSize + 0x03L) & ~0x03L;
            pExtra = (OMX_OTHER_EXTRADATATYPE *)(buf->pBuffer + offset);
        }
    }
    return 0;
}

int buffer_to_packet(AVCodecContext *avctx, AVPacket *avpkt, OMX_BUFFERHEADERTYPE* buf)
{
    OMXComponentContext *s = avctx->priv_data;

    int ret = 0;

    if (buf->nFlags & OMX_BUFFERFLAG_CODECCONFIG) { // Codecconfig will be attached to the next buffer with data. Otherwise ffmpeg will report error about NOPTS and TS muxer won't work due to empty packet.
        av_free(s->codec_config);
        s->codec_config = av_malloc(buf->nFilledLen);
        memcpy(s->codec_config, buf->pBuffer + buf->nOffset, buf->nFilledLen);

        return AVERROR(EAGAIN);
    }

    ret = av_new_packet(avpkt, buf->nFilledLen); // TODO : look at v4l2_m2m_enc.c and implement reference counting packet. Or check packet buffer size and avoid each iteration allocation.
    avpkt->flags &= ~AV_PKT_DATA_NEW_EXTRADATA;
    memcpy(avpkt->data, buf->pBuffer + buf->nOffset, buf->nFilledLen);
    avpkt->size = buf->nFilledLen;

    if (s->codec_config) {
        uint8_t* p = av_packet_new_side_data(avpkt, AV_PKT_DATA_NEW_EXTRADATA, buf->nFilledLen); // adding padding for extradata isn't neceessary here as it is added by packet allocator
        avpkt->flags |= AV_PKT_DATA_NEW_EXTRADATA;
        memcpy(p, buf->pBuffer + buf->nOffset, buf->nFilledLen);
        av_free(s->codec_config);
        s->codec_config = NULL;
    }

    if (buf->nFlags & OMX_BUFFERFLAG_TIMESTAMPINVALID) {
        avpkt->pts = AV_NOPTS_VALUE;
        avpkt->dts = AV_NOPTS_VALUE;
    } else {
        const int64_t getting_pts =
                (from_omx_ticks(buf->nTimeStamp) * (int64_t)avctx->time_base.den + 1000000LL * (int64_t)avctx->time_base.num / 2) /
                        (1000000LL * avctx->time_base.num);
        int64_t getting_dts = 0;

        int64_t dts_omx = AV_NOPTS_VALUE;
        parse_extradata(buf, &dts_omx);

        getting_dts = (llabs(dts_omx) * (int64_t)avctx->time_base.den + 1000000LL * avctx->time_base.num / 2) /
                                    (1000000LL * avctx->time_base.num);
        getting_dts = dts_omx > 0 ? getting_dts : -getting_dts;

        avpkt->pts = getting_pts;
        avpkt->dts = getting_dts;

//        printf("pts %i, dts %i, dts_omx %i, pts %i\n", (int)getting_pts, (int)getting_dts, (int)dts_omx, (int)from_omx_ticks(buf->nTimeStamp));
    }

    if (buf->nFlags & OMX_BUFFERFLAG_SYNCFRAME)
        avpkt->flags |= AV_PKT_FLAG_KEY;
    else
        avpkt->flags &= ~AV_PKT_FLAG_KEY;

    return ret;
}

static int convert_buffer(AVCodecContext* avctx, OMX_BUFFERHEADERTYPE** out_buf, AVPacket* avpkt, int* buffer_eos_flag)
{
    OMXComponentContext *s = avctx->priv_data;

    int ret = 0;

    avctx->frame_size = (*out_buf)->nFilledLen;

    ret = buffer_to_packet(avctx, avpkt, *out_buf);

    *buffer_eos_flag = (*out_buf)->nFlags & OMX_BUFFERFLAG_EOS;
    (*out_buf)->nFilledLen = 0;
    OMX_FillThisBuffer(s->component, *out_buf);
    *out_buf = NULL;

    return ret;
}

int omx_receive_packet(AVCodecContext *avctx, AVPacket *avpkt)
{
    OMXComponentContext *s = avctx->priv_data;
    int ret = 0;
    int buffer_eos_flag = 0;

    OMX_BUFFERHEADERTYPE *out_buf = NULL;
    OMX_BUFFERHEADERTYPE *in_buf  = NULL;

    omx_wait_any_buffer(avctx, &out_buf, &in_buf);

    if (s->eos_flag) {
        if (in_buf) { // Send buffer back, if eos flag is set, we won't have anything to proceed.
            in_buf->nFlags |= OMX_BUFFERFLAG_EOS;
            OMX_EmptyThisBuffer(s->component, in_buf);
            in_buf = NULL;
        }

        if (!out_buf)
            out_buf = omx_wait_output_buffer(s); // TODO : If there is no out buffer we will stay in this loop forever.
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
// the first buffer we received while flushing is codec config. But it has no payload. So we have to wait for the next buffer (and we expect it will be payload buffer, otherwise it won't work). But we can't
// wait for output payload buffer when stream wasn't ended because encoder could send codec config before any input frames were consumed.
        if (avpkt->size == 0 && s->eos_flag) {
            out_buf = omx_wait_output_buffer(s); // TODO : If there is no out buffer we will stay in this loop forever.

            ret = convert_buffer(avctx, &out_buf, avpkt, &buffer_eos_flag);
        }

        return buffer_eos_flag ? AVERROR_EOF : ret;
    } else {
        omx_put_input_buffer(s, in_buf);
        return AVERROR(EAGAIN);
    }
}

av_cold void omx_append_parameter(char* dst, int dst_len, const char* format, ...)
{
    const int bufsize = MAX_ARG_STRLEN;
    char tmp[MAX_ARG_STRLEN];

    va_list args;
    va_start(args, format);
    vsnprintf(tmp, bufsize, format, args);
    va_end(args);

    strncat(dst, tmp, dst_len - strlen(dst) - 1); // -1 is for terminating NULL character
}

OMX_COLOR_FORMATTYPE pix_format_to_OMX(enum AVPixelFormat fmt)
{
    switch(fmt) {
        case AV_PIX_FMT_YUV420P:        return OMX_COLOR_FormatYUV420PackedPlanar;
        case AV_PIX_FMT_NV12:           return OMX_COLOR_FormatYUV420PackedSemiPlanar;
        case AV_PIX_FMT_YUV420P10LE:    return OMX_COLOR_FormatYUV420PackedPlanar10bit;
        case AV_PIX_FMT_YUV422P:        return OMX_COLOR_FormatYUV422PackedPlanar;
        case AV_PIX_FMT_YUV422P10LE:    return OMX_COLOR_FormatYUV422PackedPlanar10bit;
        default: break;
    }

    return OMX_COLOR_FormatUnused;
}

av_cold int omx_populate_extradata(AVCodecContext *avctx, OMX_BUFFERHEADERTYPE* buf)
{
    int ret = 0;

    if (avctx->flags & AV_CODEC_FLAG_GLOBAL_HEADER) {
        av_free(avctx->extradata);
        avctx->extradata = NULL;

        avctx->extradata_size = buf->nFilledLen;
        ret = av_reallocp(&avctx->extradata, avctx->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);

        if (ret < 0) {
            avctx->extradata_size = 0;
            return AVERROR(ENOMEM);
        }

        memcpy(avctx->extradata + avctx->extradata_size, buf->pBuffer + buf->nOffset, buf->nFilledLen);
        avctx->extradata_size += buf->nFilledLen;
        memset(avctx->extradata + avctx->extradata_size, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    }

    return 0;
}

av_cold int omx_set_pic_param(AVCodecContext *avctx)
{
    int ret = 0;
    OMXComponentContext *s = avctx->priv_data;

    OMX_PARAM_PORTDEFINITIONTYPE port_definition;
    INIT_STRUCT(port_definition);

    port_definition.nPortIndex = s->nStartPortNumber + s->in_port_idx;

    OMX_GetParameter(s->component, OMX_IndexParamPortDefinition, &port_definition);

    assert(port_definition.eDomain == OMX_PortDomainVideo);

    port_definition.format.video.nFrameWidth  = avctx->width;
    port_definition.format.video.nFrameHeight = avctx->height;
    port_definition.format.video.eColorFormat = pix_format_to_OMX(avctx->pix_fmt);
    port_definition.format.video.xFramerate   = avctx->framerate.num / (double)avctx->framerate.den * Q16_SHIFT;

    ret = OMX_SetParameter(s->component, OMX_IndexParamPortDefinition, &port_definition);
    OMX_ERROR_CHECK(ret, avctx)

    return 0;
}

av_cold int profile_to_omx(enum AVPixelFormat pix_fmt, int profile)
{
    switch (profile) {
        case FF_PROFILE_H264_BASELINE : return OMX_VIDEO_AVCProfileBaseline;
        case FF_PROFILE_H264_MAIN     : return OMX_VIDEO_AVCProfileMain;
        case FF_PROFILE_H264_EXTENDED : return OMX_VIDEO_AVCProfileExtended;
        case FF_PROFILE_H264_HIGH     : return OMX_VIDEO_AVCProfileHigh;
        case FF_PROFILE_H264_HIGH_10  : return OMX_VIDEO_AVCProfileHigh10;
        case FF_PROFILE_H264_HIGH_422 : return OMX_VIDEO_AVCProfileHigh422;
        case FF_PROFILE_H264_HIGH_444 : return OMX_VIDEO_AVCProfileHigh444;
    }

    switch (pix_fmt) {
        case AV_PIX_FMT_YUV420P10LE : return OMX_VIDEO_AVCProfileHigh10;
        case AV_PIX_FMT_YUV422P10LE : return OMX_VIDEO_AVCProfileHigh422;
        case AV_PIX_FMT_YUV422P     : return OMX_VIDEO_AVCProfileHigh422; // It's based on SLACK message 10 Jun 2020, "Thomas 2:57 PM Corrected correction: When encoding 8bit 422 we need: HEVC: set HEVC_MAIN_422_10 preset and bit_depth=8; ..."
    }

    return 0;
}

av_cold int level_to_omx(const char* level)
{
    char *tail = NULL;
    int level_id = -1;

    if (!level)
        return 0;

    if (!strcmp(level, "1b")) { // from libx264.c
        return OMX_VIDEO_AVCLevel1b;
    } else if (strlen(level) <= 3) {
        level_id = av_strtod(level, &tail) * 10 + 0.5;
        if (*tail)
            level_id = -1;
    }

    if (level_id <= 0)
        return 0;

    switch(level_id) {
        case 10 : return OMX_VIDEO_AVCLevel1;
        case 11 : return OMX_VIDEO_AVCLevel11;
        case 12 : return OMX_VIDEO_AVCLevel2;
        case 13 : return OMX_VIDEO_AVCLevel3;
        case 20 : return OMX_VIDEO_AVCLevel2;
        case 21 : return OMX_VIDEO_AVCLevel21;
        case 22 : return OMX_VIDEO_AVCLevel22;
        case 30 : return OMX_VIDEO_AVCLevel3;
        case 31 : return OMX_VIDEO_AVCLevel31;
        case 32 : return OMX_VIDEO_AVCLevel32;
        case 40 : return OMX_VIDEO_AVCLevel4;
        case 41 : return OMX_VIDEO_AVCLevel41;
        case 42 : return OMX_VIDEO_AVCLevel42;
        case 50 : return OMX_VIDEO_AVCLevel5;
        case 51 : return OMX_VIDEO_AVCLevel51;
    }

    return 0;
}

av_cold int omx_set_avc_param(AVCodecContext *avctx, const char* level)
{
    int ret = 0;
    OMXComponentContext *s = avctx->priv_data;

    OMX_VIDEO_PARAM_BITRATETYPE bitrate;
    OMX_VIDEO_PARAM_AVCTYPE avc_param;

    INIT_STRUCT(bitrate);
    INIT_STRUCT(avc_param);

    bitrate.eControlRate = avctx->rc_min_rate == avctx->rc_max_rate ? OMX_Video_ControlRateConstant : OMX_Video_ControlRateVariable;
    bitrate.nTargetBitrate = avctx->bit_rate;
    bitrate.nPortIndex = s->nStartPortNumber + s->out_port_idx;

    ret = OMX_SetParameter(s->component, OMX_IndexParamVideoBitrate, &bitrate);
    OMX_ERROR_CHECK(ret, avctx)

    avc_param.nPortIndex = s->nStartPortNumber + s->out_port_idx;
    avc_param.nSliceHeaderSpacing = 0;
    avc_param.nPFrames = avctx->gop_size     >= 0 ? avctx->gop_size - 1 : UINT_MAX;
    avc_param.nBFrames = avctx->max_b_frames >= 0 ? avctx->max_b_frames : UINT_MAX;
    avc_param.bUseHadamard = OMX_TRUE;
    avc_param.nRefFrames = avctx->refs >= 0 ? avctx->refs : UINT_MAX;
    avc_param.nRefIdx10ActiveMinus1 = avctx->refs >= 0 ? MAX((int)avc_param.nRefFrames - 1, 0) : UINT_MAX;
    avc_param.nRefIdx11ActiveMinus1 = avctx->refs >= 0 ? MAX((int)avc_param.nRefFrames - 1, 0) : UINT_MAX;
    avc_param.bEnableUEP = OMX_FALSE;
    avc_param.bEnableFMO = OMX_FALSE;
    avc_param.bEnableASO = OMX_FALSE;
    avc_param.bEnableRS = OMX_FALSE;
    avc_param.eProfile = profile_to_omx(avctx->pix_fmt, s->profile);
    avc_param.eLevel = level_to_omx(level);
    avc_param.nAllowedPictureTypes = OMX_VIDEO_PictureTypeI | OMX_VIDEO_PictureTypeP | OMX_VIDEO_PictureTypeB;
    avc_param.bFrameMBsOnly = !(avctx->flags & AV_CODEC_FLAG_INTERLACED_DCT); // It's how interlaced flag is set in libx264
    avc_param.bMBAFF = OMX_TRUE;
    avc_param.bEntropyCodingCABAC = OMX_TRUE;
    avc_param.bWeightedPPrediction = OMX_FALSE;
    avc_param.nWeightedBipredicitonMode = 0;
    avc_param.bconstIpred = OMX_FALSE;
    avc_param.bDirect8x8Inference = OMX_TRUE;
    avc_param.bDirectSpatialTemporal = OMX_FALSE;
    avc_param.nCabacInitIdc = 0;
    avc_param.eLoopFilterMode = OMX_VIDEO_AVCLoopFilterEnable;

    ret = OMX_SetParameter(s->component, OMX_IndexParamVideoAvc, &avc_param);
    OMX_ERROR_CHECK(ret, avctx)

    return 0;
}

av_cold int omx_set_commandline(AVCodecContext *avctx)
{
    int ret = 0;
    OMXComponentContext *s = avctx->priv_data;

    OMX_VENDOR_PARAM_COMMANDLINETYPE *commandline;

    int buffer_size = 0;
    int struct_size = 0;

    if (!s->component_param)
        return 0;

    buffer_size = strlen(s->component_param);
    struct_size = sizeof(OMX_VENDOR_PARAM_COMMANDLINETYPE) + buffer_size;

    commandline = malloc(struct_size);
    INIT_STRUCT(*commandline);

    commandline->nSize = struct_size;
    commandline->nCommandlineMaxSize = buffer_size;
    commandline->nCommandlineSizeUsed = buffer_size;

    memcpy(commandline->data, s->component_param, buffer_size);

    ret = OMX_SetParameter(s->component, OMX_IndexParamVendorCommandline, commandline);
    OMX_ERROR_CHECK(ret, avctx)

    free(commandline);

    return 0;
}
