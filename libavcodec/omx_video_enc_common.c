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
#include "omx_video_enc_common.h"

static enum AVPictureType omxpic_to_avpic(OMX_VIDEO_PICTURETYPE pix_t)
{
    switch (pix_t) {
    case OMX_VIDEO_PictureTypeI:                 return AV_PICTURE_TYPE_I;
    case OMX_VIDEO_PictureTypeP:                 return AV_PICTURE_TYPE_P;
    case OMX_VIDEO_PictureTypeB:                 return AV_PICTURE_TYPE_B;
    case OMX_VIDEO_PictureTypeSI:                return AV_PICTURE_TYPE_SI;
    case OMX_VIDEO_PictureTypeSP:                return AV_PICTURE_TYPE_SP;
    case OMX_VIDEO_PictureTypeS:                 return AV_PICTURE_TYPE_S;
    default: break;
    }
    return AV_PICTURE_TYPE_NONE;
}

static AVRational xFramerate_to_scale(const OMX_U32 xFramerate)
{
    AVRational r = { 0,1 };
    switch (xFramerate) {
    case 14:
    case 14000:
        r = (AVRational){ 14000, 1000 };
        break;
    case 15:
    case 15000:
        r = (AVRational){ 15000, 1000 };
        break;
    case 23976:
        r = (AVRational){ 24000, 1001 };
        break;
    case 29970:
        r = (AVRational){ 30000, 1001 };
        break;
    case 59940: //59.940
        r = (AVRational){ 60000, 1001 };
        break;
    case 119880: //119.880
        r = (AVRational){ 120000, 1001 };
        break;
    case 24:
    case 24000:
        r = (AVRational){ 24000, 1000 };
        break;
    case 25:
    case 25000:
        r = (AVRational){ 25000, 1000 };
        break;
    case 50:
    case 50000:
        r = (AVRational){ 50000, 1000 };
        break;
    case 100:
    case 100000:
        r = (AVRational){ 100000, 1000 };
        break;
    case 120:
    case 120000:
        r = (AVRational){ 120000, 1000 };
        break;
    case 200:
    case 200000:
        r = (AVRational){ 200000, 1000 };
        break;
    case 239760:
        r = (AVRational){ 240000, 1001 };
        break;
    }
    return r;
}

static enum AVPixelFormat OMX_to_pix_format(const OMX_COLOR_FORMATTYPE fmt)
{
    switch (fmt) {
    case OMX_COLOR_FormatYUV420PackedPlanar:        return AV_PIX_FMT_YUV420P;
    case OMX_COLOR_FormatYUV420PackedSemiPlanar:    return AV_PIX_FMT_NV12;
    case OMX_COLOR_FormatYUV420PackedPlanar10bit:   return AV_PIX_FMT_YUV420P10LE;
    case OMX_COLOR_FormatYUV422PackedPlanar:        return AV_PIX_FMT_YUV422P;
    case OMX_COLOR_FormatYUV422PackedPlanar10bit:   return AV_PIX_FMT_YUV422P10LE;
    case OMX_COLOR_FormatYUV420Planar:              return AV_PIX_FMT_YUV420P;
    }
    return AV_PIX_FMT_NONE;
}

OMX_COLOR_FORMATTYPE pix_format_to_OMX(enum AVPixelFormat fmt)
{
    switch (fmt) {
    case AV_PIX_FMT_YUV420P:        return OMX_COLOR_FormatYUV420PackedPlanar;
    case AV_PIX_FMT_NV12:           return OMX_COLOR_FormatYUV420PackedSemiPlanar;
    case AV_PIX_FMT_YUV420P10LE:    return OMX_COLOR_FormatYUV420PackedPlanar10bit;
    case AV_PIX_FMT_YUV422P:        return OMX_COLOR_FormatYUV422PackedPlanar;
    case AV_PIX_FMT_YUV422P10LE:    return OMX_COLOR_FormatYUV422PackedPlanar10bit;
    case AV_PIX_FMT_GBRP10LE:       return OMX_COLOR_FormatBGRPackedPlanar10bit;
    default: break;
    }
    return OMX_COLOR_FormatUnused;
}

