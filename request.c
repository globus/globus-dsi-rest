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
 * @file request.c GridFTP DSI REST Request
 */
#endif

#include "globus_i_dsi_rest.h"

globus_result_t
globus_dsi_rest_request(
    const char                         *method,
    const char                         *uri,
    const globus_dsi_rest_key_array_t  *query_parameters,
    const globus_dsi_rest_key_array_t  *headers,
    const globus_dsi_rest_callbacks_t  *callbacks)
{
    globus_result_t                     result = GLOBUS_SUCCESS;
    globus_i_dsi_rest_request_t        *request;

    GlobusDsiRestEnter();


    if (method == NULL || uri == NULL)
    {
        result = GlobusDsiRestErrorParameter();
        goto bad_params;
    }

    GlobusDsiRestInfo("%s %s\n", method, uri);

    request = malloc(sizeof(globus_i_dsi_rest_request_t));
    if (request == NULL)
    {
        result = GlobusDsiRestErrorMemory();
        goto request_malloc_fail;
    }

    *request = (globus_i_dsi_rest_request_t)
    {
        .method                       = method,
        .callbacks                    = *callbacks
    };

    if (callbacks->data_write_callback == globus_dsi_rest_write_block)
    {
        request->write_block_callback_arg =
                *(globus_dsi_rest_write_block_arg_t *) callbacks->data_write_callback_arg;
        request->callbacks.data_write_callback_arg = &request->write_block_callback_arg;
    }
    else if (callbacks->data_write_callback == globus_dsi_rest_write_json)
    {
        char                           *jstring;

        jstring = json_dumps(callbacks->data_write_callback_arg, JSON_COMPACT);

        if (jstring == NULL)
        {
            result = GlobusDsiRestErrorMemory();
            goto json_encode_fail;
        }
        request->write_block_callback_arg = (globus_dsi_rest_write_block_arg_t)
        {
            .block_data = jstring,
            .block_len = strlen(jstring)
        };
        request->callbacks.data_write_callback_arg = &request->write_block_callback_arg;
    }
    else if (callbacks->data_write_callback == globus_dsi_rest_write_form)
    {
        char                           *form_data;

        result = globus_i_dsi_rest_encode_form_data(
                callbacks->data_write_callback_arg,
                &form_data);
        if (result != GLOBUS_SUCCESS)
        {
            goto form_encode_fail;
        }
        request->write_block_callback_arg = (globus_dsi_rest_write_block_arg_t)
        {
            .block_data = form_data,
            .block_len = strlen(form_data)
        };
        request->callbacks.data_write_callback_arg = &request->write_block_callback_arg;
    }
    else if (callbacks->data_write_callback == globus_dsi_rest_write_gridftp_op)
    {
        globus_off_t                    start_byte = 0, byte_count = 0;

        globus_gridftp_server_get_write_range(
                callbacks->data_write_callback_arg,
                &start_byte,
                &byte_count);

        request->gridftp_op_arg = (globus_i_dsi_rest_gridftp_op_arg_t)
        {
            .op = callbacks->data_write_callback_arg,
            .pending_buffers_last = &request->gridftp_op_arg.pending_buffers,
            .offset = start_byte,
            .total = byte_count
        };
        globus_mutex_init(&request->gridftp_op_arg.mutex, NULL);
        globus_cond_init(&request->gridftp_op_arg.cond, NULL);

        request->callbacks.data_write_callback_arg = &request->gridftp_op_arg;
    }

    if (callbacks->data_read_callback == globus_dsi_rest_read_json)
    {
        request->read_json_arg.json_out = callbacks->data_read_callback_arg;
        request->callbacks.data_read_callback_arg = &request->read_json_arg;
    }
    else if (callbacks->data_read_callback == globus_dsi_rest_read_gridftp_op)
    {
        globus_off_t                    start_byte = 0, byte_count = 0;

        globus_gridftp_server_get_read_range(
                callbacks->data_read_callback_arg,
                &start_byte,
                &byte_count);

        request->gridftp_op_arg = (globus_i_dsi_rest_gridftp_op_arg_t)
        {
            .op = callbacks->data_read_callback_arg,
            .pending_buffers_last = &request->gridftp_op_arg.pending_buffers,
            .offset = start_byte,
            .total = byte_count
        };
        globus_mutex_init(&request->gridftp_op_arg.mutex, NULL);
        globus_cond_init(&request->gridftp_op_arg.cond, NULL);

        request->callbacks.data_read_callback_arg = &request->gridftp_op_arg;
    }
    if (callbacks->progress_callback == globus_dsi_rest_progress_idle_timeout)
    {
        request->idle_arg = (globus_i_dsi_rest_idle_arg_t)
        {
            .idle_timeout = (intptr_t) callbacks->progress_callback_arg
        };
        GlobusTimeAbstimeGetCurrent(request->idle_arg.last_activity);
        request->callbacks.progress_callback_arg = &request->idle_arg;
    }

    result = globus_i_dsi_rest_handle_get(&request->handle, (void *) request);
    if (result != GLOBUS_SUCCESS)
    {
        goto no_handle;
    }

    result = globus_dsi_rest_uri_add_query(
            uri, query_parameters, &request->complete_uri);

    if (result != GLOBUS_SUCCESS)
    {
        goto invalid_uri;
    }

    result = globus_i_dsi_rest_compute_headers(
            &request->request_headers,
            headers);
    if (result != GLOBUS_SUCCESS)
    {
        goto invalid_headers;
    }
    if (callbacks->data_write_callback == globus_dsi_rest_write_json)
    {
        result = globus_i_dsi_rest_add_header(
                &request->request_headers,
                "Content-Type",
                "application/json; charset=UTF=8");
        if (result != GLOBUS_SUCCESS)
        {
            goto invalid_headers;
        }
    }
    else if (callbacks->data_write_callback == globus_dsi_rest_write_form)
    {
        result = globus_i_dsi_rest_add_header(
                &request->request_headers,
                "Content-Type",
                "application/x-www-form-urlencoded");

        if (result != GLOBUS_SUCCESS)
        {
            goto invalid_headers;
        }
    }
    if (strcmp(method, "POST") == 0
        || strcmp(method, "PATCH") == 0
        || strcmp(method, "PUT") == 0
        || callbacks->data_write_callback != NULL)
    {
        for (size_t i = 0; i < (headers != NULL) ? headers->count : 0; i++)
        {
            if (strcasecmp(headers->key_value[i].key, "Content-Length") == 0)
            {
                if (sscanf(headers->key_value[i].value,
                            "%"SCNu64,
                            &request->request_content_length) != 1)
                {
                    goto invalid_headers;
                }
                request->request_content_length_set = true;
                goto skip_chunked_header;
            }
            else if(strcasecmp(headers->key_value[i].key, "Transfer-Encoding") == 0)
            {
                goto skip_chunked_header;
            }
        }
        if (callbacks->data_write_callback == globus_dsi_rest_write_json ||
            callbacks->data_write_callback == globus_dsi_rest_write_block)
        {
            request->request_content_length =
                    request->write_block_callback_arg.block_len;
            request->request_content_length_set = true;

            size_t                      slen = snprintf(
                                                NULL,
                                                0,
                                                "%"PRIu64,
                                                request->request_content_length);
            char                        h[slen+1];

            snprintf(h,
                    sizeof(h),
                    "%"PRIu64,
                    request->request_content_length);

            result = globus_i_dsi_rest_add_header(
                    &request->request_headers,
                    "Content-Length",
                    h);
        }
            result = globus_i_dsi_rest_add_header(
                    &request->request_headers,
                    "Transfer-Encoding",
                    "chunked");
        if (result != GLOBUS_SUCCESS)
        {
            goto invalid_headers;
        }
    }
skip_chunked_header:
    /* Set URI, method, headers */
    result = globus_i_dsi_rest_set_request(
            request->handle,
            method,
            request->complete_uri,
            request->request_headers,
            callbacks);
    if (result != GLOBUS_SUCCESS)
    {
        goto invalid_method;
    }

    result = globus_i_dsi_rest_perform(request);

    if (callbacks->complete_callback == NULL)
    {
        free(request);
    }

request_malloc_fail:
form_encode_fail:
json_encode_fail:
invalid_method:
invalid_headers:
invalid_uri:
no_handle:
bad_params:
    GlobusDsiRestExitResult(result);
    return result;
}
/* globus_dsi_rest_request() */
