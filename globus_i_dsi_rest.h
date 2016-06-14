/*
 * Copyright 1999-2016 University of Chicago
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file globus_i_dsi_rest.h GridFTP DSI REST Helper API Internals
 */

#ifndef GLOBUS_I_DSI_REST_H
#define GLOBUS_I_DSI_REST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "globus_dsi_rest.h"
#include "globus_common.h"
#include "globus_gridftp_server.h"

#include <curl/curl.h>
#include <jansson.h>

typedef
struct globus_i_dsi_rest_read_json_arg_s
{
    size_t                              buffer_used;
    size_t                              buffer_len;
    char                               *buffer;
    json_t                            **json_out;
}
globus_i_dsi_rest_read_json_arg_t;

typedef struct
globus_i_dsi_rest_buffer_s
{
    size_t                              buffer_len;
    size_t                              buffer_used;
    uint64_t                            transfer_offset;
    struct globus_i_dsi_rest_buffer_s  *next;
    unsigned char                       buffer[];
}
globus_i_dsi_rest_buffer_t;

typedef struct
globus_i_dsi_rest_idle_arg_s
{
    globus_abstime_t                    last_activity;
    uintptr_t                           idle_timeout;
}
globus_i_dsi_rest_idle_arg_t;

typedef
struct globus_i_dsi_rest_gridftp_op_arg_s
{
    globus_gfs_operation_t              op;
    globus_mutex_t                      mutex;
    globus_cond_t                       cond;
    globus_result_t                     result;

    uint64_t                            offset;
    uint64_t                            total;
    bool                                eof;

    globus_i_dsi_rest_buffer_t         *pending_buffers;
    globus_i_dsi_rest_buffer_t        **pending_buffers_last;
    globus_i_dsi_rest_buffer_t         *current_buffer;
    globus_i_dsi_rest_buffer_t         *registered_buffers;
    globus_i_dsi_rest_buffer_t         *free_buffers;
}
globus_i_dsi_rest_gridftp_op_arg_t;

/**
 * @brief Data Structure for request state
 */
typedef
struct globus_i_dsi_rest_request_s
{
    CURL                               *handle;
    globus_result_t                     result;
    const char                         *method;
    int                                 response_code;
    char                                response_reason[64];
    struct curl_slist                  *request_headers;
    globus_dsi_rest_key_array_t         response_headers;
    char                               *complete_uri;

    globus_dsi_rest_callbacks_t         callbacks;
    globus_thread_t                     thread;

    globus_dsi_rest_write_block_arg_t   write_block_callback_arg;
    globus_i_dsi_rest_read_json_arg_t   read_json_arg;
    globus_i_dsi_rest_gridftp_op_arg_t  gridftp_op_arg;
    globus_i_dsi_rest_idle_arg_t        idle_arg;

    uint64_t                            request_content_length;
    bool                                request_content_length_set;
}
globus_i_dsi_rest_request_t;

/**
 * @brief Obtain a CURL handle to use for a request
 * @details
 *     Allocates or reuses a CURL handle to use for a request. The new
 *     handle is returned in the value pointed to by handlep.
 *
 * @param[inout] handlep
 *     Pointer to storage to hold the resulting CURL * value.
 * @param[in] callback_arg
 *     Request data to associate with all curl callbacks.
 * @return
 *     On success, return GLOBUS_SUCCESS and set handlep to the handle.
 *     Otherwise, return an error result.
 */
globus_result_t
globus_i_dsi_rest_handle_get(
    CURL                              **handlep,
    void                               *callback_arg);

/**
 * @brief Release a CURL handle after completion of a request
 * @details
 *     Frees or caches a CURL handle no longer needed for a request. The handle
 *     must no longer be referenced by the caller.
 *
 * @param[in] handle
 *     Pointer to the CURL handle to release.
 * @return
 *     On success, return GLOBUS_SUCCESS. Otherwise, return an error result.
 */
void
globus_i_dsi_rest_handle_release(
    CURL                               *handle);


globus_result_t
globus_i_dsi_rest_compute_headers(
    struct curl_slist                 **request_headers,
    const globus_dsi_rest_key_array_t  *headers);

globus_result_t
globus_i_dsi_rest_set_request(
    CURL                               *curl,
    const char                         *method,
    const char                         *uri,
    struct curl_slist                  *headers,
    const globus_dsi_rest_callbacks_t  *callbacks);