static av_cold int profile_to_omx(enum AVPixelFormat pix_fmt, int profile)
{
    switch (profile) {
    case FF_PROFILE_H264_BASELINE: return OMX_VIDEO_AVCProfileBaseline;
    case FF_PROFILE_H264_MAIN:     return OMX_VIDEO_AVCProfileMain;
    case FF_PROFILE_H264_EXTENDED: return OMX_VIDEO_AVCProfileExtended;
    case FF_PROFILE_H264_HIGH:     return OMX_VIDEO_AVCProfileHigh;
    case FF_PROFILE_H264_HIGH_10:  return OMX_VIDEO_AVCProfileHigh10;
    case FF_PROFILE_H264_HIGH_422: return OMX_VIDEO_AVCProfileHigh422;
    case FF_PROFILE_H264_HIGH_444: return OMX_VIDEO_AVCProfileHigh444;
    }
 
    switch (pix_fmt) {
    case AV_PIX_FMT_YUV420P10LE: return OMX_VIDEO_AVCProfileHigh10;
    case AV_PIX_FMT_YUV422P10LE: return OMX_VIDEO_AVCProfileHigh422;
    case AV_PIX_FMT_YUV422P:     return OMX_VIDEO_AVCProfileHigh422;
    }
 
     return 0;
}

static enum AVColorRange OMX_to_AV_color_range(const OMX_COLOR_RANGE fmt)
{
    switch (fmt) {
    case RangeUnspecified: return AVCOL_RANGE_UNSPECIFIED;
    case RangeFull:        return AVCOL_RANGE_MPEG;
    case RangeLimited:     return AVCOL_RANGE_JPEG;
    }
    return AVCOL_RANGE_NB;
}

static enum AVColorPrimaries OMX_to_AV_color_primaries(const OMX_COLOR_PRIMARIES fmt)
{
    switch (fmt) {
    case PrimariesBT709_6:
    case PrimariesBT709_5:       return AVCOL_PRI_BT709;

    case OMX_COLOR_FormatYUV420PackedPlanar: return AVCOL_PRI_RESERVED0;
    case PrimariesUnspecified:               return AVCOL_PRI_UNSPECIFIED;
    case PrimariesOther:                     return AVCOL_PRI_RESERVED;
    case PrimariesBT470_6M:                  return AVCOL_PRI_BT470M;
    case PrimariesBT470_6BG:                 return AVCOL_PRI_BT470BG;
    case Primaries_SMPTE_170M:               return AVCOL_PRI_SMPTE170M;
    case Primaries_SMPTE_240M:               return AVCOL_PRI_SMPTE240M;
    case PrimariesGenericFilm:               return AVCOL_PRI_FILM;
    case PrimariesBT2020:                    return AVCOL_PRI_BT2020;
    case Primaries_SMPTEST428_1:             return AVCOL_PRI_SMPTEST428_1;
    }
    return AVCOL_PRI_NB;
}

static enum AVColorTransferCharacteristic OMX_to_AV_color_trc(const OMX_COLOR_TRANSFER fmt)
{
    switch (fmt) {
    case TransferUnspecified:   return AVCOL_TRC_UNSPECIFIED;
    case TransferSMPTE170M:     return AVCOL_TRC_SMPTE170M;
                                return AVCOL_TRC_BT709;
    case TransferST2084:        return AVCOL_TRC_SMPTEST2084;
    case TransferSMPTE240M:     return AVCOL_TRC_SMPTE240M;
    case TransferST428:         return AVCOL_TRC_SMPTEST428_1;
    case TransferLinear:        return AVCOL_TRC_LINEAR;
    case Transfer_BT2020_10:    return AVCOL_TRC_BT2020_10;
    case Transfer_BT2020_12:    return AVCOL_TRC_BT2020_12;
    case Transfer_IEC61966_2_1: return AVCOL_TRC_IEC61966_2_1;
    case Transfer_IEC61966_2_4: return AVCOL_TRC_IEC61966_2_4;
    case Transfer_BT1361_0:     return AVCOL_TRC_BT1361_ECG;
    }
    return AVCOL_TRC_NB;
}

static enum AVColorSpace OMX_to_AV_colorspace(const OMX_COLOR_MATRIX_COEFFS fmt)
{
    switch (fmt) {
    case MatrixRGB:         return AVCOL_SPC_RGB;
    case MatrixBT709_6:     return AVCOL_SPC_BT709;
    case MatrixUnspecified: return AVCOL_SPC_UNSPECIFIED;
    case MatrixFCC:         return AVCOL_SPC_FCC;
    case MatrixBT470_6M:
    case MatrixBT470_6BG:   return AVCOL_SPC_BT470BG;
    case MatrixBT601_6:     return AVCOL_SPC_SMPTE170M;
    case MatrixSMPTE240M:   return AVCOL_SPC_SMPTE240M;
    case MatrixYCGCO:       return AVCOL_SPC_YCOCG;
    }
    return AVCOL_SPC_NB;
}

