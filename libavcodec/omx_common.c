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

#include "config.h"

#ifdef _WIN32
#include "compat/w32dlfcn.h"
#else
#include <dlfcn.h>
#endif

#include <time.h>

#include "libavutil/thread.h"

#include <stdio.h>
#include <stdlib.h>

#include "libavutil/internal.h"
#include "libavutil/avstring.h"
#include "libavutil/avutil.h"
#include "libavutil/common.h"
#include "libavutil/imgutils.h"
#include "libavutil/log.h"
#include "libavutil/opt.h"

#include "decode.h"
#include "avcodec.h"
#include "internal.h"
#include "omx_common.h"

#include <float.h>
// Our intention is to support any OMX Components implemented as shared objects. But component creation isn't specified
// by OMX spec, and is between IL Core and components. Right now it looks like we don't need IL Core, we could use
// component directly as soon as it loaded. So, let's start with it. Let's think that there is kinda "IL Core", but
// it's very thin and is spreaded over this file.
// Anyway, TODO : implement decent IL Core


static av_cold int populate_core_methods(OMXCoreLibrary *lib)
{
    lib->OMX_Init       = (void*)dlsym(lib->lib, "OMX_Init");
    lib->OMX_Deinit     = (void*)dlsym(lib->lib, "OMX_Deinit");
    lib->OMX_GetHandle  = (void*)dlsym(lib->lib, "OMX_GetHandle");
    lib->OMX_FreeHandle = (void*)dlsym(lib->lib, "OMX_FreeHandle");

    if (lib->OMX_Init && lib->OMX_Deinit && lib->OMX_GetHandle && lib->OMX_FreeHandle)
        return 0;
    else
        return AVERROR_UNKNOWN;
}

static av_cold int load_core_library(OMXCoreLibrary *l, void *logctx,
                                     const char *core_libname)
{
    int ret;

    if (core_libname) {
        l->lib = dlopen(core_libname, RTLD_NOW | RTLD_GLOBAL);

        if (!l->lib) {
            av_log(logctx, AV_LOG_ERROR, "%s not found\n", core_libname);
            return AVERROR_DECODER_NOT_FOUND;
        }

        ret = populate_core_methods(l);

        if (ret) {
            av_log(logctx, AV_LOG_WARNING, "OMX IL Core is invalid. Some methods are not exported.\n");
            dlclose(l->lib);
            l->lib = NULL;
            return AVERROR_DECODER_NOT_FOUND;
        }
    } else {
        av_log(logctx, AV_LOG_ERROR, "OMX IL Core filename wasn't provided.\n");
        return AVERROR_DECODER_NOT_FOUND;
    }

    return 0;
}

static av_cold int get_port_idx(AVCodecContext *avctx)
{
    OMXComponentContext *s = avctx->priv_data;
    int ret = 0;

    OMX_PORT_PARAM_TYPE port_params;
    INIT_STRUCT(port_params);

    if(s->avctx->codec_type == AVMEDIA_TYPE_VIDEO)
        ret = OMX_GetParameter(s->component, OMX_IndexParamVideoInit, &port_params);
    else
        ret = OMX_GetParameter(s->component, OMX_IndexParamAudioInit, &port_params);

    OMX_ERROR_CHECK(ret, avctx);

    if (port_params.nPorts != 2) {
        av_log(avctx, AV_LOG_ERROR, "OMX component has %li ports intead of two.\n", port_params.nPorts);
        return AVERROR_UNKNOWN;
    }

    s->nPorts = port_params.nPorts;
    s->nStartPortNumber = port_params.nStartPortNumber;

    for (int i = 0; i < s->nPorts; i++) {
        OMX_PARAM_PORTDEFINITIONTYPE port_def;

        pthread_mutex_lock(&s->buffers_mutex[i]);

        INIT_STRUCT(port_def);
        port_def.nPortIndex = s->nStartPortNumber + i;
        OMX_GetParameter(s->component, OMX_IndexParamPortDefinition, &port_def);

        if (port_def.eDir == OMX_DirInput)
            s->in_port_idx  = i;
        else
            s->out_port_idx = i;

        pthread_mutex_unlock(&s->buffers_mutex[i]);
    }

    return ret;
}

