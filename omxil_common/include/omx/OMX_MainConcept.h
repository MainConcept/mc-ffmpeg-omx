/*
 * Copyright (C) 2018 Mainconcept
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
 *
 */

#ifndef OMXIL_COMMON_INCLUDE_OMX_OMX_MAINCONCEPT_H
#define OMXIL_COMMON_INCLUDE_OMX_OMX_MAINCONCEPT_H

#pragma once

#include <OMX_Index.h>
#include <OMX_Video.h>
#include <OMX_Image.h>
#include <OMX_Audio.h>
#include <OMX_Other.h>

/**
 * Q16 Multiplier constant.
 */
#define Q16_SHIFT (0x10000)

/***************************************************************************************************
**********                      COMPONENT ROLE DEFINITIONS                                **********
***************************************************************************************************/
#define OMX_ROLE_CONTAINER_DEMUXER_MP4              "container_demuxer.mp4"
#define OMX_ROLE_CONTAINER_DEMUXER_QUICKTIME        "container_demuxer.qt"

#define VENDOR_AUDIO_INDEX(num)     VENDOR_INDEX(num)
#define VENDOR_VIDEO_INDEX(num)     VENDOR_INDEX(num) + 0x100000
#define VENDOR_IMAGE_INDEX(num)     VENDOR_INDEX(num) + 0x200000
#define VENDOR_OTHER_INDEX(num)     VENDOR_INDEX(num) + 0x300000
#define VENDOR_CONFIG_INDEX(num)    VENDOR_INDEX(num) + 0x500000

/* Helper macros */
#ifdef __cplusplus
#define VENDOR_COMMAND(num)         static_cast<OMX_COMMANDTYPE>(OMX_CommandVendorStartUnused + num)
#define VENDOR_INDEX(num)           static_cast<OMX_INDEXTYPE>(OMX_IndexVendorStartUnused + num)
#define VENDOR_EVENT(num)           static_cast<OMX_EVENTTYPE>(OMX_EventVendorStartUnused + num)
#define VENDOR_PARAM_INDEX(num)     OMX_INDEXTYPE(VENDOR_INDEX(num) + 0x400000)

#define VENDOR_COLOR_FORMAT(num)    OMX_COLOR_FORMATTYPE(OMX_COLOR_FormatVendorStartUnused + num)

#define VENDOR_CODING(domain, num)  OMX_##domain##_CODINGTYPE(OMX_##domain##_CodingVendorStartUnused + num)
#define VENDOR_OTHER_FORMAT(num)    OMX_OTHER_FORMATTYPE(OMX_OTHER_FormatVendorStartUnused + num)
#define VENDOR_EXTRADATATYPE(num)   OMX_EXTRADATATYPE(OMX_ExtraDataVendorStartUnused + num)
#else // __cplusplus
#define VENDOR_COMMAND(num)         (OMX_COMMANDTYPE)(OMX_CommandVendorStartUnused + num)
#define VENDOR_INDEX(num)           (OMX_INDEXTYPE)(OMX_IndexVendorStartUnused + num)
#define VENDOR_EVENT(num)           (OMX_EVENTTYPE)(OMX_EventVendorStartUnused + num)
#define VENDOR_PARAM_INDEX(num)     (OMX_INDEXTYPE)(VENDOR_INDEX(num) + 0x400000)

#define VENDOR_COLOR_FORMAT(num)    (OMX_COLOR_FORMATTYPE)(OMX_COLOR_FormatVendorStartUnused + num)

#define VENDOR_CODING(domain, num)  (OMX_##domain##_CODINGTYPE)(OMX_##domain##_CodingVendorStartUnused + num)
#define VENDOR_OTHER_FORMAT(num)    (OMX_OTHER_FORMATTYPE)(OMX_OTHER_FormatVendorStartUnused + num)
#define VENDOR_EXTRADATATYPE(num)   (OMX_EXTRADATATYPE)(OMX_ExtraDataVendorStartUnused + num)
#endif // __cplusplus

