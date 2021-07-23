/*
 * OMX Common
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

#ifndef AVCODEC_OMX_COMMON_H
#define AVCODEC_OMX_COMMON_H

#include "internal.h"

#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_Extension.h>
//#include <OMX_MainConcept.h>

#define OMX_COMPONENT_VERSION_VERSIONMAJOR 0x1
#define OMX_COMPONENT_VERSION_VERSIONMINOR 0x1
#define OMX_COMPONENT_VERSION_REVISION 0x2
#define OMX_COMPONENT_VERSION_STEP 0x0

#define OMX_TRUE  1 // TODO : Are there any ffmpeg-wide defines for true/false?
#define OMX_FALSE 0

#define OMX_MAX(x, y) ((x)>(y) ? (x) : (y))

#ifdef OMX_SKIP64BIT
static OMX_TICKS to_omx_ticks(int64_t value)
{
    OMX_TICKS s;
    s.nLowPart  = value & 0xffffffff;
    s.nHighPart = value >> 32;
    return s;
}
static int64_t from_omx_ticks(OMX_TICKS value)
{
    return (((int64_t)value.nHighPart) << 32) | value.nLowPart;
}
#else
#define to_omx_ticks(x) (x)
#define from_omx_ticks(x) (x)
#endif

#define INIT_STRUCT(x) do {                                                 \
        memset(&(x), 0, sizeof(x));                                         \
        (x).nSize = sizeof(x);                                              \
        (x).nVersion.s.nVersionMajor = OMX_COMPONENT_VERSION_VERSIONMAJOR;  \
        (x).nVersion.s.nVersionMinor = OMX_COMPONENT_VERSION_VERSIONMINOR;  \
        (x).nVersion.s.nRevision = OMX_COMPONENT_VERSION_REVISION;          \
        (x).nVersion.s.nStep = OMX_COMPONENT_VERSION_STEP;                  \
    } while (0)

#define CHECK(x) do {                                                     \
        if (x != OMX_ErrorNone) {                                         \
            av_log(avctx, AV_LOG_ERROR,                                   \
                   "err %x (%d) on line %d\n", x, x, __LINE__);           \
            return AVERROR_UNKNOWN;                                       \
        }                                                                 \
    } while (0)

typedef struct OMXCoreLibrary {
    void *lib;

    OMX_ERRORTYPE (*OMX_Init)(void);
    OMX_ERRORTYPE (*OMX_Deinit)(void);
    OMX_ERRORTYPE (*OMX_GetHandle)(OMX_HANDLETYPE*, OMX_STRING, OMX_PTR, const OMX_CALLBACKTYPE*);
    OMX_ERRORTYPE (*OMX_FreeHandle)(OMX_HANDLETYPE);
} OMXCoreLibrary;

#define MAX_PORT_NUMBER 16

typedef struct OMXComponentContext {
    const AVClass *class;
    void* avctx;

    char* core_libname;
    char* component_name;
    char* component_param;

    int profile;

    OMXCoreLibrary core;

    OMX_HANDLETYPE component;

    pthread_mutex_t state_mutex; // State mutex and conditional are used in port disabling-enabling routine as well
    pthread_cond_t  state_cond;

    OMX_STATETYPE state;

    OMX_BUFFERHEADERTYPE** buffers[MAX_PORT_NUMBER];
    OMX_U32 buffers_n[MAX_PORT_NUMBER];

    OMX_U32 nStartPortNumber[OMX_PortDomainOther+1];
    OMX_U32 nPorts[OMX_PortDomainOther+1];

    OMX_U32 port_idx[MAX_PORT_NUMBER];
    OMX_U32 port_out[MAX_PORT_NUMBER];
    OMX_U32 port_num;
    OMX_BOOL port_disabled[MAX_PORT_NUMBER];

    pthread_mutex_t buffers_mutex[MAX_PORT_NUMBER];
    pthread_mutex_t buffers_cond_mutex;
    pthread_cond_t  buffers_cond;

    OMX_BOOL        eos_flag;

    int             port_enabling; // Single port per time. It won't work with components with several output ports.
    int             port_disabling;
    int             port_disable_command_was_send;
    int             port_enable_command_was_send;

    int             deiniting;
    pthread_mutex_t deiniting_mutex;

    int             a53_cc;
    void*           codec_config;
    unsigned int    codec_config_len;
} OMXComponentContext;


#define OMX_ERROR_CHECK(x, logctx) { if (OMX_ErrorNone != (x)) \
                    {av_log(logctx, AV_LOG_ERROR, "OMX Error 0x%x\n", (x));\
                    return AVERROR_UNKNOWN; } }

extern const struct AVOption omx_options[];

// Component life cycle
av_cold int omx_cmpnt_init(OMXComponentContext *s);
av_cold int omx_cmpnt_start(OMXComponentContext *s);
av_cold int omx_cmpnt_end(OMXComponentContext *s);
//av_cold int omx_cmpnt_parse_param(AVCodecContext *avctx);

// Helpers
int omx_port_idx(struct OMXComponentContext* s, int output);
int omx_out_port_idx(struct OMXComponentContext* s);
int omx_in_port_idx(struct OMXComponentContext* s);
int omx_disable_port(struct OMXComponentContext *s, OMX_U32 port_idx);
int rev_port_idx(struct OMXComponentContext *s, int omx_port_idx);

// Buffer queue operations
void omx_wait_any_buffer(AVCodecContext *avctx, OMX_BUFFERHEADERTYPE** out_buf, OMX_BUFFERHEADERTYPE** in_buf);
OMX_BUFFERHEADERTYPE* omx_wait_input_buffer(OMXComponentContext *s);
OMX_BUFFERHEADERTYPE* omx_wait_input_buffer_n(OMXComponentContext *s, int port_num);
OMX_BUFFERHEADERTYPE* omx_pick_input_buffer(OMXComponentContext *s);
OMX_BUFFERHEADERTYPE* omx_pick_input_buffer_n(OMXComponentContext *s, int port_num);
OMX_BUFFERHEADERTYPE* omx_wait_output_buffer(OMXComponentContext *s);
OMX_BUFFERHEADERTYPE* omx_pick_output_buffer(OMXComponentContext *s);
int omx_input_buffers_n(OMXComponentContext *s);
void omx_put_input_buffer(OMXComponentContext *s, OMX_BUFFERHEADERTYPE * buf);

#endif // #ifndef AVCODEC_OMX_COMMON_H