av_cold int level_to_omx(const char* level)
{
    char* tail = NULL;
    int level_id = -1;

    if (!level)
        return 0;

    if (!strcmp(level, "1b")) { // from libx264.c
        return OMX_VIDEO_AVCLevel1b;
    }
    else if (strlen(level) <= 3) {
        level_id = av_strtod(level, &tail) * 10 + 0.5;
        if (*tail)
            level_id = -1;
    }

    if (level_id <= 0)
        return 0;
    switch (level_id) {
    case 10: return OMX_VIDEO_AVCLevel1;
    case 11: return OMX_VIDEO_AVCLevel11;
    case 12: return OMX_VIDEO_AVCLevel2;
    case 13: return OMX_VIDEO_AVCLevel3;
    case 20: return OMX_VIDEO_AVCLevel2;
    case 21: return OMX_VIDEO_AVCLevel21;
    case 22: return OMX_VIDEO_AVCLevel22;
    case 30: return OMX_VIDEO_AVCLevel3;
    case 31: return OMX_VIDEO_AVCLevel31;
    case 32: return OMX_VIDEO_AVCLevel32;
    case 40: return OMX_VIDEO_AVCLevel4;
    case 41: return OMX_VIDEO_AVCLevel41;
    case 42: return OMX_VIDEO_AVCLevel42;
    case 50: return OMX_VIDEO_AVCLevel5;
    case 51: return OMX_VIDEO_AVCLevel51;
    }
 
    return 0;
}

static int fill_extradata_dts(OMX_BUFFERHEADERTYPE* buf, int64_t dts, int64_t duration)
{
    uint64_t offset = omx_get_ext_pos(buf->pBuffer, buf->nOffset + buf->nFilledLen);

    OMX_OTHER_EXTRADATATYPE* dts_ext = buf->pBuffer + offset;
    INIT_STRUCT(*dts_ext);

    TIMESTAMP_PARAM current_time_param;
    current_time_param.DTS = dts;
    current_time_param.duration = duration;

    dts_ext->eType = OMX_ExtraDataDTS;
    dts_ext->nDataSize = sizeof(TIMESTAMP_PARAM);
    dts_ext->nSize += dts_ext->nDataSize;

    memcpy(dts_ext->data, &current_time_param, dts_ext->nDataSize);

    offset += omx_get_ext_pos(buf->pBuffer + offset, dts_ext->nSize);

    OMX_OTHER_EXTRADATATYPE last_empty_extra;
    INIT_STRUCT(last_empty_extra);
    last_empty_extra.eType = OMX_ExtraDataNone;

    memcpy(buf->pBuffer + offset, (uint8_t*)&last_empty_extra, last_empty_extra.nSize);
    buf->nFlags |= OMX_BUFFERFLAG_EXTRADATA;

    return 0;
}

typedef struct extra_options {            // flags
    TIMESTAMP_PARAM       dts;            // 1
    OMX_U64               seek_pos;       // 2
    OMX_ASPECT_RATIO      aspect_ratio;   // 4
    OMX_VIDEO_PICTURETYPE picture_type;   // 8
    OMX_INTERLACES        interlace_type; // 16
    OMX_COLOR_ASPECT      color_aspect;   // 32

    uint64_t flags;
} extra_options_t;


typedef enum extra_options_type {
    dts_ext_flag            = 1,
    seek_pos_ext_flag       = 2,
    aspect_ratio_ext_flag   = 4,
    picture_type_ext_flag   = 8,
    interlace_type_ext_flag = 16,
    color_aspect_ext_flag   = 32
} extra_options_type;


