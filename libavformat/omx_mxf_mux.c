/*
 * OMX MXF muxer
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

#include "libavutil/avassert.h"
#include "libavutil/bswap.h"
#include "libavutil/crc.h"
#include "libavutil/dict.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/mathematics.h"
#include "libavutil/opt.h"

#include "libavutil/timecode.h"

#include "libavutil/thread.h"

#include "libavcodec/internal.h"
#include "libavcodec/omx_common.h"

#include "avformat.h"
#include "avio_internal.h"
#include "internal.h"
#include "mxf.h"

extern AVOutputFormat ff_mxf_omx_muxer;
 /**
  * SMPTE RP210 http://www.smpte-ra.org/mdd/index.html
  *             https://smpte-ra.org/sites/default/files/Labels.xml
  */

#define MAX_STREAMS_COUNT 16

typedef struct OMX_MXF_Mux {
    struct OMXComponentContext base;

    AVRational time_base;

    int store_user_comments;
    AVRational audio_edit_rate;

    int stream_idx_to_port[MAX_STREAMS_COUNT];

    AVIOContext* streams[MAX_STREAMS_COUNT];
    int first_out_port;
    int split_track;
    int split_channel;
    int out_port_enabled;
} OMX_MXF_Mux;

typedef struct MXFStreamContext {
    int64_t pkt_cnt;         ///< pkt counter for muxed packets
    UID track_essence_element_key;
    int index;               ///< index in mxf_essence_container_uls table
    const UID* codec_ul;
    const UID* container_ul;
    int order;               ///< interleaving order if dts are equal
    int interlaced;          ///< whether picture is interlaced
    int field_dominance;     ///< tff=1, bff=2
    int component_depth;
    int color_siting;
    int signal_standard;
    int h_chroma_sub_sample;
    int v_chroma_sub_sample;
    int temporal_reordering;
    AVRational aspect_ratio; ///< display aspect ratio
    int closed_gop;          ///< gop is closed, used in mpeg-2 frame parsing
    int video_bit_rate;
    int slice_offset;
    int frame_size;          ///< frame size in bytes
    int seq_closed_gop;      ///< all gops in sequence are closed, used in mpeg-2 descriptor
    int max_gop;             ///< maximum gop size, used by mpeg-2 descriptor
    int b_picture_count;     ///< maximum number of consecutive b pictures, used in mpeg-2 descriptor
    int low_delay;           ///< low delay, used in mpeg-2 descriptor
    int avc_intra;
} MXFStreamContext;