globus_result_t
globus_i_dsi_rest_add_header(
    struct curl_slist                 **request_headers,
    const char                         *header_name,
    const char                         *header_value);

globus_result_t
globus_i_dsi_rest_perform(
    globus_i_dsi_rest_request_t        *request);

globus_result_t
globus_i_dsi_rest_encode_form_data(
    const globus_dsi_rest_key_array_t  *form_fields,
    char                              **form_datap);

globus_i_dsi_rest_buffer_t *
globus_i_dsi_rest_buffer_get(
    globus_i_dsi_rest_gridftp_op_arg_t *gridftp_op_arg,
    size_t                              size);

void
globus_i_dsi_rest_uri_escape(
    const char                         *raw,
    char                              **encodedp,
    size_t                             *availablep);

/* Callbacks that are passed to libcurl that cause user-specific callbacks */
int
globus_i_dsi_rest_xferinfo(
    void                               *callback_arg,
    curl_off_t                          dltotal,
    curl_off_t                          dlnow,
    curl_off_t                          ultotal,
    curl_off_t                          ulnow);

int
globus_i_dsi_rest_progress(
    void                               *callback_arg,
    double                              dltotal,
    double                              dlnow,
    double                              ultotal,
    double                              ulnow);

size_t
globus_i_dsi_rest_header(
    char                               *buffer,
    size_t                              size,
    size_t                              nitems,
    void                               *callback_arg);

size_t
globus_i_dsi_rest_write_data(
    char                               *ptr,
    size_t                              size,
    size_t                              nmemb,
    void                               *callback_arg);

size_t 
globus_i_dsi_rest_read_data(
    char                               *buffer,
    size_t                              size,
    size_t                              nitems,
    void                               *callback_arg);

enum
{
    GLOBUS_DSI_REST_ERROR_PARAMETER = 1,
    GLOBUS_DSI_REST_ERROR_MEMORY,
    GLOBUS_DSI_REST_ERROR_PARSE,
    GLOBUS_DSI_REST_ERROR_CURL,
    GLOBUS_DSI_REST_ERROR_JSON,
    GLOBUS_DSI_REST_ERROR_TIME_OUT,
    GLOBUS_DSI_REST_ERROR_THREAD_FAIL
};

#define GlobusDsiRestErrorParameter() \
    globus_error_put(GlobusDsiRestErrorParameterObject())
#define GlobusDsiRestErrorMemory() \
    globus_error_put(GlobusDsiRestErrorMemoryObject())
#define GlobusDsiRestErrorParse(s) \
    globus_error_put(GlobusDsiRestErrorParseObject(s))
#define GlobusDsiRestErrorCurl(rc) \
    globus_error_put(GlobusDsiRestErrorCurlObject(rc))
#define GlobusDsiRestErrorJson(err) \
    globus_error_put(GlobusDsiRestErrorJsonObject(err))
#define GlobusDsiRestErrorTimeOut() \
    globus_error_put(GlobusDsiRestErrorTimeOutObject())
#define GlobusDsiRestErrorThreadFail(rc) \
    globus_error_put(GlobusDsiRestErrorThreadFailObject(rc))

#define GlobusDsiRestErrorParameterObject() \
    globus_error_construct_error( \
        GLOBUS_DSI_REST_MODULE, \
        NULL, \
        GLOBUS_DSI_REST_ERROR_PARAMETER, \
        __FILE__, \
        __func__, \
        __LINE__, \
        "Invalid parameter")
#define GlobusDsiRestErrorMemoryObject() \
    globus_error_construct_error( \
        GLOBUS_DSI_REST_MODULE, \
        NULL, \
        GLOBUS_DSI_REST_ERROR_MEMORY, \
        __FILE__, \
        __func__, \
        __LINE__, \
        "Out of memory")
    
#define GlobusDsiRestErrorParseObject(s) \
    globus_error_construct_error( \
        GLOBUS_DSI_REST_MODULE, \
        NULL, \
        GLOBUS_DSI_REST_ERROR_PARSE, \
        __FILE__, \
        __func__, \
        __LINE__, \
        "Unable to parse %s", s)

#define GlobusDsiRestErrorCurlObject(rc) \
    globus_error_construct_error( \
        GLOBUS_DSI_REST_MODULE, \
        NULL, \
        GLOBUS_DSI_REST_ERROR_CURL, \
        __FILE__, \
        __func__, \
        __LINE__, \
        "libcurl error %d: %s", rc, curl_easy_strerror(rc))
