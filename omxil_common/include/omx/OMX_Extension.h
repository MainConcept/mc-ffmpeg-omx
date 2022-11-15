/*
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

#ifndef OMXIL_COMMON_INCLUDE_OMX_OMX_EXTENSION_H
#define OMXIL_COMMON_INCLUDE_OMX_OMX_EXTENSION_H

#include <OMX_Index.h>
#include <OMX_Video.h>
#include <OMX_Image.h>
#include <OMX_Audio.h>
#include <OMX_Other.h>
#include <OMX_Core.h>

/**
 * Q16 Multiplier constant.
 */
#define Q16_SHIFT (0x10000)

/**
 * Helper macros
 **/
#define VENDOR_AUDIO_INDEX(num)     VENDOR_INDEX(num)
#define VENDOR_VIDEO_INDEX(num)     VENDOR_INDEX(num) + 0x100000
#define VENDOR_IMAGE_INDEX(num)     VENDOR_INDEX(num) + 0x200000
#define VENDOR_OTHER_INDEX(num)     VENDOR_INDEX(num) + 0x300000
#define VENDOR_CONFIG_INDEX(num)    VENDOR_INDEX(num) + 0x500000

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

/**
 * Custom colorspaces enum
 **/
#define OMX_COLOR_FormatYUV420PackedPlanar10bit VENDOR_COLOR_FORMAT(2)
#define OMX_COLOR_FormatYUV422PackedPlanar10bit VENDOR_COLOR_FORMAT(3)
#define OMX_COLOR_FormatBGRPackedPlanar10bit VENDOR_COLOR_FORMAT(4)

/**
 * Custom parameter structures
 **/
typedef struct OMX_VENDOR_PARAM_COMMANDLINETYPE {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;

    OMX_U32 nCommandlineMaxSize;
    OMX_U32 nCommandlineSizeUsed;
    OMX_U8  data[1];
} OMX_VENDOR_PARAM_COMMANDLINETYPE;

typedef struct TIMESTAMP_PARAM {
    OMX_TICKS DTS;
    OMX_TICKS duration;
} TIMESTAMP_PARAM;

/**
 * Dolby Digital structures were copied from https://github.com/raspberrypi/firmware/blob/master/opt/vc/include/IL/OMX_Audio.h
 */
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
    OMX_U32 nChannels;             /**< Number of channels */
    OMX_U32 nBitRate;              /**< Bit rate of the input data.  Use 0 for variable
                                        rate or unknown bit rates */
    OMX_U32 nSampleRate;           /**< Sampling rate of the source data. Use 0 for
                                        variable or unknown sampling rate. */
    OMX_AUDIO_DDPBITSTREAMID eBitStreamId;
    OMX_AUDIO_DDPBITSTREAMMODE eBitStreamMode;
    OMX_AUDIO_DDPDOLBYSURROUNDMODE eDolbySurroundMode;
    OMX_AUDIO_CHANNELTYPE eChannelMapping[OMX_AUDIO_MAXCHANNELS]; /**< Slot i contains channel defined by eChannelMapping[i] */
} OMX_AUDIO_PARAM_DDPTYPE;

/** xHEAAC params */
typedef struct OMX_AUDIO_PARAM_XHEAACTYPE {
    OMX_U32 nSize;                 /**< size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;      /**< OMX specification version information */
    OMX_U32 nPortIndex;            /**< port that this structure applies to */
    OMX_S32 nMaxBitRate;
    OMX_S32 nAvgBitRate;
    OMX_S32 nBufferSize;
    OMX_S32 nRapInterval;
    OMX_S32 nPriming;
    OMX_S32 nStandardDelay;
    OMX_S32 nFrameSamples;
    OMX_U32 nCodecConfigMaxSize;
    OMX_U32 nCodecConfigSizeUsed;
    OMX_U8  CodecConfig[1];
} OMX_AUDIO_PARAM_XHEAACTYPE;


/** Video codec config params */
typedef struct OMX_VIDEO_PARAM_CODECCONFIGTYPE {
    OMX_U32 nSize;                 /**< size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;      /**< OMX specification version information */
    OMX_U32 nPortIndex;            /**< port that this structure applies to */
    OMX_U32 nCodecConfigMaxSize;
    OMX_U32 nCodecConfigSizeUsed;
    OMX_U8  CodecConfig[1];
} OMX_VIDEO_PARAM_CODECCONFIGTYPE;