typedef struct OMX_PICTURE_COMMON_PARAM {
    OMX_U32 nSize;                 /**< size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;      /**< OMX specification version information */
    //OMX_U32         nPortIndex;

    OMX_U32 nWidth;
    OMX_U32 nHeight;
    OMX_S8  sFourcc[OMX_FOURCC_SIZE];
} OMX_PICTURE_COMMON_PARAM;

typedef struct OMX_VIDEO_PARAM_PARSET {
    OMX_U32 nSize;                 /**< size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;      /**< OMX specification version information */

    OMX_U32 nParSetMaxSize;
    OMX_U32 nParSetSizeUsed;
    OMX_S8  nParSet[1];
} OMX_VIDEO_PARAM_PARSET;

// TODO : Remove all data types except of OMX_AUDIO_COMMON_PARAM
typedef enum OMX_AUDIO_DDPBITSTREAMID {
    OMX_AUDIO_DDPBitStreamIdAC3 = 8,
    OMX_AUDIO_DDPBitStreamIdEAC3 = 16,
    OMX_AUDIO_DDPBitStreamIdKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
    OMX_AUDIO_DDPBitStreamIdVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
    OMX_AUDIO_DDPBitStreamIdMax = 0x7FFFFFFF
} OMX_AUDIO_DDPBITSTREAMID;

typedef enum OMX_AUDIO_DDPBITSTREAMMODE {
    OMX_AUDIO_DDPBitStreamModeCM = 0,   /**< DDP any main audio service: complete main (CM) */
    OMX_AUDIO_DDPBitStreamModeME,       /**< DDP any main audio service: music and effects (ME) */
    OMX_AUDIO_DDPBitStreamModeVI,       /**< DDP any associated service: visually impaired (VI) */
    OMX_AUDIO_DDPBitStreamModeHI,       /**< DDP any associated service: hearing impaired (HI)  */
    OMX_AUDIO_DDPBitStreamModeD,        /**< DDP any associated service: dialogue (D)           */
    OMX_AUDIO_DDPBitStreamModeC,        /**< DDP any associated service: commentary (C)         */
    OMX_AUDIO_DDPBitStreamModeE,        /**< DDP any associated service: emergency (E)          */
    OMX_AUDIO_DDPBitStreamModeVO,       /**< DDP associated service: voice over (VO)            */
    OMX_AUDIO_DDPBitStreamModeK,        /**< DDP main audio service: karaoke                    */
    OMX_AUDIO_DDPBitStreamModeKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
    OMX_AUDIO_DDPBitStreamModeVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
    OMX_AUDIO_DDPBitStreamModeMax = 0x7FFFFFFF
} OMX_AUDIO_DDPBITSTREAMMODE;

typedef enum OMX_AUDIO_DDPDOLBYSURROUNDMODE {
    OMX_AUDIO_DDPDolbySurroundModeNotIndicated = 0,               /**< Not indicated */
    OMX_AUDIO_DDPDolbySurroundModeNotDolbySurround,               /**< Not Dolby Surround */
    OMX_AUDIO_DDPDolbySurroundModeDolbySurroundEncoded,           /**< Dolby Surround encoded */
    OMX_AUDIO_DDPDolbySurroundModeReserverd,                      /**< Reserved */
    OMX_AUDIO_DDPDolbySurroundModeKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
    OMX_AUDIO_DDPDolbySurroundModeVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
    OMX_AUDIO_DDPDolbySurroundModeMax = 0x7FFFFFFF
} OMX_AUDIO_DDPDOLBYSURROUNDMODE;

/** DDP params */
typedef struct OMX_AUDIO_PARAM_DDPTYPE {
    OMX_U32 nSize;                 /**< size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;      /**< OMX specification version information */
    OMX_U32 nPortIndex;            /**< port that this structure applies to */
    OMX_U16 nChannels;             /**< Number of channels */
    OMX_U32 nBitRate;              /**< Bit rate of the input data.  Use 0 for variable
                                   rate or unknown bit rates */
    OMX_U32 nSampleRate;           /**< Sampling rate of the source data. Use 0 for
                                   variable or unknown sampling rate. */
    OMX_AUDIO_DDPBITSTREAMID eBitStreamId;
    OMX_AUDIO_DDPBITSTREAMMODE eBitStreamMode;
    OMX_AUDIO_DDPDOLBYSURROUNDMODE eDolbySurroundMode;
    OMX_AUDIO_CHANNELTYPE eChannelMapping[OMX_AUDIO_MAXCHANNELS]; /**< Slot i contains channel defined by eChannelMapping[i] */
} OMX_AUDIO_PARAM_DDPTYPE;