static av_cold int allocate_buffers(AVCodecContext *avctx, int only_output)
{
    OMXComponentContext *s = avctx->priv_data;
    int ret = 0;

    for (int i = (only_output ? s->out_port_idx : 0); i < (only_output ? s->out_port_idx+1 : s->nPorts); i++) {
        OMX_PARAM_PORTDEFINITIONTYPE port_def;

        pthread_mutex_lock(&s->buffers_mutex[i]);

        INIT_STRUCT(port_def);
        port_def.nPortIndex = s->nStartPortNumber + i;
        ret = OMX_GetParameter(s->component, OMX_IndexParamPortDefinition, &port_def);
        if (ret != OMX_ErrorNone) {
            pthread_mutex_unlock(&s->buffers_mutex[i]);
            break;
        }

        s->buffers_n[i] = port_def.nBufferCountMin;
        if (!only_output)
            s->buffers[i] = (OMX_BUFFERHEADERTYPE**)malloc(s->buffers_n[i] * sizeof(s->buffers[0][0]));

        for (int j=0; j < s->buffers_n[i]; j++) {
            OMX_BUFFERHEADERTYPE* buf = NULL;
            OMX_AllocateBuffer(s->component, &buf, port_def.nPortIndex, s, port_def.nBufferSize);

            s->buffers[i][j] = buf;
        }

        pthread_mutex_unlock(&s->buffers_mutex[i]);
    }

    return ret;
}

static av_cold int free_buffers(AVCodecContext *avctx, int only_output)
{
    OMXComponentContext *s = avctx->priv_data;

    for (int i = (only_output ? s->out_port_idx : 0); i < (only_output ? s->out_port_idx+1 : s->nPorts); i++) {
        OMX_U32 port_idx = s->nStartPortNumber + i;

        pthread_mutex_lock(&s->buffers_mutex[i]);

        for (int j=0; j < s->buffers_n[i]; j++) {
            OMX_FreeBuffer(s->component, port_idx, s->buffers[i][j]);
        }

        s->buffers_n[i] = 0;

        if (!only_output)
            free(s->buffers[i]);

        pthread_mutex_unlock(&s->buffers_mutex[i]);
    }

    return 0;
}