static int parse_all_extradata(OMX_BUFFERHEADERTYPE* buf, extra_options_t* duration_omx)
{
    uint64_t offset = (buf->nOffset + buf->nFilledLen + 0x03L) & ~0x03L;
    duration_omx->flags = 0;
 
    if (buf->nFlags & OMX_BUFFERFLAG_EXTRADATA && buf->nAllocLen - (offset + sizeof(OMX_OTHER_EXTRADATATYPE)) > 0) {
        OMX_OTHER_EXTRADATATYPE* pExtra = (OMX_OTHER_EXTRADATATYPE*)(buf->pBuffer + offset);
        while (pExtra->eType != OMX_ExtraDataNone && buf->nAllocLen - (offset + pExtra->nSize) > 0)
        {
            switch (pExtra->eType) {
            case (OMX_EXTRADATATYPE)OMX_ExtraDataDTS:
                duration_omx->dts = *((TIMESTAMP_PARAM*)pExtra->data);                duration_omx->flags |= dts_ext_flag; break;
            case OMX_ExtraDataSeekInfo:
                duration_omx->seek_pos = *((OMX_U64*)pExtra->data);                   duration_omx->flags |= seek_pos_ext_flag; break;
            case OMX_ExtraDataAspectRatio:
                duration_omx->aspect_ratio = *((OMX_ASPECT_RATIO*)pExtra->data);      duration_omx->flags |= aspect_ratio_ext_flag; break;
            case OMX_ExtraDataVideoPictureType:
                duration_omx->picture_type = *((OMX_VIDEO_PICTURETYPE*)pExtra->data); duration_omx->flags |= picture_type_ext_flag; break;
            case OMX_ExtraDataInterlaceFormat:
                duration_omx->interlace_type = *((OMX_INTERLACES*)pExtra->data);      duration_omx->flags |= interlace_type_ext_flag; break;
            case OMX_ExtraDataColorAspect:
                duration_omx->color_aspect = *((OMX_COLOR_ASPECT*)pExtra->data);      duration_omx->flags |= color_aspect_ext_flag; break;
            case OMX_ExtraDataA53CC:
                break;
            default:
                break;
            }
            offset += (pExtra->nSize + 0x03L) & ~0x03L;
            pExtra = (OMX_OTHER_EXTRADATATYPE*)(buf->pBuffer + offset);
        }
    }
    return 0;
}