typedef enum WORD_SIZE_T {
    OMX_DDP_OPT_SI_PCM_SAMPLES      = 0x00000000, //The audio samples being input to the encoder are 16-bit
    OMX_DDP_OPT_SI_SWAP_PCM_SAMPLES = 0x00000001, //The audio samples being input to the encoder are 16-bit
    OMX_DDP_OPT_FP_PCM_SAMPLES      = 0x00000002, //The audio samples being input to the encoder are 32 bit
    OMX_DDP_OPT_DFP_PCM_SAMPLES     = 0x00000004, //The audio samples being input to the encoder are 64 bit
    OMX_DDP_OPT_24B_PCM_SAMPLES     = 0x00000008, //The audio samples being input to the encoder are 24 bit
    OMX_DDP_OPT_32B_PCM_SAMPLES     = 0x00000010  //The audio samples being input to the encoder are 32 bit
} WORD_SIZE_T;

typedef struct OMX_DEC_PARSER_SETTINGS//OMX_IndexDecSettings
{
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;

    OMX_U8 output_word_size;                   ///< @brief Format of the PCM data output by the decoder. One of DDP_OPT_* values.
    OMX_U8 match_input_mode;                   ///< @brief Specifies a custom output mode
    OMX_S8 output_mode;                        ///< @brief Output mode // find enum ddp_audio_mode_t
    OMX_U8 output_lfe;                         ///< @brief Output LFE flag
    OMX_U8 output_num_channels;                ///< @brief Number of output channels
    OMX_S8 compression_mode;                   ///< @brief Output compression mode
    OMX_U8 stereo_output_mode;                 ///< @brief Output mode for stereo (2/0) and dual (1+1) bitstreams
    OMX_U8 dual_mono_mode;                     ///< @brief Output reproduction mode for dual (1+1) and mono (1/0) bitstreams
    OMX_U32 dynamic_range_scale_low;            ///< @brief Integer number for low-level signals
    OMX_U32 dynamic_range_scale_hi;             ///< @brief Integer number for high-level signals
    OMX_U32 pcm_scale_factor;                   ///< @brief Integer number to scale the final output data prior
    OMX_U8 error_conceal_flag;                 ///< @brief Error concealment flag
    OMX_S8  error_conceal_block_repeats;        ///< @brief Maximum repeat count of the error concealment blocks. Use @ref MC_DECDDPP_CONCEAL_REPEAT or @ref MC_DECDDPP_CONCEAL_MUTE for decoding mode (@ref transcoding_flag = 0)
    OMX_U8 use_channel_table;                  ///< @brief Channel routing mode
    OMX_U8 channel_table[8];                   ///< @brief Channel routing table
    OMX_U8 use_downmix_table;                  ///< @brief Indicates whether the @ref downmix_table field should be used
    double downmix_table[8][8];                ///< @brief Pointer to a custom downmix table
    OMX_U8 evo_hash_flag;                      ///< @brief Evolution decoder mode (0 - parse Evolution data, but no calculation for PCM protection; 1 (default) - parse Evolution data, and calculate PCM protection)

    OMX_U8 transcoding_flag;                   ///< @brief Enables (1) / disables (0) transcoding mode. By default transcoding mode is disabled */

    OMX_U8 decoder_reserved[91];              ///< @brief Decoder reserved data
} OMX_DEC_PARSER_SETTINGS;

typedef struct OMX_AUDIO_COMMON_PARAM {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;

    OMX_U16 nChannels;
    OMX_U32 nSampleRate;
    OMX_U32 nBitRate;

    OMX_U32 nBytesPerSec;
    OMX_U16 nBitsPerSample;
    OMX_U16 nBlockAlign;
    OMX_AUDIO_CHANNELTYPE eChannelMapping[OMX_AUDIO_MAXCHANNELS]; /**< Slot i contains channel defined by eChannelMap[i] */
} OMX_AUDIO_COMMON_PARAM;