static int disable_unused_ports(AVFormatContext *avctx)
{
    struct OMX_MXF_Mux *s_mux = avctx->priv_data;
    struct OMXComponentContext *s = &s_mux->base;

    int n_streams[AVMEDIA_TYPE_NB] = {0};

    for (int i=0; i<avctx->nb_streams; i++)
        n_streams[avctx->streams[i]->codecpar->codec_type]++;

    static int omx_domain_to_codec_type[] = {AVMEDIA_TYPE_AUDIO, AVMEDIA_TYPE_VIDEO, AVMEDIA_TYPE_DATA, AVMEDIA_TYPE_DATA};

    s_mux->out_port_enabled = s_mux->split_track ? avctx->nb_streams : 1;
    if (s_mux->split_channel) {
        if (s_mux->split_track) {
            s_mux->split_track = 0;
            av_log(avctx, AV_LOG_WARNING, "Don`t use options split_track and split_channel at the same time, select single one. -split_track disabled.\n");
        }

        s_mux->out_port_enabled = 0;
        for (int i = 0; i < avctx->nb_streams; i++) {
            if (avctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
                s_mux->out_port_enabled++;
            else if (avctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
                s_mux->out_port_enabled += avctx->streams[i]->codecpar->channels;
        }
    }

    int out_port_to_enable = s_mux->out_port_enabled;

    for (int i=0; i<OMX_PortDomainOther+1; i++) {
        const int n_ports = s->nPorts[i];
        const int codec_type = omx_domain_to_codec_type[i];

        const int start_n = s->nStartPortNumber[i];
        const int end_n   = s->nStartPortNumber[i] + s->nPorts[i];

        for (int j=start_n + n_streams[codec_type]; j<end_n; j++)
            if (!s->port_out[rev_port_idx(s, j)]) {
                omx_disable_port(s, j);
            } else {
                if (!out_port_to_enable) {
                    omx_disable_port(s, j);
                } else {
                    out_port_to_enable--;
                }
            }
    }

    return 0;
}

static int populate_stream_idx_map(AVFormatContext *avctx)
{
    struct OMX_MXF_Mux *s_mux = avctx->priv_data;
    struct OMXComponentContext *s = &s_mux->base;

    OMX_U32 port_idx[4] = {0};

    for (int i=0; i<4; i++)
        port_idx[i] = s->nStartPortNumber[i];

    static int codec_type_to_domain[] = {OMX_PortDomainVideo, OMX_PortDomainAudio, OMX_PortDomainOther, OMX_PortDomainOther, OMX_PortDomainOther};

    int n = 0;

    for (int i=0; i<avctx->nb_streams; i++) {
        int domain = codec_type_to_domain[avctx->streams[i]->codecpar->codec_type];
        s_mux->stream_idx_to_port[n++] = port_idx[domain] - s->nStartPortNumber[domain] < s->nPorts[domain] ? port_idx[domain]++ : -1;
    }

    return 0;
}

static int omx_set_pcm_param(AVFormatContext* avctx){

    struct OMX_MXF_Mux* s_mux = avctx->priv_data;
    struct OMXComponentContext* s = &s_mux->base;

    int buffer_size = 0;
    int struct_size = 0;
    int ret = 0;

    for (int i = 0; i < avctx->nb_streams; i++) {
        if (avctx->streams[i]->codecpar->codec_id >= AV_CODEC_ID_PCM_S16LE && 
            avctx->streams[i]->codecpar->codec_id <= AV_CODEC_ID_PCM_SGA) {

            const OMX_U32 port_idx = s_mux->stream_idx_to_port[i];

            OMX_PARAM_PORTDEFINITIONTYPE apcp = {0};
            INIT_STRUCT(apcp);
            apcp.nPortIndex = port_idx;
            OMX_ERRORTYPE err = OMX_GetParameter(s->component, OMX_IndexParamPortDefinition, &apcp);
            OMX_ERROR_CHECK(ret, avctx)
            apcp.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
            OMX_SetParameter(s->component, OMX_IndexParamPortDefinition, &apcp);

            AVCodecParameters* cur_codec_par = avctx->streams[i]->codecpar;

            OMX_AUDIO_PARAM_PCMMODETYPE pcm_conf;
            INIT_STRUCT(pcm_conf);

            pcm_conf.nPortIndex = port_idx;

            pcm_conf.nSamplingRate = cur_codec_par->sample_rate;
            pcm_conf.nChannels     = cur_codec_par->channels;
            pcm_conf.nBitPerSample = cur_codec_par->bits_per_coded_sample;

            ret = OMX_SetParameter(s->component, OMX_IndexParamAudioPcm, &pcm_conf);
            OMX_ERROR_CHECK(ret, avctx)
        }
    }

    return 0;
}

static int parse_extradata(OMX_BUFFERHEADERTYPE* buf, int64_t* seek_pos)
{
    uint64_t offset = (buf->nOffset + buf->nFilledLen + 0x03L) & ~0x03L;

    if (buf->nFlags & OMX_BUFFERFLAG_EXTRADATA && buf->nAllocLen - (offset + sizeof(OMX_OTHER_EXTRADATATYPE)) > 0) {
        OMX_OTHER_EXTRADATATYPE* pExtra = (OMX_OTHER_EXTRADATATYPE*)(buf->pBuffer + offset);
        while (pExtra->eType != OMX_ExtraDataNone && buf->nAllocLen - (offset + pExtra->nSize) > 0)
        {
            switch (pExtra->eType) {
            case OMX_ExtraDataSeekInfo:
                memcpy(seek_pos, pExtra->data, sizeof(int64_t));
            default:
                break;
            }
            offset += (pExtra->nSize + 0x03L) & ~0x03L;
            pExtra = (OMX_OTHER_EXTRADATATYPE*)(buf->pBuffer + offset);
        }
    }
    return 0;
}

static int omx_set_commandline(AVFormatContext* avctx)
{
    int ret = 0;
    OMXComponentContext* s = avctx->priv_data;

    OMX_VENDOR_PARAM_COMMANDLINETYPE* commandline;

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

static OMX_BOOL fill_buffer_done_cb(OMXComponentContext* s, OMX_BUFFERHEADERTYPE* buffer)
{
    if (s->deiniting || s->eos_flag && buffer->nFilledLen == 0)
        return OMX_FALSE;

    AVFormatContext* avctx = s->avctx;
    struct OMX_MXF_Mux* s_mux = avctx->priv_data;

    int64_t seek_pos = -1;
    parse_extradata(buffer, &seek_pos);

    OMX_U32 stream_idx = rev_port_idx(s, buffer->nOutputPortIndex) - s_mux->first_out_port;

    if (seek_pos >= 0) {
        avio_seek(avctx->pb, seek_pos, SEEK_SET);
        avio_write(s_mux->streams[stream_idx], buffer->pBuffer + buffer->nOffset, buffer->nFilledLen);
        buffer->nFlags = 0;
        avio_seek(avctx->pb, 0, SEEK_END);
    }
    else {
        avio_write(s_mux->streams[stream_idx], buffer->pBuffer + buffer->nOffset, buffer->nFilledLen);
    }

    buffer->nFilledLen = 0;
    OMX_FillThisBuffer(s->component, buffer);

    return OMX_TRUE;
}

static int omx_set_pic_param(AVFormatContext* avctx)
{
    int ret = 0;
    struct OMX_MXF_Mux* s_mux = avctx->priv_data;
    OMXComponentContext* s = avctx->priv_data;

    AVCodecParameters* cur_codec_par;
    AVRational avg_frame_rate = {0,0};
    OMX_U32 port_idx;

    for (int i = 0; i < avctx->nb_streams; i++)
        if (avctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            port_idx = s_mux->stream_idx_to_port[i];
            avg_frame_rate = avctx->streams[i]->avg_frame_rate;
            cur_codec_par = avctx->streams[i]->codecpar;
            break;
        }

    if(avg_frame_rate.den==0 || avg_frame_rate.num == 0)
        return 0;//no video stream

    OMX_PARAM_PORTDEFINITIONTYPE port_definition = { 0 };
    INIT_STRUCT(port_definition);
    port_definition.nPortIndex = port_idx;
    OMX_ERRORTYPE err = OMX_GetParameter(s->component, OMX_IndexParamPortDefinition, &port_definition);
    OMX_ERROR_CHECK(ret, avctx)
    port_definition.format.video.xFramerate = avg_frame_rate.num / (double)avg_frame_rate.den * Q16_SHIFT;
    ret = OMX_SetParameter(s->component, OMX_IndexParamPortDefinition, &port_definition);

    OMX_ERROR_CHECK(ret, avctx)

    return 0;
}

static char* remove_file_name_ext(char* myStr) {
    char* retStr;
    char* lastExt;
    if (myStr == NULL) return NULL;
    if ((retStr = malloc(strlen(myStr) + 1)) == NULL) return NULL;
    strcpy(retStr, myStr);
    lastExt = strrchr(retStr, '.');
    if (lastExt != NULL)
        *lastExt = '\0';
    return retStr;
}

static int mxf_init_avio(AVFormatContext* avctx)
{
    AVIOContext* pb;
    int ret;

    struct OMX_MXF_Mux* s_mux = avctx->priv_data;
    OMXComponentContext* s = avctx->priv_data;

    s_mux->streams[0] = avctx->pb;

    if (s_mux->split_track || s_mux->split_channel) {// TODO: use the same file extension as in avctx->pb

        char* file_name = remove_file_name_ext(avctx->url);
        char new_filename[1024];
        int stream_index = 1;

        int stream_i = avctx->nb_streams == 1 && avctx->streams[0]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && s_mux->split_channel ? 0 : 1;

        for (stream_i; stream_i < avctx->nb_streams; stream_i++) {
            if (s_mux->split_track || s_mux->split_channel && avctx->streams[stream_i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                snprintf(new_filename, sizeof(new_filename), "%s%d%s", file_name, stream_i, ".mxf");
                ret = avctx->io_open(avctx, &pb, new_filename, AVIO_FLAG_WRITE, NULL);

                s_mux->streams[stream_index] = pb;
                stream_index++;
            } else if (s_mux->split_channel && avctx->streams[stream_i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                int channel_i = stream_i == 0 ? 1 : 0;
                for (channel_i;  channel_i < avctx->streams[stream_i]->codecpar->channels; channel_i++) {
                    snprintf(new_filename, sizeof(new_filename), "%s%d%d%s", file_name, stream_i, channel_i, ".mxf");
                    ret = avctx->io_open(avctx, &pb, new_filename, AVIO_FLAG_WRITE, NULL);

                    s_mux->streams[stream_index] = pb;
                    stream_index++;
                }
            }
        }

        free(file_name);
    }

    s_mux->first_out_port = rev_port_idx(s, s->nStartPortNumber[OMX_PortDomainOther]);
    return 0;
}

static int mxf_init(AVFormatContext *avctx)
{
    struct OMX_MXF_Mux *s_mux = avctx->priv_data;
    struct OMXComponentContext *s = &s_mux->base;

    s->avctx = avctx;
    s->fill_buffer_done_cb = fill_buffer_done_cb;

    if (avctx->nb_streams > MAX_STREAMS_COUNT)
        return AVERROR_STREAM_NOT_FOUND;

    int ret = omx_cmpnt_init(s);
    if (ret) return ret;

    ret = disable_unused_ports(avctx);
    if (ret) return ret;

    ret = populate_stream_idx_map(avctx);
    if (ret) return ret;

    ret = omx_set_pic_param(avctx);
    if (ret) return ret;

    ret = omx_set_pcm_param(avctx);
    if (ret) return ret;

    ret = omx_set_commandline(avctx);
    if (ret) return ret;

    ret = mxf_init_avio(avctx);
    if (ret) return ret;

    ret = omx_cmpnt_start(s);
    if (ret) return ret;

    return ret;
}

static void mxf_deinit(AVFormatContext *avctx)
{
    struct OMX_MXF_Mux *s_mux = avctx->priv_data;
    struct OMXComponentContext *s = &s_mux->base;

    omx_cmpnt_end(s);
}

static int convert_buffer(AVCodecContext* avctx, OMX_BUFFERHEADERTYPE** out_buf, AVPacket* avpkt, int* buffer_eos_flag)
{
    OMXComponentContext *s = avctx->priv_data;

    int ret = 0;

    OMX_BUFFERHEADERTYPE *buf = *out_buf;

    ret = av_new_packet(avpkt, buf->nFilledLen);
    memcpy(avpkt->data, buf->pBuffer + buf->nOffset, buf->nFilledLen);
    avpkt->size = buf->nFilledLen;

    *buffer_eos_flag = (*out_buf)->nFlags & OMX_BUFFERFLAG_EOS;
    (*out_buf)->nFilledLen = 0;
    OMX_FillThisBuffer(s->component, *out_buf);
    *out_buf = NULL;

    return ret;
}

static int port_to_domain(OMXComponentContext *s, int port_idx)
{
    for (int i=OMX_PortDomainAudio; i <= OMX_PortDomainOther; i++) {
        if (port_idx >= s->nStartPortNumber[i] && port_idx < s->nStartPortNumber[i] + s->nPorts[i])
            return i;
    }

    return -1;
}

static int mxf_write_packet(AVFormatContext *avctx, AVPacket *avpkt)
{
    struct OMX_MXF_Mux *s_mux = avctx->priv_data;
    struct OMXComponentContext *s = &s_mux->base;

    int ret = 0;
    int buffer_eos_flag = 0;

    OMX_BUFFERHEADERTYPE *out_buf = NULL;
    OMX_BUFFERHEADERTYPE *in_buf  = NULL;

    AVStream *st = avctx->streams[avpkt->stream_index];
    MXFStreamContext* sc = st->priv_data;

    int video_pkt = st->codecpar->codec_id == AV_CODEC_ID_H264 ||
                    st->codecpar->codec_id == AV_CODEC_ID_MPEG2VIDEO ||
                    st->codecpar->codec_id == AV_CODEC_ID_DNXHD || 
                    st->codecpar->codec_id == AV_CODEC_ID_DVVIDEO;
    int audio_pkt = st->codecpar->codec_id == AV_CODEC_ID_PCM_S16LE || st->codecpar->codec_id == AV_CODEC_ID_PCM_S24LE;

    av_assert0(video_pkt || audio_pkt);

    int omx_port = s_mux->stream_idx_to_port[avpkt->stream_index];

    if (omx_port < 0) {
        av_log(avctx, AV_LOG_WARNING, "There are more input streams than muxer supports. Stream %i won't be muxed.\n", avpkt->stream_index);
        return 0;
    }

    OMX_BUFFERHEADERTYPE* buf = omx_wait_input_buffer_n(s, rev_port_idx(s, omx_port));

    buf->nFlags = 0;

    av_assert0(buf->nAllocLen >= avpkt->size);

    if (avpkt->pts == AV_NOPTS_VALUE) {
        buf->nFlags |= OMX_BUFFERFLAG_TIMESTAMPINVALID;
    } else {
        const AVRational omx_time_base = {1, 1000000};
        buf->nTimeStamp = to_omx_ticks(av_rescale_q(avpkt->pts, st->time_base, omx_time_base));
    }

    buf->nFilledLen = avpkt->size;
    memcpy(buf->pBuffer + buf->nOffset, avpkt->data, avpkt->size);

    OMX_EmptyThisBuffer(s->component, buf);

    while (out_buf = omx_pick_output_buffer(s)) {
        out_buf->nFilledLen = 0;
        OMX_FillThisBuffer(s->component, out_buf);
    }

    if (s->cur_err == OMX_ErrorUndefined) {
        return -1;//end mux
    }

    return 0;
}

static int mxf_write_end(AVFormatContext* avctx)
{
    OMXComponentContext* s = avctx->priv_data;
    struct OMX_MXF_Mux* s_mux = avctx->priv_data;
    OMX_BUFFERHEADERTYPE* out_buf = NULL;

    for (int i = 0; i < s->port_num; i++)
    {
        if (s->port_out[i] || s->port_disabled[i])
            continue;

        OMX_BUFFERHEADERTYPE* buf = omx_wait_input_buffer_n(s, i);
        buf->nFlags = OMX_BUFFERFLAG_EOS;
        s->eos_flag = OMX_TRUE;
        OMX_EmptyThisBuffer(s->component, buf);
    }

    int expect_eos = s_mux->out_port_enabled;
    while (out_buf = omx_wait_output_buffer(s)) {
        assert(buffer->nFilledLen == 0);

        if (out_buf->nFlags & OMX_BUFFERFLAG_EOS)
            expect_eos--;

        out_buf->nFilledLen = 0;
        OMX_FillThisBuffer(s->component, out_buf);

        if (!expect_eos)
            break;

    }
    return 0;
}

static int mxf_check_bitstream(struct AVFormatContext *s, const AVPacket *pkt)
{
    int ret = 1;
    AVStream *st = s->streams[pkt->stream_index];
    MXFStreamContext* sc = st->priv_data;

    if (st->codecpar->codec_id == AV_CODEC_ID_MPEG2VIDEO ||
        st->codecpar->codec_id == AV_CODEC_ID_DNXHD || st->codecpar->codec_id == AV_CODEC_ID_DVVIDEO) {
        // Nothing to do
    }
    else if (st->codecpar->codec_id == AV_CODEC_ID_H264) {
        if (pkt->size >= 5 && AV_RB32(pkt->data) != 0x0000001 &&
            (AV_RB24(pkt->data) != 0x000001 ||
                (st->codecpar->extradata_size > 0 &&
                    st->codecpar->extradata[0] == 1)))
            av_log(s, AV_LOG_WARNING, "mxf_check_bitstream empty ff_stream_add_bitstream_filter(st, h264_mp4toannexb, NULL)????.\n");
        ret = ff_stream_add_bitstream_filter(st, "h264_mp4toannexb", NULL);
    }
    else if (st->codecpar->codec_id == AV_CODEC_ID_PCM_S16LE || st->codecpar->codec_id == AV_CODEC_ID_PCM_S24LE) {
    } else {
        av_log(s, AV_LOG_ERROR, "Unsupported input AV_CODEC_ID = %d", st->codecpar->codec_id);
        ret = AVERROR_MUXER_NOT_FOUND;
    }

    return ret;
}

#define OFFSET(x) offsetof(struct OMX_MXF_Mux, x)

static const AVOption options[] = {
    { "omx_core" , "OMX Core library name", OFFSET(base.core_libname), AV_OPT_TYPE_STRING, { .str = NULL }, 0, 0, AV_OPT_FLAG_ENCODING_PARAM },
    { "omx_format_name" , "OMX component name", OFFSET(base.component_name), AV_OPT_TYPE_STRING, { .str = NULL }, 0, 0, AV_OPT_FLAG_ENCODING_PARAM },
    { "omx_format_param", "OMX component parameters",  OFFSET(base.component_param), AV_OPT_TYPE_STRING, { .str = NULL }, 0, 0, AV_OPT_FLAG_ENCODING_PARAM },

    { "mxf_audio_edit_rate", "Audio edit rate for timecode", offsetof(OMX_MXF_Mux, audio_edit_rate), AV_OPT_TYPE_RATIONAL, {.dbl=25}, 0, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM },
    { "store_user_comments", "", offsetof(OMX_MXF_Mux, store_user_comments), AV_OPT_TYPE_BOOL, {.i64 = 1}, 0, 1, AV_OPT_FLAG_ENCODING_PARAM},
    { "split_track", "", offsetof(OMX_MXF_Mux, split_track), AV_OPT_TYPE_BOOL, {.i64 = 0}, 0, 1, AV_OPT_FLAG_ENCODING_PARAM},
    { "split_channel", "", offsetof(OMX_MXF_Mux, split_channel), AV_OPT_TYPE_BOOL, {.i64 = 0}, 0, 1, AV_OPT_FLAG_ENCODING_PARAM},
    { NULL },
};


static const AVClass mxf_muxer_test_class = {
    .class_name = "MXF muxer",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVOutputFormat ff_mxf_omx_muxer = {
    .name            = "omx_mxf_mux",
    .long_name       = NULL_IF_CONFIG_SMALL("MXF (Material eXchange Format) D-10 Mapping"),
    .mime_type       = "application/mxf",
    .extensions      = "mxf",
    .priv_data_size  = sizeof(struct OMX_MXF_Mux),
    .audio_codec     = AV_CODEC_ID_PCM_S16LE,
    .video_codec     = AV_CODEC_ID_H264,
    .init            = mxf_init,
    .write_packet    = mxf_write_packet,
    .write_trailer   = mxf_write_end,
    .deinit          = mxf_deinit,
    .check_bitstream = mxf_check_bitstream,
    .flags           = AVFMT_NOTIMESTAMPS,
    .priv_class      = &mxf_muxer_test_class,
};