typedef enum OMX_INDEXTYPE_EXT {
    OMX_IndexParamAudioDdp = 0x7f00007B,           /**< reference: OMX_AUDIO_PARAM_DDPTYPE */
    OMX_IndexParamAudioXheaac = 0x7f00007C,        /**< reference: OMX_AUDIO_PARAM_XHEAACTYPE */
    OMX_IndexParamVideoCodecConfig = 0x7f00007D,   /**< reference: OMX_VIDEO_PARAM_CODECCONFIGTYPE */
} OMX_INDEXTYPE_EXT;
/*
 * End of Dolby structures copied from https://github.com/raspberrypi/firmware/blob/master/opt/vc/include/IL/OMX_Audio.h
 */

/*
 * OMX_NumericalDataFloat = 0x7F000001,
 * Was copied from https://github.com/LineageOS/android_frameworks_native/blob/lineage-18.1/headers/media_plugin/media/openmax/OMX_Types.h
 * It extends OMX_NUMERICALDATATYPE
 */
typedef enum OMX_NUMERICALDATATYPE_EXT
{
    OMX_NumericalDataFloat = 0x7F000001
} OMX_NUMERICALDATATYPE_EXT;

typedef enum OMX_AUDIO_AACPROFILETYPE_EXT
{
    OMX_AUDIO_AACObjectXHE = 0x7F000001
} OMX_AUDIO_AACPROFILETYPE_EXT;

/*
 * it was taken here: https://review.carbonrom.org/plugins/gitiles/CarbonROM/android_hardware_qcom_media/+/fa202b9b18f17f7835fd602db5fff530e61112b4/msmcobalt/mm-core/inc/OMX_QCOMExtns.h
 * these are part of OMX1.2 but JB MR2 branch doesn't have them defined
 * OMX_IndexParamInterlaceFormat
 * OMX_INTERLACEFORMATTYPE
 */

typedef struct OMX_INTERLACEFORMATTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nFormat;//OMX_INTERLACETYPE
    OMX_TICKS nTimeStamp;

} OMX_INTERLACEFORMATTYPE;

typedef enum OMX_INTERLACETYPE
{
    OMX_InterlaceFrameProgressive,
    OMX_InterlaceInterleaveFrameTopFieldFirst,
    OMX_InterlaceInterleaveFrameBottomFieldFirst,
    OMX_InterlaceFrameTopFieldFirst,
    OMX_InterlaceFrameBottomFieldFirst,
    OMX_InterlaceInterleaveFieldTop,
    OMX_InterlaceInterleaveFieldBottom
} OMX_INTERLACES;

typedef struct OMX_ASPECT_RATIO
{
    OMX_U32 aspectRatioX;
    OMX_U32 aspectRatioY;
} OMX_ASPECT_RATIO;


/*
 * it was taken here:https://android.googlesource.com/platform/frameworks/native/+/android-7.1.1_r58/include/media/hardware/VideoAPI.h
 */

 // this is in sync with the range values in graphics.h
typedef enum OMX_COLOR_RANGE {
    RangeUnspecified,
    RangeFull,
    RangeLimited,
    RangeOther = 0xff,
} OMX_COLOR_RANGE;

typedef enum OMX_COLOR_PRIMARIES {
    PrimariesUnspecified,
    PrimariesBT709_5,       // Rec.ITU-R BT.709-5 or equivalent
    PrimariesBT709_6,       // Rec.ITU-R BT.709-6 or equivalent
    PrimariesBT470_6M,      // Rec.ITU-R BT.470-6 System M or equivalent
    PrimariesBT470_6BG,     // Rec.ITU-R BT.470-6 System B G or equivalent
    PrimariesBT601_6_625,   // Rec.ITU-R BT.601-6 625 or equivalent
    PrimariesBT601_6_525,   // Rec.ITU-R BT.601-6 525 or equivalent
    PrimariesGenericFilm,   // Generic Film
    PrimariesBT2020,        // Rec.ITU-R BT.2020 or equivalent
    Primaries_SMPTE_170M,
    Primaries_SMPTE_240M,
    Primaries_SMPTEST428_1,
    PrimariesOther = 0xff,
} OMX_COLOR_PRIMARIES;
// this partially in sync with the transfer values in graphics.h prior to the transfers
// unlikely to be required by Android section
typedef enum OMX_COLOR_TRANSFER {
    TransferUnspecified,
    TransferLinear,         // Linear transfer characteristics
    TransferSRGB,           // sRGB or equivalent
    TransferSMPTE170M,      // SMPTE 170M or equivalent (e.g. BT.601/709/2020)
    TransferGamma22,        // Assumed display gamma 2.2
    TransferGamma28,        // Assumed display gamma 2.8
    TransferST2084,         // SMPTE ST 2084 for 10/12/14/16 bit systems
    TransferHLG,            // ARIB STD-B67 hybrid-log-gamma
    // transfers unlikely to be required by Android
    TransferSMPTE240M = 0x40, // SMPTE 240M
    TransferXvYCC,          // IEC 61966-2-4
    TransferBT1361,         // Rec.ITU-R BT.1361 extended gamut
    TransferST428,          // SMPTE ST 428-1
    Transfer_BT2020_10,
    Transfer_BT2020_12,
    Transfer_IEC61966_2_1,
    Transfer_IEC61966_2_4,
    Transfer_BT1361_0,
    TransferOther = 0xff,
} OMX_COLOR_TRANSFER;