// TODO : Use OMX_PARAM_U32TYPE?
typedef struct OMX_OUTPUT_WORD_SIZE {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;

    WORD_SIZE_T     word_size;
} OMX_OUTPUT_WORD_SIZE;

typedef struct OMX_LICENSE_FILE_PATH {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;

    OMX_STRING      pLicPath;
} OMX_LICENSE_FILE_PATH;

typedef struct OMX_PARAM_SEQUENCE {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;

    OMX_STRING      pParamSequence;
} OMX_PARAM_SEQUENCE;



// TODO : Move indices below to the appropriate place and change prefix to MC_ and use VENDOR_INDEX macro
#define OMX_IndexParamAudioWordSizeDdp (OMX_INDEXTYPE)(0x7F000001)   /**< reference: OMX_OUTPUT_WORD_SIZE */
#define OMX_LicFilePath                (OMX_INDEXTYPE)(0x7F000002)   /**< reference: OMX_LICENSE_FILE_PATH */
#define OMX_IndexParamAudioOutputInfo  (OMX_INDEXTYPE)(0x7F000003)
#define OMX_IndexDecSettings           (OMX_INDEXTYPE)(0x7f000004)   /**< reference: OMX_DEC_PARSER_SETTINGS */
#define OMX_IndexParamSequence         (OMX_INDEXTYPE)(0x7F000005)   /**< reference: OMX_PARAM_SEQUENCE */
#define OMX_ChannelCount               (OMX_INDEXTYPE)(0x7F000006)
#define OMX_IndexParamAudioBitrate     (OMX_INDEXTYPE)(0x7F000007)
#define OMX_IndexParamAudioDdp         (OMX_INDEXTYPE)(0x7f00007B)   /**< reference: OMX_AUDIO_PARAM_DDPTYPE */

// PARAM for hevc_encoder
#define OMX_IndexParamPictureCommon    (OMX_INDEXTYPE)(0x7f000100)   /**< reference: OMX_PICTURE_COMMON_PARAM */
#define OMX_IndexParamVideoParSet      (OMX_INDEXTYPE)(0x7f000101)   /**< reference: OMX_VIDEO_PARAM_PARSET */


/***************************************************************************************************
**********                      FORMATS SECTION                                           **********
***************************************************************************************************/

/* Video formats */
#define OMX_VIDEO_CodingVC3         VENDOR_CODING(VIDEO, 0x01)
#define OMX_VIDEO_CodingDV          VENDOR_CODING(VIDEO, 0x02)
#define OMX_VIDEO_CodingMJ2K        VENDOR_CODING(VIDEO, 0x03)
#define OMX_VIDEO_CodingHEVC        VENDOR_CODING(VIDEO, 0x04)

/* Audio formats */
#define OMX_AUDIO_CodingDDP         VENDOR_CODING(AUDIO, 0x01)
#define OMX_AUDIO_CodingMPEG1A      VENDOR_CODING(AUDIO, 0x02)
#define OMX_AUDIO_CodingMPEG2A      VENDOR_CODING(AUDIO, 0x03)

/* Stream formats */
#define OMX_OTHER_FormatMPEG2PS     VENDOR_OTHER_FORMAT(0x01)
#define OMX_OTHER_FormatMPEG2TS     VENDOR_OTHER_FORMAT(0x02)
#define OMX_OTHER_FormatMP4         VENDOR_OTHER_FORMAT(0x03)

/* Video format settings indices */
#define OMX_IndexParamVideoVC3      VENDOR_VIDEO_INDEX(0x01)
#define OMX_IndexParamVideoDV       VENDOR_VIDEO_INDEX(0x02)
#define OMX_IndexParamVideoMJ2K     VENDOR_VIDEO_INDEX(0x03)

/* Audio format settings indices */
#define OMX_IndexParamAudioMPEGA    OMX_INDEXTYPE(VENDOR_AUDIO_INDEX(0x01))

/***************************************************************************************************
**********                      COLOR SPACES SECTION                                      **********
***************************************************************************************************/
#define OMX_COLOR_FormatYVU420PackedPlanar      VENDOR_COLOR_FORMAT(1)      // YV12

