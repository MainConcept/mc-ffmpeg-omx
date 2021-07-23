/*
 * OMX TS muxer
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

#include "libavutil/avassert.h"
#include "libavutil/bswap.h"
#include "libavutil/crc.h"
#include "libavutil/dict.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/mathematics.h"
#include "libavutil/opt.h"

#include "libavutil/thread.h"

#include "libavcodec/internal.h"
#include "libavcodec/omx_common.h"

#include "avformat.h"
#include "avio_internal.h"
#include "internal.h"
#include "mpegts.h"

#define PCR_TIME_BASE 27000000

/* write DVB SI sections */

#define DVB_PRIVATE_NETWORK_START 0xff01

/*********************************************/
/* mpegts section writer */

typedef struct MpegTSSection {
    int pid;
    int cc;
    int discontinuity;
    void (*write_packet)(struct MpegTSSection *s, const uint8_t *packet);
    void *opaque;
} MpegTSSection;

typedef struct MpegTSService {
    MpegTSSection pmt; /* MPEG-2 PMT table context */
    int sid;           /* service ID */
    uint8_t name[256];
    uint8_t provider_name[256];
    int pcr_pid;
    int pcr_packet_count;
    int pcr_packet_period;
    AVProgram *program;
} MpegTSService;

// service_type values as defined in ETSI 300 468
enum {
    MPEGTS_SERVICE_TYPE_DIGITAL_TV                   = 0x01,
    MPEGTS_SERVICE_TYPE_DIGITAL_RADIO                = 0x02,
    MPEGTS_SERVICE_TYPE_TELETEXT                     = 0x03,
    MPEGTS_SERVICE_TYPE_ADVANCED_CODEC_DIGITAL_RADIO = 0x0A,
    MPEGTS_SERVICE_TYPE_MPEG2_DIGITAL_HDTV           = 0x11,
    MPEGTS_SERVICE_TYPE_ADVANCED_CODEC_DIGITAL_SDTV  = 0x16,
    MPEGTS_SERVICE_TYPE_ADVANCED_CODEC_DIGITAL_HDTV  = 0x19,
    MPEGTS_SERVICE_TYPE_HEVC_DIGITAL_HDTV            = 0x1F,
};

const size_t MUX_PORT_NUM = 3;

#define MAX_STREAMS_COUNT 16

typedef struct OMX_TS_Mux {
    struct OMXComponentContext base;

    MpegTSSection pat; /* MPEG-2 PAT table */
    MpegTSSection sdt; /* MPEG-2 SDT table context */
    MpegTSService **services;
    int sdt_packet_count;
    int sdt_packet_period;
    int pat_packet_count;
    int pat_packet_period;
    int nb_services;
    int onid;
    int tsid;
    int64_t first_pcr;
    int mux_rate; ///< set to 1 when VBR
    int pes_payload_size;

    int transport_stream_id;
    int original_network_id;
    int service_id;
    int service_type;

    int pmt_start_pid;
    int start_pid;
    int m2ts_mode;

    int reemit_pat_pmt; // backward compatibility

    int pcr_period;
#define MPEGTS_FLAG_REEMIT_PAT_PMT              0x01
#define MPEGTS_FLAG_AAC_LATM                    0x02
#define MPEGTS_FLAG_PAT_PMT_AT_FRAMES           0x04
#define MPEGTS_FLAG_SYSTEM_B                    0x08
#define MPEGTS_FLAG_DISCONT                     0x10
    int flags;
    int copyts;
    int tables_version;
    double pat_period;
    double sdt_period;
    int64_t last_pat_ts;
    int64_t last_sdt_ts;

    int omit_video_pes_length;

    int stream_idx_to_port[MAX_STREAMS_COUNT];
} MpegTSWrite;

/* a PES packet header is generated every DEFAULT_PES_HEADER_FREQ packets */
#define DEFAULT_PES_HEADER_FREQ  16
#define DEFAULT_PES_PAYLOAD_SIZE ((DEFAULT_PES_HEADER_FREQ - 1) * 184 + 170)

/* The section length is 12 bits. The first 2 are set to 0, the remaining
 * 10 bits should not exceed 1021. */
#define SECTION_LENGTH 1020

/*********************************************/
/* mpegts writer */