#define GlobusDsiRestErrorJsonObject(err) \
    globus_error_construct_error( \
        GLOBUS_DSI_REST_MODULE, \
        NULL, \
        GLOBUS_DSI_REST_ERROR_CURL, \
        __FILE__, \
        __func__, \
        __LINE__, \
        "json error %s", (err)->text)
#define GlobusDsiRestErrorTimeOutObject() \
    globus_error_construct_error( \
        GLOBUS_DSI_REST_MODULE, \
        NULL, \
        GLOBUS_DSI_REST_ERROR_TIME_OUT, \
        __FILE__, \
        __func__, \
        __LINE__, \
        "Operation timed out")
#define GlobusDsiRestErrorThreadFailObject(rc) \
    globus_error_construct_error( \
        GLOBUS_DSI_REST_MODULE, \
        NULL, \
        GLOBUS_DSI_REST_ERROR_THREAD_FAIL, \
        __FILE__, \
        __func__, \
        __LINE__, \
        "Thread create failed: %d", rc)

/* Logging */
GlobusDebugDeclare(GLOBUS_DSI_REST);

enum
{
    GLOBUS_DSI_REST_TRACE             = 1<<0,
    GLOBUS_DSI_REST_INFO              = 1<<1,
    GLOBUS_DSI_REST_DEBUG             = 1<<2,
    GLOBUS_DSI_REST_WARN              = 1<<3,
    GLOBUS_DSI_REST_ERROR             = 1<<4,
    GLOBUS_DSI_REST_ALL               = 1<<6
};

extern const char * globus_i_dsi_rest_debug_level_names[];

#define GlobusDsiRestLog(level, ...) \
    do { \
        int level__ = level; \
        if (level__ > GLOBUS_DSI_REST_ERROR || level__  < 0) { \
            level__ = 1; \
        } \
        GlobusDebugPrintf(GLOBUS_DSI_REST, level__, \
            ("dsi_rest: %5s: %s: ", \
             globus_i_dsi_rest_debug_level_names[level__], __func__)); \
        GlobusDebugPrintf(GLOBUS_DSI_REST, level__, \
            (__VA_ARGS__)); \
    } while (0)

#define GlobusDsiRestTrace(...) GlobusDsiRestLog(GLOBUS_DSI_REST_TRACE, __VA_ARGS__)
#define GlobusDsiRestInfo(...) GlobusDsiRestLog(GLOBUS_DSI_REST_INFO, __VA_ARGS__)
#define GlobusDsiRestDebug(...) GlobusDsiRestLog(GLOBUS_DSI_REST_DEBUG, __VA_ARGS__)
#define GlobusDsiRestWarn(...) GlobusDsiRestLog(GLOBUS_DSI_REST_WARN, __VA_ARGS__)
#define GlobusDsiRestError(...) GlobusDsiRestLog(GLOBUS_DSI_REST_ERROR, __VA_ARGS__)

#define GlobusDsiRestEnter() GlobusDsiRestTrace("enter\n")
#define GlobusDsiRestExit()  GlobusDsiRestTrace("exit\n")
#define GlobusDsiRestExitResult(result) \
    do { \
        if (GlobusDebugTrue(GLOBUS_DSI_REST, GLOBUS_DSI_REST_TRACE)) \
        { \
            globus_object_t * obj = globus_error_peek(result); \
            char *errstr = obj?globus_error_print_friendly(obj):strdup("Success"); \
            GlobusDsiRestTrace("exit: %#x: %s\n", (unsigned) result, errstr?errstr:"UNKNOWN ERROR"); \
            free(errstr); \
        } \
    } while (0)
#define GlobusDsiRestExitSizeT(size) GlobusDsiRestTrace("exit: %zu\n", size)
#define GlobusDsiRestExitInt(rc)  GlobusDsiRestTrace("exit: %d\n", rc)
#define GlobusDsiRestExitBool(rc)  GlobusDsiRestTrace("exit: %s\n", rc?"true":"false")
#define GlobusDsiRestExitPointer(p)  GlobusDsiRestTrace("exit: %p\n", (void *)p)


#ifdef __cplusplus
}
#endif

#endif /* GLOBUS_DSI_REST_H */
#endif /* GLOBUS_DONT_DOCUMENT_INTERNAL */