/* Video format structures */
typedef struct OMX_VIDEO_PARAM_VC3TYPE
{
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    // TODO: Fill
} OMX_VIDEO_PARAM_VC3TYPE;

typedef struct OMX_VIDEO_PARAM_DVTYPE
{
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    // TODO: Fill
} OMX_VIDEO_PARAM_DVTYPE;

typedef struct OMX_VIDEO_PARAM_MJ2KTYPE
{
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    // TODO: Fill
} OMX_VIDEO_PARAM_MJ2KTYPE;

typedef struct OMX_AUDIO_PARAM_MPEGAUDIOTYPE
{
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    // TODO: Fill
} OMX_AUDIO_PARAM_MPEGAUDIOTYPE;

typedef OMX_ERRORTYPE (*MC_SINK_INITHANDLERTYPE)(OMX_HANDLETYPE hSink);
typedef OMX_U32 (*MC_SINK_DATAHANDLERTYPE)(OMX_HANDLETYPE hSink, OMX_U8* pData, OMX_U32 nSize, OMX_U32 nAllocLen, OMX_U32 flags, OMX_TICKS nTimestamp);
typedef OMX_ERRORTYPE (*MC_SINK_EOSHANDLERTYPE)(OMX_HANDLETYPE hSink);
typedef OMX_ERRORTYPE (*MC_SINK_STOPTYPE)(OMX_HANDLETYPE hSink);
typedef OMX_ERRORTYPE (*MC_SINK_STARTTYPE)(OMX_HANDLETYPE hSink);
typedef OMX_ERRORTYPE (*MC_SINK_PAUSETYPE)(OMX_HANDLETYPE hSink);
typedef OMX_ERRORTYPE (*MC_SINK_UPSTREAMINFOTYPE)(OMX_HANDLETYPE hSink, OMX_PARAM_PORTDEFINITIONTYPE* port_def);
typedef OMX_ERRORTYPE (*MC_SINK_DEINITTYPE)(OMX_HANDLETYPE hSink);

typedef struct MC_SINKENGINETYPE
{
    OMX_HANDLETYPE      hSink;

    MC_SINK_INITHANDLERTYPE     SinkOnInit;
    MC_SINK_DATAHANDLERTYPE     SinkOnData;
    MC_SINK_EOSHANDLERTYPE      SinkOnEOS;
    MC_SINK_STOPTYPE            SinkOnStop;
    MC_SINK_STARTTYPE           SinkOnStart;
    MC_SINK_PAUSETYPE           SinkOnPause;
    MC_SINK_UPSTREAMINFOTYPE    SinkOnUpstreamInfo;
    MC_SINK_DEINITTYPE          SinkOnDeinit;
} MC_SINKENGINETYPE;

typedef struct MC_PARAM_SINKENGINETYPE
{
    OMX_U32             nSize;
    OMX_VERSIONTYPE     nVersion;
    struct MC_SINKENGINETYPE   sSinkEngine;
} MC_PARAM_SINKENGINETYPE;

struct MC_PARAM_COMPONENTNAMETYPE
{
    OMX_U32             nSize;
    OMX_VERSIONTYPE     nVersion;
    OMX_STRING          pComponentName;
};

typedef struct MC_PARAM_DURATIONTYPE
{
    OMX_U32             nSize;
    OMX_VERSIONTYPE     nVersion;
    OMX_U32             nPortIndex;
    OMX_TICKS           nDuration;
} MC_PARAM_DURATIONTYPE;

typedef struct DIVX_PARAM_DASHDIVXDRMPROVISIONINGBLOB
{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;

    OMX_S8 ProvisioningBlobBuff[1024];
} DIVX_PARAM_DASHDIVXDRMPROVISIONINGBLOB;

typedef struct DIVX_PARAM_DASHDIVXPROTECTIONINFO
{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_S8 sUpdate[1024];
    OMX_S8 sLicense[1024];
    OMX_S8 sSession[1024];
    OMX_S8 sConfig[1024];
} DIVX_PARAM_DASHDIVXPROTECTIONINFO;