#define DEFAULT_PROVIDER_NAME   "FFmpeg"
#define DEFAULT_SERVICE_NAME    "Service01"

/* we retransmit the SI info at this rate */
#define SDT_RETRANS_TIME 500
#define PAT_RETRANS_TIME 100
#define PCR_RETRANS_TIME 20

typedef struct MpegTSWriteStream {
    struct MpegTSService *service;
    int pid; /* stream associated pid */
    int cc;
    int discontinuity;
    int payload_size;
    int first_pts_check; ///< first pts check needed
    int prev_payload_key;
    int64_t payload_pts;
    int64_t payload_dts;
    int payload_flags;
    uint8_t *payload;
    AVFormatContext *amux;
    AVRational user_tb;

    /* For Opus */
    int opus_queued_samples;
    int opus_pending_trim_start;
} MpegTSWriteStream;


static int disable_unused_ports(AVFormatContext *avctx)
{
    struct OMX_TS_Mux *s_mux = avctx->priv_data;
    struct OMXComponentContext *s = &s_mux->base;

    int n_streams[AVMEDIA_TYPE_NB] = {0};

    for (int i=0; i<avctx->nb_streams; i++)
        n_streams[avctx->streams[i]->codecpar->codec_type]++;

    static int omx_domain_to_codec_type[] = {AVMEDIA_TYPE_AUDIO, AVMEDIA_TYPE_VIDEO, AVMEDIA_TYPE_DATA, AVMEDIA_TYPE_DATA};

    for (int i=0; i<OMX_PortDomainOther+1; i++) {
        const int n_ports = s->nPorts[i];
        const int codec_type = omx_domain_to_codec_type[i];

        const int start_n = s->nStartPortNumber[i];
        const int end_n   = s->nStartPortNumber[i] + s->nPorts[i];

        for (int j=start_n + n_streams[codec_type]; j<end_n; j++)
            if (!s->port_out[rev_port_idx(s, j)])
                omx_disable_port(s, j);
    }

    return 0;
}