typedef enum OMX_COLOR_MATRIX_COEFFS {
    MatrixUnspecified,
    MatrixBT709_5,          // Rec.ITU-R BT.709-5 or equivalent
    MatrixBT709_6,          // Rec.ITU-R BT.709-6 or equivalent
    MatrixBT470_6M,         // KR=0.30, KB=0.11 or equivalent
    MatrixBT470_6BG,
    MatrixBT601_6,          // Rec.ITU-R BT.601-6 625 or equivalent
    MatrixSMPTE240M,        // SMPTE 240M or equivalent
    MatrixBT2020,           // Rec.ITU-R BT.2020 non-constant luminance
    MatrixBT2020Constant,   // Rec.ITU-R BT.2020 constant luminance
    MatrixYCGCO,
    MatrixRGB,
    MatrixFCC,
    MatrixOther = 0xff,
} OMX_COLOR_MATRIX_COEFFS;
// this is in sync with the standard values in graphics.h
typedef enum  OMX_COLOR_STANDART {
    StandardUnspecified,
    StandardBT709,                  // PrimariesBT709_5 and MatrixBT709_5
    StandardBT601_625,              // PrimariesBT601_6_625 and MatrixBT601_6
    StandardBT601_625_Unadjusted,   // PrimariesBT601_6_625 and KR=0.222, KB=0.071
    StandardBT601_525,              // PrimariesBT601_6_525 and MatrixBT601_6
    StandardBT601_525_Unadjusted,   // PrimariesBT601_6_525 and MatrixSMPTE240M
    StandardBT2020,                 // PrimariesBT2020 and MatrixBT2020
    StandardBT2020Constant,         // PrimariesBT2020 and MatrixBT2020Constant
    StandardBT470M,                 // PrimariesBT470_6M and MatrixBT470_6M
    StandardFilm,                   // PrimariesGenericFilm and KR=0.253, KB=0.068
    StandardOther = 0xff,
}OMX_COLOR_STANDART;

typedef struct OMX_COLOR_ASPECT {
    OMX_COLOR_RANGE mRange;                // IN/OUT
    OMX_COLOR_PRIMARIES mPrimaries;        // IN/OUT
    OMX_COLOR_TRANSFER mTransfer;          // IN/OUT
    OMX_COLOR_MATRIX_COEFFS mMatrixCoeffs; // IN/OUT
} OMX_COLOR_ASPECT;


/**
 * Custom parameter indices
 **/
#define OMX_IndexParamVendorCommandline VENDOR_PARAM_INDEX(0)   /**< reference: OMX_VENDOR_PARAM_COMMANDLINETYPE */

#ifndef OMX_IndexParamInterlaceFormat
#define OMX_IndexParamInterlaceFormat VENDOR_PARAM_INDEX(1)
#endif

 /**
  * Custom ExtraData id
  **/
#define OMX_ExtraDataDTS VENDOR_EXTRADATATYPE(0)   /**< reference: OMX_OTHER_EXTRADATATYPE */
#define OMX_ExtraDataA53CC VENDOR_EXTRADATATYPE(1) /**< reference: OMX_OTHER_EXTRADATATYPE */
#define OMX_ExtraDataSeekInfo VENDOR_EXTRADATATYPE(2) /**< reference: OMX_OTHER_EXTRADATATYPE */
#define OMX_ExtraDataAspectRatio VENDOR_EXTRADATATYPE(3)/**< reference: OMX_OTHER_EXTRADATATYPE */
#define OMX_ExtraDataInterlaceFormat VENDOR_EXTRADATATYPE(4)/**< reference: OMX_OTHER_EXTRADATATYPE */
#define OMX_ExtraDataVideoPictureType VENDOR_EXTRADATATYPE(5)/**< reference: OMX_OTHER_EXTRADATATYPE */
#define OMX_ExtraDataColorAspect VENDOR_EXTRADATATYPE(6)/**< reference: OMX_OTHER_EXTRADATATYPE */

#endif // OMXIL_COMMON_INCLUDE_OMX_OMX_EXTENSION_H