typedef enum DIVX_MESHTYPE {
    DIVX_MeshType_None = 0,
    DIVX_MeshType_Mesh,
    DIVX_MeshType_SphereTile, // Sphere tile
    DIVX_MeshType_CompleteSphere, // default mesh type if no metadata is available
    DIVX_MeshType_Plane // TODO : Unsupported yet. Not sure if it's really necessary.
} DIVX_MESHTYPE;

typedef struct DIVX_PARAM_MESH_DATA
{
    OMX_U32 nNumVertices;
    const double * pVertices;
    OMX_U32 nNumIndices;
    const OMX_U32 * pIndices;
} DIVX_PARAM_MESH_DATA;

typedef struct DIVX_PARAM_SPHERE_DATA
{
    double center[4]; // quaternion for rotation of vector (0, 1, 0) (looking up in OpenGL coordinate system) to the center of tile.
    double h_angle;
    double v_angle;
} DIVX_PARAM_SPHERE_DATA;

typedef struct DIVX_PARAM_PLANE_DATA
{
    double center[4]; // quaternion
    double width;
    double height;
} DIVX_PARAM_PLANE_DATA;

typedef struct DIVX_PARAM_METADATA_360
{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;

    union {
        DIVX_PARAM_MESH_DATA mesh;
        DIVX_PARAM_SPHERE_DATA sphere;
        DIVX_PARAM_PLANE_DATA plane;
    };

    DIVX_MESHTYPE eMeshType;
    OMX_U32 nViewId;
} DIVX_PARAM_METADATA_360;

typedef struct DIVX_PARAM_DASHDIVXPROTECTIONLICPARAMS
{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_S8 EphermalGetLicenseParams[1024];
    OMX_S8 PersistentGetLicenseParams[1024];
} DIVX_PARAM_DASHDIVXPROTECTIONLICPARAMS;

typedef struct DIVX_PARAM_SMOKE_AND_MIRRORS {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U8 bEnable;
} DIVX_PARAM_SMOKE_AND_MIRRORS;

typedef enum DIVX_NALCODECCONFIGTYPE {
    DIVX_NALCodecConfigTypeNone,
    DIVX_NALCodecConfigTypeCR,      // H.264/H.265 Configuration Record
    DIVX_NALCodecConfigTypeNALU,    // NAL Unit deconfig format (SPS + PPS)
    DIVX_NALCodecConfigTypeAnnexB   // Annex B (start codes) deconfig format (SPS + PPS)
} DIVX_NALCODECCONFIGTYPE;

typedef struct DIVX_PARAM_MP4STREAMEMISSIONMODE
{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;

    enum DIVX_NALCODECCONFIGTYPE eMarkedConfigType;
    OMX_BOOL bSendInBandConfig;
} DIVX_PARAM_MP4STREAMEMISSIONMODE;

typedef struct DIVX_CONFIG_SEEKRANGE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;

    OMX_BOOL bLive;
    OMX_TICKS nDuration; //Relative to zero value of available data to seek, stream duration for VOD, TimeShiftBuffer for LIVE, MICROSECONDS
    OMX_TICKS nStart; //MICROSECONDS
    OMX_TICKS nEnd;   //MICROSECONDS
}DIVX_CONFIG_SEEKRANGE;

typedef struct DIVX_CONFIG_AST {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;

    OMX_TICKS nPosixAST; //Posix time representation for Availability Start Time

}DIVX_CONFIG_AST;

struct DIVX_PARAM_GSTPIPELINE
{
    OMX_U32             nSize;
    OMX_VERSIONTYPE     nVersion;
    OMX_U8              pGstPipeline[1];
};

typedef struct DIVX_CONFIG_QUALITYCOUNT{
    OMX_U32             nSize;
    OMX_VERSIONTYPE     nVersion;
    OMX_U32             nPortIndex;

    OMX_U32             nCount;         /* [OUT]  Quality level count */
}DIVX_CONFIG_QUALITYCOUNT;