static int populate_stream_idx_map(AVFormatContext *avctx)
{
    struct OMX_TS_Mux *s_mux = avctx->priv_data;
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

static int mpegts_init(AVFormatContext *avctx)
{
    struct OMX_TS_Mux *s_mux = avctx->priv_data;
    struct OMXComponentContext *s = &s_mux->base;

    s->avctx = avctx;

    if (avctx->nb_streams > MAX_STREAMS_COUNT)
        return AVERROR_STREAM_NOT_FOUND;

    int ret = omx_cmpnt_init(s);
    if (ret) return ret;

    ret = disable_unused_ports(avctx);
    if (ret) return ret;

    ret = populate_stream_idx_map(avctx);
    if (ret) return ret;

    ret = omx_set_commandline(avctx);
    if (ret) return ret;
    ret = omx_cmpnt_start(s);
    if (ret) return ret;

    return ret;
}

static void mpegts_deinit(AVFormatContext *avctx)
{
    struct OMX_TS_Mux *s_mux = avctx->priv_data;
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

static int mpegts_write_packet(AVFormatContext *avctx, AVPacket *avpkt)
{
    struct OMX_TS_Mux *s_mux = avctx->priv_data;
    struct OMXComponentContext *s = &s_mux->base;

    int ret = 0;
    int buffer_eos_flag = 0;

    OMX_BUFFERHEADERTYPE *out_buf = NULL;
    OMX_BUFFERHEADERTYPE *in_buf  = NULL;

    AVStream *st = avctx->streams[avpkt->stream_index];

    int video_pkt = st->codecpar->codec_id == AV_CODEC_ID_H264;
    int audio_pkt = st->codecpar->codec_id == AV_CODEC_ID_AC3 || st->codecpar->codec_id == AV_CODEC_ID_EAC3;

    av_assert0(video_pkt || audio_pkt);

    int omx_port = s_mux->stream_idx_to_port[avpkt->stream_index];

    if (omx_port < 0) {
        av_log(avctx, AV_LOG_WARNING, "There are more input streams than muxer supports. Stream %i won't be muxed.\n", avpkt->stream_index);
        return 0;
//        return AVERROR_STREAM_NOT_FOUND;
    }

    OMX_BUFFERHEADERTYPE* buf = omx_wait_input_buffer_n(s, rev_port_idx(s, omx_port));

    buf->nFlags = 0;

    av_assert0(buf->nAllocLen >= avpkt->size);

    if (avpkt->pts == AV_NOPTS_VALUE) {
        buf->nFlags |= OMX_BUFFERFLAG_TIMESTAMPINVALID;
    } else {
        buf->nTimeStamp = to_omx_ticks(avpkt->pts * 1000000 * st->codec->time_base.num / st->codec->time_base.den);
    }
    
    buf->nFilledLen = avpkt->size;
    memcpy(buf->pBuffer + buf->nOffset, avpkt->data, avpkt->size);

    // TODO : Timestamps, eos flag

    OMX_EmptyThisBuffer(s->component, buf);

    while (out_buf = omx_pick_output_buffer(s)) {
        avio_write(avctx->pb, out_buf->pBuffer + out_buf->nOffset, out_buf->nFilledLen);

        out_buf->nFilledLen = 0;
        OMX_FillThisBuffer(s->component, out_buf);
    }

    return 0;
}

static int mpegts_write_end(AVFormatContext* avctx)
{
    OMXComponentContext* s = avctx->priv_data;
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

    while (out_buf = omx_wait_output_buffer(s)) {
        avio_write(avctx->pb, out_buf->pBuffer + out_buf->nOffset, out_buf->nFilledLen);

        out_buf->nFilledLen = 0;
        OMX_FillThisBuffer(s->component, out_buf);

        if (out_buf->nFlags & OMX_BUFFERFLAG_EOS)
            break;
    }

    return 0;
}

static int mpegts_check_bitstream(struct AVFormatContext *s, const AVPacket *pkt)
{
    int ret = 1;
    AVStream *st = s->streams[pkt->stream_index];

    if (st->codecpar->codec_id == AV_CODEC_ID_H264) {
        if (pkt->size >= 5 && AV_RB32(pkt->data) != 0x0000001 &&
            (AV_RB24(pkt->data) != 0x000001 ||
             (st->codecpar->extradata_size > 0 &&
              st->codecpar->extradata[0] == 1)))
            ret = ff_stream_add_bitstream_filter(st, "h264_mp4toannexb", NULL);
    } else if (st->codecpar->codec_id == AV_CODEC_ID_HEVC) {
        if (pkt->size >= 5 && AV_RB32(pkt->data) != 0x0000001 &&
            (AV_RB24(pkt->data) != 0x000001 ||
             (st->codecpar->extradata_size > 0 &&
              st->codecpar->extradata[0] == 1)))
            ret = ff_stream_add_bitstream_filter(st, "hevc_mp4toannexb", NULL);
    } else if (st->codecpar->codec_id == AV_CODEC_ID_AC3 || st->codecpar->codec_id == AV_CODEC_ID_EAC3) {
        int a = st->codecpar->extradata_size;
    } else {
        av_log(s, AV_LOG_ERROR, "Currently supported formats are AVC, HEVC, AC3, EAC3 only.\n");
        ret = AVERROR_MUXER_NOT_FOUND;
    }

    return ret;
}

#define OFFSET(x) offsetof(struct OMX_TS_Mux, x)

static const AVOption options[] = {
    { "omx_core" , "OMX Core library name", OFFSET(base.core_libname), AV_OPT_TYPE_STRING, { .str = NULL }, 0, 0, AV_OPT_FLAG_ENCODING_PARAM },
    { "omx_format_name" , "OMX component name", OFFSET(base.component_name), AV_OPT_TYPE_STRING, { .str = NULL }, 0, 0, AV_OPT_FLAG_ENCODING_PARAM },
    { "omx_format_param", "OMX component parameters",  OFFSET(base.component_param), AV_OPT_TYPE_STRING, { .str = NULL }, 0, 0, AV_OPT_FLAG_ENCODING_PARAM },
    { "mpegts_transport_stream_id", "Set transport_stream_id field.",
      offsetof(MpegTSWrite, transport_stream_id), AV_OPT_TYPE_INT,
      { .i64 = 0x0001 }, 0x0001, 0xffff, AV_OPT_FLAG_ENCODING_PARAM },
    { "mpegts_original_network_id", "Set original_network_id field.",
      offsetof(MpegTSWrite, original_network_id), AV_OPT_TYPE_INT,
      { .i64 = DVB_PRIVATE_NETWORK_START }, 0x0001, 0xffff, AV_OPT_FLAG_ENCODING_PARAM },
    { "mpegts_service_id", "Set service_id field.",
      offsetof(MpegTSWrite, service_id), AV_OPT_TYPE_INT,
      { .i64 = 0x0001 }, 0x0001, 0xffff, AV_OPT_FLAG_ENCODING_PARAM },
    { "mpegts_service_type", "Set service_type field.",
      offsetof(MpegTSWrite, service_type), AV_OPT_TYPE_INT,
      { .i64 = 0x01 }, 0x01, 0xff, AV_OPT_FLAG_ENCODING_PARAM, "mpegts_service_type" },
    { "digital_tv", "Digital Television.",
      0, AV_OPT_TYPE_CONST, { .i64 = MPEGTS_SERVICE_TYPE_DIGITAL_TV }, 0x01, 0xff,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_service_type" },
    { "digital_radio", "Digital Radio.",
      0, AV_OPT_TYPE_CONST, { .i64 = MPEGTS_SERVICE_TYPE_DIGITAL_RADIO }, 0x01, 0xff,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_service_type" },
    { "teletext", "Teletext.",
      0, AV_OPT_TYPE_CONST, { .i64 = MPEGTS_SERVICE_TYPE_TELETEXT }, 0x01, 0xff,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_service_type" },
    { "advanced_codec_digital_radio", "Advanced Codec Digital Radio.",
      0, AV_OPT_TYPE_CONST, { .i64 = MPEGTS_SERVICE_TYPE_ADVANCED_CODEC_DIGITAL_RADIO }, 0x01, 0xff,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_service_type" },
    { "mpeg2_digital_hdtv", "MPEG2 Digital HDTV.",
      0, AV_OPT_TYPE_CONST, { .i64 = MPEGTS_SERVICE_TYPE_MPEG2_DIGITAL_HDTV }, 0x01, 0xff,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_service_type" },
    { "advanced_codec_digital_sdtv", "Advanced Codec Digital SDTV.",
      0, AV_OPT_TYPE_CONST, { .i64 = MPEGTS_SERVICE_TYPE_ADVANCED_CODEC_DIGITAL_SDTV }, 0x01, 0xff,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_service_type" },
    { "advanced_codec_digital_hdtv", "Advanced Codec Digital HDTV.",
      0, AV_OPT_TYPE_CONST, { .i64 = MPEGTS_SERVICE_TYPE_ADVANCED_CODEC_DIGITAL_HDTV }, 0x01, 0xff,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_service_type" },
    { "hevc_digital_hdtv", "HEVC Digital Television Service.",
      0, AV_OPT_TYPE_CONST, { .i64 = MPEGTS_SERVICE_TYPE_HEVC_DIGITAL_HDTV }, 0x01, 0xff,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_service_type" },
    { "mpegts_pmt_start_pid", "Set the first pid of the PMT.",
      offsetof(MpegTSWrite, pmt_start_pid), AV_OPT_TYPE_INT,
      { .i64 = 0x1000 }, 0x0010, 0x1f00, AV_OPT_FLAG_ENCODING_PARAM },
    { "mpegts_start_pid", "Set the first pid.",
      offsetof(MpegTSWrite, start_pid), AV_OPT_TYPE_INT,
      { .i64 = 0x0100 }, 0x0010, 0x0f00, AV_OPT_FLAG_ENCODING_PARAM },
    { "mpegts_m2ts_mode", "Enable m2ts mode.",
      offsetof(MpegTSWrite, m2ts_mode), AV_OPT_TYPE_BOOL,
      { .i64 = -1 }, -1, 1, AV_OPT_FLAG_ENCODING_PARAM },
    { "muxrate", NULL,
      offsetof(MpegTSWrite, mux_rate), AV_OPT_TYPE_INT,
      { .i64 = 1 }, 0, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM },
    { "pes_payload_size", "Minimum PES packet payload in bytes",
      offsetof(MpegTSWrite, pes_payload_size), AV_OPT_TYPE_INT,
      { .i64 = DEFAULT_PES_PAYLOAD_SIZE }, 0, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM },
    { "mpegts_flags", "MPEG-TS muxing flags",
      offsetof(MpegTSWrite, flags), AV_OPT_TYPE_FLAGS, { .i64 = 0 }, 0, INT_MAX,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_flags" },
    { "resend_headers", "Reemit PAT/PMT before writing the next packet",
      0, AV_OPT_TYPE_CONST, { .i64 = MPEGTS_FLAG_REEMIT_PAT_PMT }, 0, INT_MAX,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_flags" },
    { "latm", "Use LATM packetization for AAC",
      0, AV_OPT_TYPE_CONST, { .i64 = MPEGTS_FLAG_AAC_LATM }, 0, INT_MAX,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_flags" },
    { "pat_pmt_at_frames", "Reemit PAT and PMT at each video frame",
      0, AV_OPT_TYPE_CONST, { .i64 = MPEGTS_FLAG_PAT_PMT_AT_FRAMES}, 0, INT_MAX,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_flags" },
    { "system_b", "Conform to System B (DVB) instead of System A (ATSC)",
      0, AV_OPT_TYPE_CONST, { .i64 = MPEGTS_FLAG_SYSTEM_B }, 0, INT_MAX,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_flags" },
    { "initial_discontinuity", "Mark initial packets as discontinuous",
      0, AV_OPT_TYPE_CONST, { .i64 = MPEGTS_FLAG_DISCONT }, 0, INT_MAX,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_flags" },
    // backward compatibility
    { "resend_headers", "Reemit PAT/PMT before writing the next packet",
      offsetof(MpegTSWrite, reemit_pat_pmt), AV_OPT_TYPE_INT,
      { .i64 = 0 }, 0, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM },
    { "mpegts_copyts", "don't offset dts/pts",
      offsetof(MpegTSWrite, copyts), AV_OPT_TYPE_BOOL,
      { .i64 = -1 }, -1, 1, AV_OPT_FLAG_ENCODING_PARAM },
    { "tables_version", "set PAT, PMT and SDT version",
      offsetof(MpegTSWrite, tables_version), AV_OPT_TYPE_INT,
      { .i64 = 0 }, 0, 31, AV_OPT_FLAG_ENCODING_PARAM },
    { "omit_video_pes_length", "Omit the PES packet length for video packets",
      offsetof(MpegTSWrite, omit_video_pes_length), AV_OPT_TYPE_BOOL,
      { .i64 = 1 }, 0, 1, AV_OPT_FLAG_ENCODING_PARAM },
    { "pcr_period", "PCR retransmission time in milliseconds",
      offsetof(MpegTSWrite, pcr_period), AV_OPT_TYPE_INT,
      { .i64 = PCR_RETRANS_TIME }, 0, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM },
    { "pat_period", "PAT/PMT retransmission time limit in seconds",
      offsetof(MpegTSWrite, pat_period), AV_OPT_TYPE_DOUBLE,
      { .dbl = INT_MAX }, 0, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM },
    { "sdt_period", "SDT retransmission time limit in seconds",
      offsetof(MpegTSWrite, sdt_period), AV_OPT_TYPE_DOUBLE,
      { .dbl = INT_MAX }, 0, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM },
    { NULL },
};

static const AVClass mpegts_muxer_test_class = {
    .class_name = "MPEGTS muxer",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVOutputFormat ff_mpegts_omx_muxer = {
    .name           = "omx_ts_mux",
    .long_name      = NULL_IF_CONFIG_SMALL("MPEG-TS (MPEG-2 Transport Stream)"),
    .mime_type      = "video/MP2T",
    .extensions     = "ts,m2t,m2ts,mts",
    .priv_data_size = sizeof(struct OMX_TS_Mux),
    .audio_codec    = AV_CODEC_ID_MP2,
    .video_codec    = AV_CODEC_ID_MPEG2VIDEO,
    .init           = mpegts_init,
    .write_packet   = mpegts_write_packet,
    .write_trailer  = mpegts_write_end,
    .deinit         = mpegts_deinit,
    .check_bitstream = mpegts_check_bitstream,
    .flags          = AVFMT_ALLOW_FLUSH | AVFMT_VARIABLE_FPS | AVFMT_NODIMENSIONS,
    .priv_class     = &mpegts_muxer_test_class,
};
