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
        request->write_block_callback_arg_orig =
                *(globus_dsi_rest_write_block_arg_t *)
                callbacks->data_write_callback_arg;
        request->callbacks.data_write_callback_arg =
                &request->write_block_callback_arg;
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
        request->write_block_callback_arg =
        request->write_block_callback_arg_orig =
        (globus_dsi_rest_write_block_arg_t)
        {
            .block_data = jstring,
            .block_len = strlen(jstring)
        };
        request->callbacks.data_write_callback_arg =
                &request->write_block_callback_arg;
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
        request->write_block_callback_arg =
        request->write_block_callback_arg_orig =
        (globus_dsi_rest_write_block_arg_t)
        {
            .block_data = form_data,
            .block_len = strlen(form_data)
        };
        request->callbacks.data_write_callback_arg =
                &request->write_block_callback_arg;
    }
    else if (callbacks->data_write_callback == globus_dsi_rest_write_gridftp_op)
    {
        globus_dsi_rest_gridftp_op_arg_t
                                       *gridftp_op_arg = callbacks->data_write_callback_arg;
        int                             rc = 0;

        GlobusDsiRestDebug(
                "data_write_callback=globus_dsi_rest_write_gridftp_op"
                " gridftp_op=%p"
                " offset=%"GLOBUS_OFF_T_FORMAT
                " length=%"GLOBUS_OFF_T_FORMAT"\n",
                (void *) gridftp_op_arg->op,
                gridftp_op_arg->offset,
                gridftp_op_arg->length);

        request->gridftp_op_arg = (globus_i_dsi_rest_gridftp_op_arg_t)
        {
            .op = gridftp_op_arg->op,
            .pending_buffers_last = &request->gridftp_op_arg.pending_buffers,
            .offset = gridftp_op_arg->offset,
            .total = gridftp_op_arg->length
        };
        rc = globus_mutex_init(&request->gridftp_op_arg.mutex, NULL);
        if (rc != GLOBUS_SUCCESS)
        {
            result = GlobusDsiRestErrorThreadFail(rc);
            goto write_mutex_init_fail;
        }
        rc = globus_cond_init(&request->gridftp_op_arg.cond, NULL);
        if (rc != GLOBUS_SUCCESS)
        {
            result = GlobusDsiRestErrorThreadFail(rc);
            goto write_cond_init_fail;
        }

        request->callbacks.data_write_callback_arg = &request->gridftp_op_arg;
    }

    if (callbacks->data_read_callback == globus_dsi_rest_read_json)
    {
        request->read_json_arg.json_out = callbacks->data_read_callback_arg;
        request->callbacks.data_read_callback_arg = &request->read_json_arg;
    }
    else if (callbacks->data_read_callback == globus_dsi_rest_read_gridftp_op)
    {
        globus_dsi_rest_gridftp_op_arg_t
                                       *gridftp_op_arg = callbacks->data_read_callback_arg;
        int                             rc = 0;

        GlobusDsiRestDebug(
                "data_write_callback=globus_dsi_rest_write_gridftp_op"
                " gridftp_op=%p"
                " offset=%"GLOBUS_OFF_T_FORMAT
                " length=%"GLOBUS_OFF_T_FORMAT"\n",
                (void *) gridftp_op_arg->op,
                gridftp_op_arg->offset,
                gridftp_op_arg->length);

        request->gridftp_op_arg = (globus_i_dsi_rest_gridftp_op_arg_t)
        {
            .op = gridftp_op_arg->op,
            .pending_buffers_last = &request->gridftp_op_arg.pending_buffers,
            .offset = gridftp_op_arg->offset,
            .total = gridftp_op_arg->length,
        };
        rc = globus_mutex_init(&request->gridftp_op_arg.mutex, NULL);
        if (rc != GLOBUS_SUCCESS)
        {
            result = GlobusDsiRestErrorThreadFail(rc);
            goto read_mutex_init_fail;
        }

        rc = globus_cond_init(&request->gridftp_op_arg.cond, NULL);
        if (rc != GLOBUS_SUCCESS)
        {
            result = GlobusDsiRestErrorThreadFail(rc);
            goto read_cond_init_fail;
        }

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
                "application/json; charset=UTF-8");
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
            if (strcasecmp(headers->key_value[i].key, "Transfer-Encoding") == 0
                || strcasecmp(headers->key_value[i].key, "Content-Length") == 0)
            {
                goto skip_chunked_header;
            }
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

    if (result != GLOBUS_SUCCESS || callbacks->complete_callback == NULL)
    {
invalid_method:
invalid_headers:
invalid_uri:
no_handle:
read_cond_init_fail:
write_cond_init_fail:
read_mutex_init_fail:
write_mutex_init_fail:
form_encode_fail:
json_encode_fail:
        globus_i_dsi_rest_request_cleanup(request);
    }
request_malloc_fail:
bad_params:
    GlobusDsiRestExitResult(result);
    return result;
}
/* globus_dsi_rest_request() */