typedef struct DIVX_CONFIG_QUALITYDETAILS {
    OMX_U32             nSize;
    OMX_VERSIONTYPE     nVersion;
    OMX_U32             nPortIndex;

    OMX_S32             nQualityIndex;  /* [IN/OUT]  Should be set to valid quality index in range
                                                 obtained via @DIVX_CONFIG_QUALITYCOUNT config.
                                                 Value set to -1 used to obtain current quality. */
    OMX_U32             nBandwidth;     /* [IN/OUT] Bandwidth of queried quality */
    OMX_S32             nMinQuality;
    OMX_S32             nMaxQuality;
}DIVX_CONFIG_QUALITYDETAILS;

typedef struct DIVX_CONFIG_SYNC_RENDER_TIME
{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;

    OMX_TICKS nCurrentTime; // time from the sync renderer in microseconds (UTC)
} DIVX_CONFIG_SYNC_RENDER_TIME;

typedef struct tm (*DIVX_GET_TIME_UTC)(OMX_HANDLETYPE);

typedef struct DIVX_PARAM_UTC_CLOCK
{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;

    OMX_HANDLETYPE hClock; // application supplied pointer to the user data (e.g. clock instance or clock private data)
    DIVX_GET_TIME_UTC ClockGetTimeUTC; // application supplied method to obtain current UTC time
} DIVX_PARAM_UTC_CLOCK;

/***************************************************************************************************
**********                         CONFIGURATION SETTINGS                                 **********
***************************************************************************************************/

/* General settings */
#define MC_IndexParamDuration                   VENDOR_PARAM_INDEX(0x00001)

/* MXF demuxer/muxer */
#define MC_IndexParamMXFParseExternalStreams    VENDOR_PARAM_INDEX(0x01001)
#define MC_IndexParamMXFSplitAudioStreams       VENDOR_PARAM_INDEX(0x01002)

/* Universal Sink */
#define MC_IndexParamUSEngine                   VENDOR_PARAM_INDEX(0x03)
#define MC_IndexParamUSComponentName            VENDOR_PARAM_INDEX(0x04)

/* Network source parameters (@ref DIVX_PARAM_DASHDIVXPROTECTIONINFO) */
#define DIVX_IndexParamProtectionInfo           VENDOR_PARAM_INDEX(0x2000)

/* DivX DRM License Parameters (@ref DIVX_PARAM_DASHDIVXPROTECTIONLICPARAMS) */
#define DIVX_IndexParamProtectionLicParams      VENDOR_PARAM_INDEX(0x2001)

#define DIVX_IndexParamSmokeAndMirrors      VENDOR_PARAM_INDEX(0x2002)

/* DivX DRM License Parameters (@ref DIVX_PARAM_DASHDIVXDRMPROVISIONINGBLOB) */
#define DIVX_IndexParamProtectionProvisioningBlob      VENDOR_PARAM_INDEX(0x2003)

/* DivX DASH seeking capabilities (@ref DIVX_CONFIG_SEEKRANGE); component-specific config */
#define DIVX_IndexConfigSeekRange VENDOR_PARAM_INDEX(0x2004)

/* DivX DASH Availability start time attibute value(@ref DIVX_CONFIG_AST); component-specific config */
#define DIVX_IndexConfigAvailabilityStartTime VENDOR_PARAM_INDEX(0x2005)

/* DivX Taco component gst pipeline string. (@ref DIVX_PARAM_GSTPIPELINE) component-specific config*/
#define DIVX_IndexParamGstPipeline VENDOR_PARAM_INDEX(0x2006)

/* DivX DASH stream quality level count (@ref DIVX_CONFIG_QUALITYCOUNT); component-specific config*/
#define DIVX_IndexConfigQualityLevelCount VENDOR_PARAM_INDEX(0x2007)

/* DivX DASH stream particular quality level information (@ref DIVX_CONFIG_QUALITYDETAILS); component-specific config*/
#define DIVX_IndexConfigQualityLevelDetails VENDOR_PARAM_INDEX(0x2008)

#define DIVX_IndexParamMP4StreamEmissionMode    VENDOR_PARAM_INDEX(0x2100)

#define DIVX_IndexParamMetadata360 VENDOR_PARAM_INDEX(0x2009)

#define DIVX_IndexConfigQualityLimit VENDOR_PARAM_INDEX(0x2010)

/* current time from the sync renderer in micro seconds (UTC) (@ref DIVX_CONFIG_SYNC_RENDER_TIME) */
#define DIVX_IndexConfigSyncRenderTime VENDOR_PARAM_INDEX(0x2011)

