/*
 * Copyright (c) 2021 MainConcept GmbH or its affiliates.
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

typedef enum OMX_INDEXTYPE_EXT {
    OMX_IndexParamAudioDdp = 0x7f00007B,       /**< reference: OMX_AUDIO_PARAM_DDPTYPE */
    OMX_IndexParamAudioXheaac = 0x7f00007C,       /**< reference: OMX_AUDIO_PARAM_XHEAACTYPE */
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

/**
 * Custom parameter indices
 **/
#define OMX_IndexParamVendorCommandline VENDOR_PARAM_INDEX(0)   /**< reference: OMX_VENDOR_PARAM_COMMANDLINETYPE */

 /**
  * Custom ExtraData id
  **/
#define OMX_ExtraDataDTS VENDOR_EXTRADATATYPE(0)   /**< reference: OMX_OTHER_EXTRADATATYPE */
#define OMX_ExtraDataA53CC VENDOR_EXTRADATATYPE(1) /**< reference: OMX_OTHER_EXTRADATATYPE */

#endif // OMXIL_COMMON_INCLUDE_OMX_OMX_EXTENSION_H