int dec_fill_next_input_buffer(AVCodecContext* avctx, OMX_BUFFERHEADERTYPE* buf)
{
    AVPacket pkt = { 0 };
    int ret = ff_decode_get_packet(avctx, &pkt);
    buf->nFlags = 0;

    if (ret < 0 || pkt.size == 0) { // && ret != AVERROR_EOF) {
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

static int omx_get_pic_param(OMXComponentContext *omxctx)
{
    AVCodecContext *avctx = omxctx->avctx;

    OMX_PARAM_PORTDEFINITIONTYPE port_definition;
    INIT_STRUCT(port_definition);

    int out_port_idx = omxctx->port_idx[omx_port_idx(omxctx, 1)];

    port_definition.nPortIndex = out_port_idx;

    OMX_ERRORTYPE rt = OMX_GetParameter(omxctx->component, OMX_IndexParamPortDefinition, &port_definition);

    assert(port_definition.eDomain == OMX_PortDomainVideo);

    avctx->width = port_definition.format.video.nFrameWidth;
    avctx->height = port_definition.format.video.nFrameHeight;
    avctx->pix_fmt = OMX_to_pix_format( port_definition.format.video.eColorFormat );
    avctx->framerate = xFramerate_to_scale( port_definition.format.video.xFramerate );

    return 0;
}

static void* add_captured_buffer(OMXComponentContext* ctx, OMX_BUFFERHEADERTYPE* buffer)
{
    OMXCapturedBuffer* p = av_malloc(sizeof(OMXCapturedBuffer));
    memset(p, 0, sizeof(OMXCapturedBuffer));

    p->buffer = buffer;
    p->ctx = ctx;

    if (ctx->captured_buffers_tail) {
        ctx->captured_buffers_tail->next = p;
        p->prev = ctx->captured_buffers_tail;
    }

    ctx->captured_buffers_tail = p;

    return p;
}

static void remove_captured_buffer(OMXCapturedBuffer* p)
{
    if (p->next)
        p->next->prev = p->prev;
    else if (p->ctx) // it's the last buffer
        p->ctx->captured_buffers_tail = p->prev;

    if (p->prev)
        p->prev->next = p->next;

    p->buffer = NULL;
    p->ctx = NULL;

    av_free(p);
}

static void free_omx_buffer(void* p, uint8_t* data)
{
    OMXCapturedBuffer* buf = p;

    if (buf->buffer) { // Otherwise it means that this buffer was already sent because component was stopped before buffer deallocation
        buf->buffer->nFilledLen = 0;
        OMX_FillThisBuffer(buf->ctx->component, buf->buffer);
    }

    remove_captured_buffer(buf);
}

int dec_buffer_to_frame(OMXComponentContext *omxctx, AVFrame* fr, OMX_BUFFERHEADERTYPE* buf)
{
    int ret = 0;
    AVCodecContext *avctx = omxctx->avctx;

    omx_get_pic_param(omxctx);

    fr->width  = FFMAX(avctx->width,  AV_CEIL_RSHIFT(avctx->coded_width,  avctx->lowres));
    fr->height = FFMAX(avctx->height, AV_CEIL_RSHIFT(avctx->coded_height, avctx->lowres));

    void* p = add_captured_buffer(omxctx, buf);
    fr->buf[0] = av_buffer_create(buf->pBuffer + buf->nOffset, buf->nFilledLen, free_omx_buffer, p, AV_BUFFER_FLAG_READONLY);

    av_image_fill_arrays(fr->data, fr->linesize, buf->pBuffer + buf->nOffset, avctx->pix_fmt, avctx->width, avctx->height, 1);
    ret = ff_decode_frame_props(avctx, fr);
    if (ret)
        return ret;

    ret = ff_attach_decode_data(fr);
    if (ret < 0)
        return ret;

    fr->pts = from_omx_ticks(buf->nTimeStamp);

    //----------------------------------set frame flags

    if (buf->nFlags & OMX_BUFFERFLAG_DATACORRUPT)
        fr->flags |= AV_FRAME_FLAG_CORRUPT;

    extra_options_t extradata;
    parse_all_extradata(buf, &extradata);

    if (extradata.flags & dts_ext_flag) {
        fr->pkt_dts      = from_omx_ticks(extradata.dts.DTS);
        fr->pkt_duration = from_omx_ticks(extradata.dts.duration);
    }
    if (extradata.flags & aspect_ratio_ext_flag) {
        fr->sample_aspect_ratio = (AVRational){ extradata.aspect_ratio.aspectRatioX, extradata.aspect_ratio.aspectRatioY };
    }
    if (extradata.flags & picture_type_ext_flag) {
        fr->pict_type = omxpic_to_avpic(extradata.picture_type);
        fr->key_frame = extradata.picture_type == OMX_VIDEO_PictureTypeI;
    }
    if (extradata.flags & interlace_type_ext_flag) {
        fr->interlaced_frame = extradata.interlace_type &~ OMX_InterlaceFrameProgressive;
        if (fr->interlaced_frame)
            fr->top_field_first = extradata.interlace_type & OMX_InterlaceFrameTopFieldFirst ||
                                  extradata.interlace_type & OMX_InterlaceInterleaveFrameTopFieldFirst ||
                                  extradata.interlace_type & OMX_InterlaceInterleaveFieldTop;
    }
    if (extradata.flags & color_aspect_ext_flag) {
        fr->color_range     = OMX_to_AV_color_range(extradata.color_aspect.mRange);
        fr->color_primaries = OMX_to_AV_color_primaries(extradata.color_aspect.mPrimaries);
        fr->color_trc       = OMX_to_AV_color_trc(extradata.color_aspect.mTransfer);
        fr->colorspace      = OMX_to_AV_colorspace(extradata.color_aspect.mMatrixCoeffs);
    }

    return 0;
}

int dec_omx_receive_frame(OMXComponentContext* s, AVFrame* frame)
{
    AVCodecContext *avctx = s->avctx;

    int ret = 0;
    OMX_BUFFERHEADERTYPE* in_buf = NULL;
    OMX_BUFFERHEADERTYPE* out_buf = NULL;

    if (s->eos_flag)
        return AVERROR_EOF;

    in_buf = omx_pick_input_buffer(s);

    if (!in_buf) {
        // If there is no input buffer, we should wait for output one, because we can't return EAGAIN here (see line 2243 ffmpeg.c)
        ret = omx_wait_any_buffer(avctx, &out_buf, &in_buf);
        if (ret == AVERROR(EINVAL))
            return ret;
        if (out_buf) { // Frame is ready, so just returns immediately
            s->eos_flag = out_buf->nFlags & OMX_BUFFERFLAG_EOS ? 1 : 0; // out_buf->nFlags should be checked before FillThisBuffer is called

            if (out_buf->nFilledLen) {
                ret = dec_buffer_to_frame(s, frame, out_buf);
            } else { // It should be done for empty buffer only, as otherwise buffer will be sent back in free_omx_buffer()
                out_buf->nFilledLen = 0;
                OMX_FillThisBuffer(s->component, out_buf);
            }

            return ret;
        }

        assert(in_buf);
        assert(!out_buf);
    }

    ret = dec_fill_next_input_buffer(avctx, in_buf);
    OMX_EmptyThisBuffer(s->component, in_buf);

    if (ret != AVERROR_EOF && ret < 0)
        return ret;

    out_buf = omx_pick_output_buffer(s);
    if (in_buf->nFlags & OMX_BUFFERFLAG_EOS && !out_buf) // If EOS was set on input buffer, FFMPEG flushes the codec. In this case codec cannot return EAGAIN as there will be no more input data. So, EAGAIN will cause FFMPEG to report decoding error and stop
        out_buf = omx_wait_output_buffer(s);

    if (out_buf) {
        s->eos_flag = out_buf->nFlags & OMX_BUFFERFLAG_EOS ? 1 : 0;

        if (out_buf->nFilledLen) {
            ret = dec_buffer_to_frame(s, frame, out_buf);
        } else { // It should be done for empty buffer only, as otherwise buffer will be sent back in free_omx_buffer()
            out_buf->nFilledLen = 0;
            OMX_FillThisBuffer(s->component, out_buf);
        }

        return ret;
    }

    return AVERROR(EAGAIN);
}

av_cold int omx_set_pic_param(AVCodecContext *avctx)
{
    int ret = 0;
    OMXComponentContext *s = avctx->priv_data;

    OMX_PARAM_PORTDEFINITIONTYPE port_definition;
    INIT_STRUCT(port_definition);

    int in_port_idx = s->port_idx[omx_port_idx(s, 0)];

    port_definition.nPortIndex = in_port_idx;

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

av_cold int omx_set_avc_param(AVCodecContext *avctx, const char* level)
{
    int ret = 0;
    OMXComponentContext *s = avctx->priv_data;

    OMX_VIDEO_PARAM_BITRATETYPE bitrate;
    OMX_VIDEO_PARAM_AVCTYPE avc_param;

    INIT_STRUCT(bitrate);
    INIT_STRUCT(avc_param);

    int out_port_idx = s->port_idx[omx_port_idx(s, 1)];

    bitrate.eControlRate = avctx->rc_min_rate == avctx->rc_max_rate ? OMX_Video_ControlRateConstant : OMX_Video_ControlRateVariable;
    bitrate.nTargetBitrate = avctx->bit_rate;
    bitrate.nPortIndex = out_port_idx;

    ret = OMX_SetParameter(s->component, OMX_IndexParamVideoBitrate, &bitrate);
    OMX_ERROR_CHECK(ret, avctx)

    avc_param.nPortIndex = out_port_idx;
    avc_param.nSliceHeaderSpacing = 0;
    avc_param.nPFrames = avctx->gop_size     >= 0 ? avctx->gop_size - 1 : UINT_MAX;
    avc_param.nBFrames = avctx->max_b_frames >= 0 ? avctx->max_b_frames : UINT_MAX;
    avc_param.bUseHadamard = OMX_TRUE;
    avc_param.nRefFrames = avctx->refs >= 0 ? avctx->refs : UINT_MAX;
    avc_param.nRefIdx10ActiveMinus1 = avctx->refs >= 0 ? OMX_MAX((int)avc_param.nRefFrames - 1, 0) : UINT_MAX;
    avc_param.nRefIdx11ActiveMinus1 = avctx->refs >= 0 ? OMX_MAX((int)avc_param.nRefFrames - 1, 0) : UINT_MAX;
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

int omx_get_codec_config(AVCodecContext *avctx, OMX_HANDLETYPE component)
{
    const size_t codec_config_max_size = 1024;
    const size_t struct_size = sizeof(OMX_VIDEO_PARAM_CODECCONFIGTYPE) + codec_config_max_size;
    OMX_VIDEO_PARAM_CODECCONFIGTYPE *codec_config = malloc(struct_size);
    INIT_STRUCT(*codec_config);

    codec_config->nCodecConfigMaxSize = codec_config_max_size;

    OMX_ERRORTYPE err = OMX_GetParameter(component, OMX_IndexParamVideoCodecConfig, codec_config);

    if (err == OMX_ErrorNone) {
        avctx->extradata_size = codec_config->nCodecConfigSizeUsed;
        avctx->extradata = av_mallocz(avctx->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
        memcpy(avctx->extradata, codec_config->CodecConfig, avctx->extradata_size);
    }

    free(codec_config);

    return 0;
}