static OMX_ERRORTYPE event_handler(OMX_HANDLETYPE component, OMX_PTR app_data, OMX_EVENTTYPE event,
                                   OMX_U32 data1, OMX_U32 data2, OMX_PTR event_data)
{
    OMXComponentContext *s = app_data;

    switch (event) {
        case OMX_EventError:
            av_log(s->avctx, AV_LOG_ERROR, "OMX error 0x%lx\n", data1);

            if (data1 == OMX_ErrorInvalidState) {
                pthread_mutex_lock(&s->state_mutex);

                s->state = OMX_StateInvalid;

                pthread_cond_broadcast(&s->state_cond);
                pthread_mutex_unlock(&s->state_mutex);
            }
            break;
        case OMX_EventCmdComplete:
            if (data1 == OMX_CommandStateSet) {
                pthread_mutex_lock(&s->state_mutex);
                s->state = data2;
                av_log(s->avctx, AV_LOG_VERBOSE, "OMX state changed to %li\n", data2);
                pthread_cond_broadcast(&s->state_cond);
                pthread_mutex_unlock(&s->state_mutex);
            } else if (data1 == OMX_CommandPortDisable) {
                pthread_mutex_lock(&s->deiniting_mutex);
                if (!s->deiniting) {
                    pthread_mutex_lock(&s->state_mutex);
                    s->port_disabling = OMX_FALSE;
                    s->port_enabling = OMX_TRUE;
                    s->port_enable_command_was_send = OMX_FALSE;
                    pthread_mutex_unlock(&s->state_mutex);

                    pthread_mutex_lock(&s->buffers_cond_mutex);
                    pthread_cond_broadcast(&s->buffers_cond);
                    pthread_mutex_unlock(&s->buffers_cond_mutex);
                }
                pthread_mutex_unlock(&s->deiniting_mutex);
            } else if (data1 == OMX_CommandPortEnable) {
                pthread_mutex_lock(&s->deiniting_mutex);
                if (!s->deiniting) {
                    pthread_mutex_lock(&s->state_mutex);
                    s->port_enabling = OMX_FALSE;
                    pthread_mutex_unlock(&s->state_mutex);
                }
                pthread_mutex_unlock(&s->deiniting_mutex);
            } else {
                av_log(s->avctx, AV_LOG_VERBOSE, "OMX command complete, command 0x%lx, value 0x%lx\n", data1, data2);
            }
            break;
        case OMX_EventPortSettingsChanged:
            pthread_mutex_lock(&s->deiniting_mutex);
            if (!s->deiniting && !s->port_disabling && !s->port_enabling) {
                pthread_mutex_lock(&s->state_mutex);
                s->port_disabling = OMX_TRUE;
                s->port_disable_command_was_send = OMX_FALSE;
                pthread_mutex_unlock(&s->state_mutex);

                pthread_mutex_lock(&s->buffers_cond_mutex);
                pthread_cond_broadcast(&s->buffers_cond);
                pthread_mutex_unlock(&s->buffers_cond_mutex);
            }
            pthread_mutex_unlock(&s->deiniting_mutex);
            break;
        default:
            av_log(s->avctx, AV_LOG_VERBOSE, "OMX event %i, data1 0x%lx, data2 0x%lx\n",
                   event, data1, data2);
            break;
    }
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE empty_buffer_done(OMX_HANDLETYPE component, OMX_PTR app_data,
                                       OMX_BUFFERHEADERTYPE *buffer)
{
    OMXComponentContext *s = app_data;

    OMX_U32 port_idx = buffer->nInputPortIndex - s->nStartPortNumber;

    pthread_mutex_lock(&s->buffers_cond_mutex); // TODO : probably we could use single mutex istead of two
    pthread_mutex_lock(&s->buffers_mutex[port_idx]);

    s->buffers[port_idx][s->buffers_n[port_idx]++] = buffer;

    pthread_cond_broadcast(&s->buffers_cond);

    pthread_mutex_unlock(&s->buffers_mutex[port_idx]);
    pthread_mutex_unlock(&s->buffers_cond_mutex);

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE fill_buffer_done(OMX_HANDLETYPE component, OMX_PTR app_data,
                                      OMX_BUFFERHEADERTYPE *buffer)
{
    OMXComponentContext *s = app_data;

    OMX_U32 port_idx = buffer->nOutputPortIndex - s->nStartPortNumber;

    pthread_mutex_lock(&s->state_mutex);
    if (s->port_disabling && s->port_disable_command_was_send) {
        pthread_mutex_unlock(&s->state_mutex);

        OMX_FreeBuffer(component, buffer->nOutputPortIndex, buffer);
        return OMX_ErrorNone;
    }

    pthread_mutex_lock(&s->buffers_cond_mutex); // TODO : probably we could use single mutex istead of two
    pthread_mutex_lock(&s->buffers_mutex[port_idx]);

    s->buffers[port_idx][s->buffers_n[port_idx]++] = buffer;

    pthread_cond_broadcast(&s->buffers_cond);

    pthread_mutex_unlock(&s->buffers_mutex[port_idx]);
    pthread_mutex_unlock(&s->buffers_cond_mutex);

    pthread_mutex_unlock(&s->state_mutex);

    return OMX_ErrorNone;
}

static const OMX_CALLBACKTYPE omx_callbacks = {
        event_handler,
        empty_buffer_done,
        fill_buffer_done
};

static av_cold int wait_for_switch(OMXComponentContext *s, OMX_STATETYPE st)
{
    int ret = 0;

    pthread_mutex_lock(&s->state_mutex);

    while (s->state != st && s->state != OMX_StateInvalid)
        pthread_cond_wait(&s->state_cond, &s->state_mutex);

    if (s->state != st)
        ret = AVERROR_UNKNOWN;

    pthread_mutex_unlock(&s->state_mutex);

    return ret;
}

av_cold int omx_cmpnt_init(AVCodecContext *avctx)
{
    int ret = 0;
    OMXComponentContext *s = avctx->priv_data;

    av_log(avctx, AV_LOG_TRACE, "OMX component init\n");

    s->avctx = avctx;

    pthread_mutex_lock(&s->deiniting_mutex);
    s->deiniting = OMX_FALSE;
    pthread_mutex_unlock(&s->deiniting_mutex);

    ret = load_core_library(&s->core, avctx, s->core_libname);

    if (ret !=0)
        return ret;

    ret = s->core.OMX_Init();
    OMX_ERROR_CHECK(ret, avctx)

    ret = s->core.OMX_GetHandle(&s->component, s->component_name, s, &omx_callbacks);
    OMX_ERROR_CHECK(ret, avctx)

    if (!s->component) {
        av_log(avctx, AV_LOG_ERROR, "Component cannot be created\n");
        return AVERROR_UNKNOWN;
    }

    get_port_idx(avctx);

    s->state = OMX_StateLoaded;

    s->codec_config = NULL;

    return 0;
}

av_cold int omx_cmpnt_start(AVCodecContext *avctx)
{
    int ret = 0;
    OMXComponentContext *s = avctx->priv_data;

    av_log(avctx, AV_LOG_TRACE, "OMX component start\n");

    OMX_SendCommand(s->component, OMX_CommandStateSet, OMX_StateIdle, NULL);

    ret = allocate_buffers(avctx, OMX_FALSE);
    if (ret) {
        av_log(avctx, AV_LOG_ERROR, "Component cannot allocate buffers\n");
        return AVERROR_UNKNOWN;
    }

    ret = wait_for_switch(s, OMX_StateIdle);
    if (ret) {
        av_log(avctx, AV_LOG_ERROR, "Component cannot switch to Idle state\n");
        return AVERROR_UNKNOWN;
    }

    OMX_SendCommand(s->component, OMX_CommandStateSet, OMX_StateExecuting, NULL);

    ret = wait_for_switch(s, OMX_StateExecuting);
    if (ret) {
        av_log(avctx, AV_LOG_ERROR, "Component cannot switch to Executing state\n");
        return AVERROR_UNKNOWN;
    }

    return 0;
}

av_cold int omx_cmpnt_end(AVCodecContext *avctx)
{
    OMXComponentContext *s = avctx->priv_data;

    int ret = 0;

    av_log(avctx, AV_LOG_TRACE, "OMX component end\n");

    pthread_mutex_lock(&s->deiniting_mutex);
    s->deiniting = OMX_TRUE;
    pthread_mutex_unlock(&s->deiniting_mutex);

    // We expecting that component is in executing state.
    OMX_SendCommand(s->component, OMX_CommandStateSet, OMX_StateIdle, NULL);
    ret = wait_for_switch(s, OMX_StateIdle);

    OMX_SendCommand(s->component, OMX_CommandStateSet, OMX_StateLoaded, NULL);
    ret = free_buffers(avctx, OMX_FALSE);
    ret = wait_for_switch(s, OMX_StateLoaded);

    if (s->component)
        s->core.OMX_FreeHandle(s->component);

    s->core.OMX_Deinit();

    av_free(avctx->extradata);
    avctx->extradata = NULL;

    return ret;
}

#define OFFSET(x) offsetof(OMXComponentContext, x)
#define ED  AV_OPT_FLAG_AUDIO_PARAM | AV_OPT_FLAG_DECODING_PARAM | AV_OPT_FLAG_ENCODING_PARAM | AV_OPT_FLAG_VIDEO_PARAM  

const AVOption omx_options[] = {
        { "omx_core" , "OMX Core library name"   , OFFSET(core_libname)   , AV_OPT_TYPE_STRING, { .str = "" }, 0, 0, ED },
        { "omx_name" , "OMX component name"      , OFFSET(component_name) , AV_OPT_TYPE_STRING, { .str = "" }, 0, 0, ED },
        { "omx_param", "OMX component parameters", OFFSET(component_param), AV_OPT_TYPE_STRING, { .str = "" }, 0, 0, ED },

        { NULL }
};

static OMX_BUFFERHEADERTYPE* omx_get_output_buffer(OMXComponentContext *s)
{
    OMX_BUFFERHEADERTYPE* buf = NULL;

    while (s->buffers_n[s->out_port_idx]) {
        s->buffers_n[s->out_port_idx]--;

        buf = s->buffers[s->out_port_idx][0];
        memmove(&s->buffers[s->out_port_idx][0], &s->buffers[s->out_port_idx][1],
                sizeof(s->buffers[s->out_port_idx][0]) * s->buffers_n[s->out_port_idx]);

        if (buf->nFilledLen == 0 && !(buf->nFlags & OMX_BUFFERFLAG_EOS)) { // Buffer is empty, so let component fill it
//            if (s->port_disabling && s->port_disable_command_was_send) {
//                OMX_FreeBuffer(s->component, buf->nOutputPortIndex, buf);
//                return NULL;
//            } else
            if (OMX_FillThisBuffer(s->component, buf) == OMX_ErrorIncorrectStateOperation) {  // TODO : very fragile. And what if another error occurs?
                s->buffers[s->out_port_idx][s->buffers_n[s->out_port_idx]] = buf;
                s->buffers_n[s->out_port_idx]++;
                return NULL;
            }

        } else { // Buffer was filled
            return buf;
        }
    }

    return NULL;
}

OMX_BUFFERHEADERTYPE* omx_pick_output_buffer(OMXComponentContext *s)
{
    OMX_BUFFERHEADERTYPE* buf = NULL;

    pthread_mutex_lock(&s->buffers_mutex[s->out_port_idx]);

    buf = omx_get_output_buffer(s);

    pthread_mutex_unlock(&s->buffers_mutex[s->out_port_idx]);

    return buf;
}

OMX_BUFFERHEADERTYPE* omx_pick_input_buffer(OMXComponentContext *s)
{
    OMX_BUFFERHEADERTYPE* buf = NULL;

    pthread_mutex_lock(&s->buffers_mutex[s->in_port_idx]);

    if (s->buffers_n[s->in_port_idx]) {
        s->buffers_n[s->in_port_idx]--;

        buf = s->buffers[s->in_port_idx][s->buffers_n[s->in_port_idx]];
    }

    pthread_mutex_unlock(&s->buffers_mutex[s->in_port_idx]);

    return buf;
}

static void omx_send_port_commands(OMXComponentContext *s)
{
    // TODO : What a mess.
    pthread_mutex_lock(&s->state_mutex);
    if (s->port_disabling && !s->port_disable_command_was_send) {
        pthread_mutex_unlock(&s->state_mutex);
        OMX_SendCommand(s->component, OMX_CommandPortDisable, s->nStartPortNumber + s->out_port_idx, 0);
        pthread_mutex_lock(&s->state_mutex);
        s->port_disable_command_was_send = OMX_TRUE;
        pthread_mutex_unlock(&s->state_mutex);
        free_buffers(s->avctx, OMX_TRUE);
        pthread_mutex_lock(&s->state_mutex);
    }
    if (s->port_enabling && !s->port_enable_command_was_send) {
        pthread_mutex_unlock(&s->state_mutex);
        OMX_SendCommand(s->component, OMX_CommandPortEnable, s->nStartPortNumber + s->out_port_idx, 0);
        pthread_mutex_lock(&s->state_mutex);
        s->port_enable_command_was_send = OMX_TRUE;
        pthread_mutex_unlock(&s->state_mutex);
        allocate_buffers(s->avctx, OMX_TRUE);
        pthread_mutex_lock(&s->state_mutex);
    }
    pthread_mutex_unlock(&s->state_mutex);
}

void omx_wait_any_buffer(AVCodecContext *avctx, OMX_BUFFERHEADERTYPE** out_buf, OMX_BUFFERHEADERTYPE** in_buf)
{
    OMXComponentContext *s = avctx->priv_data;

    do { // TODO aaaaaaaargh
        omx_send_port_commands(s);

        pthread_mutex_lock(&s->buffers_cond_mutex);

        *out_buf = omx_pick_output_buffer(s);

        if (!*out_buf) {
            *in_buf = omx_pick_input_buffer(s);
        } else {
            *in_buf = NULL;
        }

        if (!*in_buf && !*out_buf)
            pthread_cond_wait(&s->buffers_cond, &s->buffers_cond_mutex);

        pthread_mutex_unlock(&s->buffers_cond_mutex);
    } while (!*out_buf && !*in_buf);

    assert(!!*out_buf ^ !!*in_buf);

    return;
}

OMX_BUFFERHEADERTYPE* omx_wait_output_buffer(OMXComponentContext *s)
{
    OMX_BUFFERHEADERTYPE* buf = NULL;

    omx_send_port_commands(s);

    pthread_mutex_lock(&s->buffers_cond_mutex);

    buf = omx_pick_output_buffer(s);

    while (!buf) {
        pthread_cond_wait(&s->buffers_cond, &s->buffers_cond_mutex);

        pthread_mutex_unlock(&s->buffers_cond_mutex);
        omx_send_port_commands(s);
        pthread_mutex_lock(&s->buffers_cond_mutex);

        buf = omx_pick_output_buffer(s);
    };
    pthread_mutex_unlock(&s->buffers_cond_mutex);

    return buf;
}

OMX_BUFFERHEADERTYPE* omx_wait_input_buffer(OMXComponentContext *s) // TODO : there are several copypasted wait* and pick* functions
{
    OMX_BUFFERHEADERTYPE* buf = NULL;

    pthread_mutex_lock(&s->buffers_cond_mutex);

    buf = omx_pick_input_buffer(s);

    while (!buf) {
        pthread_cond_wait(&s->buffers_cond, &s->buffers_cond_mutex);

        buf = omx_pick_input_buffer(s);
    };
    pthread_mutex_unlock(&s->buffers_cond_mutex);

    return buf;
}

int omx_input_buffers_n(OMXComponentContext *s)
{
    int n = 0;

    pthread_mutex_lock(&s->buffers_mutex[s->in_port_idx]);

    n = s->buffers_n[s->in_port_idx];

    pthread_mutex_unlock(&s->buffers_mutex[s->in_port_idx]);

    return n;
}

void  omx_put_input_buffer(OMXComponentContext *s,OMX_BUFFERHEADERTYPE * buf)
{
    pthread_mutex_lock(&s->buffers_mutex[s->in_port_idx]);

    s->buffers[s->in_port_idx][s->buffers_n[s->in_port_idx]++] = buf;

    pthread_mutex_unlock(&s->buffers_mutex[s->in_port_idx]);
}