/* custom UTC clock source (@ref DIVX_PARAM_UTC_CLOCK */
#define DIVX_IndexParamUTCClock VENDOR_PARAM_INDEX(0x2012)

/***************************************************************************************************
**********                            BUFFER FLAGS                                        **********
***************************************************************************************************/

#define OMX_BUFFERFLAG_DISCONTINUITY        0x10000000
#define OMX_BUFFERFLAG_NEWSEGMENT           0x20000000

/***************************************************************************************************
**********                            CUSTOM COMMANDS                                     **********
***************************************************************************************************/
// This one tells the demuxer that all the necessary ports were disconnected and
// that it can proceed with removing them all and detecting things all anew.
#define MC_CommandInit                          VENDOR_COMMAND(0x10002)

/***************************************************************************************************
**********                            CUSTOM ERROR CODES                                  **********
***************************************************************************************************/

/* (DivX) Specified encryption method is not supported */
enum { OMX_ErrorDivXNotSupportedEncryptMethod = (OMX_S32)0x90000001 };

/* (DivX) Operation requires authorization */
enum { OMX_ErrorDivXRequestedAuthorization = (OMX_S32)0x90000002 };

/* (DivX) DNS resolving failed */
enum { OMX_ErrorDivXDNSNameLookupFailed = (OMX_S32)0x90000003 };

/* *(DivX) Connection could not be established */
enum { OMX_ErrorDivXConnectFailed = (OMX_S32)0x90000004 };

/* (DivX) Response was not received successfully */
enum { OMX_ErrorDivXReceiveFailed = (OMX_S32)0x90000005 };

/* (DivX) Send operation failed */
enum { OMX_ErrorDivXSendFailed = (OMX_S32)0x90000006 };

/* (DivX) Data is not available */
enum { OMX_ErrorDivXNoData = (OMX_S32)0x90000007 };

/* (DivX) Operation requires active connection */
enum { OMX_ErrorDivXNotConnected = (OMX_S32)0x90000008 };

/* (DivX) Parameter is not compatible with current mode of engine */
enum { OMX_ErrorDivXServerDoesNotSupport = (OMX_S32)0x90000009 };

/* (DivX)  Don't have permission to complete operation */
enum { OMX_ErrorDivXRestricted = (OMX_S32)0x9000000A };

/* (DivX) Proxy server requires authorization */
enum { OMX_ErrorDivXRequestedProxyAuthorization = (OMX_S32)0x9000000B };

/* (DivX) Bind socket failed */
enum { OMX_ErrorDivXBindFailed = (OMX_S32)0x9000000C };

/* (DivX) Received redirect request */
enum { OMX_ErrorDivXRequestRedirect = (OMX_S32)0x9000000D };

/* (DivX) Protocol is not supported */
enum { OMX_ErrorDivXUnsupportedProtocol = (OMX_S32)0x9000000E };

/* (DivX) Initial request succeeded (i.e. playlist loaded) }; but actual stream request failed */
enum { OMX_ErrorDivXSubRequestFailed = (OMX_S32)0x9000000F };

/* (DivX) Buffer is too small */
enum { OMX_ErrorDivXBufferTooSmall = (OMX_S32)0x90000010 };

/* (DivX) Metadata of 360 stream is invalid */
enum { OMX_ErrorDivXMetadata360Error = (OMX_S32)0x90000011 };

enum OMX_EVENTTYPE_EXT {
    // nData1 is buffer fullness in percent, nData2 is port index, non-NULL
    // pEventData (can be dereferenced as OMX_BOOL) indicates buffer underflow
    // this pointer remains valid only inside EventHandler
    OMX_EventBufferFullness = VENDOR_EVENT(0x1),

    // nData1 is data availability flag, nData2 is port index, non-NULL
    // pEventData is not used,
    // triggered when HTTP 404 happens in case of DASH, HLS, others and when segment started and data is available
    OMX_EventDataAvailability = VENDOR_EVENT(0x2)
};


#define DIVX_ExtraDataMetadata360 VENDOR_EXTRADATATYPE(0x1)

#endif
