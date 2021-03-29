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
} TIMESTAMP_PARAM;


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
