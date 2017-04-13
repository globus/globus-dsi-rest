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

static
globus_result_t
globus_l_dsi_rest_prepare_write_callbacks(
    globus_i_dsi_rest_write_part_t     *current_part,
    const globus_dsi_rest_key_array_t  *headers,
    globus_dsi_rest_write_t             data_write_callback,
    void                               *data_write_callback_arg);

static
globus_result_t
globus_l_dsi_rest_prepare_write_multipart_callback(
    globus_i_dsi_rest_write_part_t     *current_part,
    globus_dsi_rest_write_multipart_arg_t
                                       *parts);

static
globus_result_t
globus_l_dsi_rest_prepare_write_json_callback(
    globus_i_dsi_rest_write_part_t     *current_part,
    json_t                             *json);

static
globus_result_t
globus_l_dsi_rest_prepare_write_form_callback(
    globus_i_dsi_rest_write_part_t     *current_part,
    globus_dsi_rest_key_array_t        *form);

static
globus_result_t
globus_l_dsi_rest_prepare_write_gridftp_op_callback(
    globus_i_dsi_rest_write_part_t     *current_part,
    globus_dsi_rest_gridftp_op_arg_t   *gridftp_op_arg);

static
globus_result_t
globus_l_dsi_rest_prepare_read_callbacks(
    globus_i_dsi_rest_read_part_t      *current_part,
    globus_dsi_rest_read_t              data_read_callback,
    void *                              data_read_callback_arg);

static
globus_result_t
globus_l_dsi_rest_prepare_read_json_callback(
    globus_i_dsi_rest_read_part_t      *current_part,
    json_t                            **jsonp);

static
globus_result_t
globus_l_dsi_rest_prepare_read_gridftp_op(
    globus_i_dsi_rest_read_part_t      *current_part,
    globus_dsi_rest_gridftp_op_arg_t   *gridftp_op_arg);

static
globus_result_t
globus_l_dsi_rest_prepare_read_multipart(
    globus_i_dsi_rest_read_part_t      *current_part,
    globus_dsi_rest_read_multipart_arg_t
                                       *multipart_arg);

static
globus_result_t
globus_l_dsi_rest_add_part_header(
    globus_i_dsi_rest_write_part_t     *current_part,
    const char                         *key,
    const char                         *value);

static
void
globus_l_dsi_rest_headers_search(
    const globus_dsi_rest_key_array_t  *headers,
    const char                         *name,
    const char                        **valuep);

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
        .response_callback            = callbacks->response_callback,
        .response_callback_arg        = callbacks->response_callback_arg,
        .complete_callback            = callbacks->complete_callback,
        .complete_callback_arg        = callbacks->complete_callback_arg,
        .progress_callback            = callbacks->progress_callback,
        .progress_callback_arg        = callbacks->progress_callback_arg,
    };

    result = globus_l_dsi_rest_prepare_write_callbacks(
            &request->write_part,
            headers ? headers : &(const globus_dsi_rest_key_array_t) {.count=0},
            callbacks->data_write_callback,
            callbacks->data_write_callback_arg);
    if (result != GLOBUS_SUCCESS)
    {
        goto prepare_write_fail;
    }

    result = globus_l_dsi_rest_prepare_read_callbacks(
        &request->read_part,
        callbacks->data_read_callback,
        callbacks->data_read_callback_arg);

    if (result != GLOBUS_SUCCESS)
    {
        goto prepare_read_fail;
    }

    if (callbacks->progress_callback == globus_dsi_rest_progress_idle_timeout)
    {
        request->idle_arg = (globus_i_dsi_rest_idle_arg_t)
        {
            .idle_timeout = (intptr_t) callbacks->progress_callback_arg
        };
        GlobusTimeAbstimeGetCurrent(request->idle_arg.last_activity);
        request->progress_callback_arg = &request->idle_arg;
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
            &request->write_part.headers);
    if (result != GLOBUS_SUCCESS)
    {
        goto invalid_headers;
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
prepare_read_fail:
prepare_write_fail:
        globus_i_dsi_rest_request_cleanup(request);
    }
request_malloc_fail:
bad_params:
    GlobusDsiRestExitResult(result);
    return result;
}
/* globus_dsi_rest_request() */

static
globus_result_t
globus_l_dsi_rest_prepare_write_callbacks(
    globus_i_dsi_rest_write_part_t     *current_part,
    const globus_dsi_rest_key_array_t  *headers,
    globus_dsi_rest_write_t             data_write_callback,
    void                               *data_write_callback_arg)
{
    globus_result_t                     result = GLOBUS_SUCCESS;

    GlobusDsiRestEnter();

    current_part->data_write_callback = data_write_callback;
    current_part->data_write_callback_arg = data_write_callback_arg;
    current_part->headers = (globus_dsi_rest_key_array_t)
    {
        .count = headers->count
    };
    if (headers->count > 0)
    {
        current_part->headers.key_value = calloc(
                headers->count, sizeof(globus_dsi_rest_key_value_t));
        if (current_part->headers.key_value == NULL)
        {
            result = GlobusDsiRestErrorMemory();

            goto headers_alloc_fail;
        }
        memcpy(current_part->headers.key_value, headers->key_value,
                headers->count * sizeof(globus_dsi_rest_key_value_t));
    }

    /*
     * For the predefined write callbacks, we use internal data structures for
     * callback arguments that we create here.
     */
    if (data_write_callback == globus_dsi_rest_write_multipart)
    {
        result = globus_l_dsi_rest_prepare_write_multipart_callback(
                current_part,
                data_write_callback_arg);
    }
    else if (data_write_callback == globus_dsi_rest_write_block)
    {
        globus_dsi_rest_write_block_arg_t
                                       *arg = data_write_callback_arg;
        globus_i_dsi_rest_write_block_arg_t
                                       *copied_arg = NULL;

        /* We pass a copy of the original arg to the callback so we can
         * track buffer offsets
         */
        copied_arg = malloc(sizeof(globus_i_dsi_rest_write_block_arg_t));
        *copied_arg = (globus_i_dsi_rest_write_block_arg_t)
        {
            .block_data = arg->block_data,
            .block_len = arg->block_len,
            .offset = 0,
        };
        current_part->data_write_callback_arg = copied_arg;
    }
    else if (data_write_callback == globus_dsi_rest_write_blocks)
    {
        globus_dsi_rest_write_blocks_arg_t
                                       *arg = data_write_callback_arg;
        globus_i_dsi_rest_write_blocks_arg_t
                                       *copied_arg = NULL;
        globus_i_dsi_rest_write_block_arg_t
                                       *block_args = NULL;

        /* We pass a copy of the original arg to the callback so we can
         * track buffer offsets
         */
        block_args = calloc(
            arg->block_count,
            sizeof(globus_i_dsi_rest_write_block_arg_t));

        for (size_t i = 0; i < arg->block_count; i++)
        {
            block_args[i] = (globus_i_dsi_rest_write_block_arg_t)
            {
                .block_data = arg->blocks[i].block_data,
                .block_len = arg->blocks[i].block_len,
                .offset = 0,
            };
        }
        copied_arg = malloc(sizeof(globus_i_dsi_rest_write_blocks_arg_t));
        *copied_arg = (globus_i_dsi_rest_write_blocks_arg_t)
        {
            .blocks = block_args,
            .block_count = arg->block_count,
            .current_block = 0,
        };
        current_part->data_write_callback_arg = copied_arg;
    }
    else if (data_write_callback == globus_dsi_rest_write_json)
    {
        result = globus_l_dsi_rest_prepare_write_json_callback(
                current_part,
                data_write_callback_arg);
    }
    else if (data_write_callback == globus_dsi_rest_write_form)
    {
        result = globus_l_dsi_rest_prepare_write_form_callback(
                current_part,
                data_write_callback_arg);
    }
    else if (data_write_callback == globus_dsi_rest_write_gridftp_op)
    {
        result = globus_l_dsi_rest_prepare_write_gridftp_op_callback(
                current_part,
                data_write_callback_arg);
    }

    if (result != GLOBUS_SUCCESS)
    {
        free(current_part->headers.key_value);
    }
headers_alloc_fail:
    GlobusDsiRestExitResult(result);

    return result;
}
/* globus_l_dsi_rest_prepare_write_callbacks() */

static
globus_result_t
globus_l_dsi_rest_prepare_write_multipart_callback(
    globus_i_dsi_rest_write_part_t     *current_part,
    globus_dsi_rest_write_multipart_arg_t
                                       *parts)
{
    globus_result_t                     result = GLOBUS_SUCCESS;
    globus_i_dsi_rest_write_multipart_arg_t  
                                       *multipart_write_arg = NULL;
    const char                         *content_type_header = NULL;

    multipart_write_arg = malloc(
        sizeof(globus_i_dsi_rest_write_multipart_arg_t));

    *multipart_write_arg = (globus_i_dsi_rest_write_multipart_arg_t)
    {
        .num_parts = parts->num_parts
    };

    current_part->data_write_callback_arg = multipart_write_arg;

    /*
     * If a Content-Type header is present, and it's a multipart MIME type
     * we'll copy the boundary from the header and use it.
     *
     * If it's present but not a multipart header, we'll concatenate the
     * parts without any special boundary handling.
     *
     * If it's not present, we'll create our own as a multipart/related
     * type and add it to the headers list.
     */
    globus_l_dsi_rest_headers_search(
            &current_part->headers,
            "Content-Type",
            &content_type_header);
    if (content_type_header != NULL)
    {
        const char                     *b = NULL, *c = NULL;

        if (strncmp(content_type_header, "multipart/", 10) == 0 &&
            (b = strstr(content_type_header, "boundary=")) != NULL)
        {
            b += 9;
            c = b;
            if (*c == '"')
            {
                b++;
                c++;
                while (*c != 0 && *c != '"')
                {
                    c++;
                }
            }
            else
            {
                while (*c != 0 && !isspace(*c))
                {
                    c++;
                }
            }
            multipart_write_arg->boundary = malloc(c-b+1);
            if (multipart_write_arg->boundary == NULL)
            {
                result = GlobusDsiRestErrorMemory();

                goto boundary_malloc_fail;
            }
            memcpy(multipart_write_arg->boundary, b, c-b);
            multipart_write_arg->boundary[c-b] = 0;
        }
    }
    else
    {
        globus_uuid_t               uuid;
        globus_uuid_create(&uuid);

        multipart_write_arg->boundary =
                globus_common_create_string("globus_%s", uuid.text);

        if (multipart_write_arg->boundary == NULL)
        {
            result = GlobusDsiRestErrorMemory();

            goto boundary_malloc_fail;
        }

        result = globus_l_dsi_rest_add_part_header(
                current_part,
                strdup("Content-Type"),
                globus_common_create_string(
                        "multipart/related; boundary=%s",
                        multipart_write_arg->boundary));
        if (result != GLOBUS_SUCCESS)
        {
            goto add_part_header_fail;
        }
    }
    /*
     * Recursively add parts to the multipart_write_arg
     */
    multipart_write_arg->parts = calloc(
            parts->num_parts, sizeof(globus_i_dsi_rest_write_part_t));

    for (size_t i = 0; i < parts->num_parts; i++)
    {
        result = globus_l_dsi_rest_prepare_write_callbacks(
                &multipart_write_arg->parts[i],
                &parts->parts[i].part_header,
                parts->parts[i].data_write_callback,
                parts->parts[i].data_write_callback_arg);
        if (result != GLOBUS_SUCCESS)
        {
            goto part_prepare_fail;
        }
    }
    /* If we have a multipart boundary string, add the initial boundary
     * and headers for the first part
     */
    if (multipart_write_arg->boundary != NULL)
    {
        result = globus_i_dsi_rest_multipart_boundary_prepare(
                multipart_write_arg->boundary,
                false,
                &multipart_write_arg->parts[0].headers,
                &multipart_write_arg->current_boundary,
                &multipart_write_arg->current_boundary_length);
    }
    if (result != GLOBUS_SUCCESS)
    {
part_prepare_fail:
add_part_header_fail:
        free(multipart_write_arg->boundary);
    }
boundary_malloc_fail:
    GlobusDsiRestExitResult(result);
    return result;
}
/* globus_l_dsi_rest_prepare_write_multipart_callback() */

static
globus_result_t
globus_l_dsi_rest_prepare_write_json_callback(
    globus_i_dsi_rest_write_part_t     *current_part,
    json_t                             *json)
{
    globus_result_t                     result = GLOBUS_SUCCESS;
    char                               *jstring = NULL;
    globus_i_dsi_rest_write_block_arg_t*block_arg = NULL;

    GlobusDsiRestEnter();
    /*
     * We dump the json to a string and then use the block write 
     * argument type, keeping the original so we can free it when we're
     * done
     */
    jstring = json_dumps(json, JSON_COMPACT);

    if (jstring == NULL)
    {
        result = GlobusDsiRestErrorMemory();
        goto json_encode_fail;
    }
    block_arg = malloc(sizeof(globus_i_dsi_rest_write_block_arg_t));
    *block_arg = (globus_i_dsi_rest_write_block_arg_t)
    {
        .block_data = jstring,
        .block_len = strlen(jstring),
        .offset = 0,
    };
    current_part->data_write_callback_arg = block_arg;
    result = globus_l_dsi_rest_add_part_header(
            current_part,
            strdup("Content-Type"),
            strdup("application/json; charset=UTF-8"));

json_encode_fail:
    GlobusDsiRestExitResult(result);
    return result;
}
/* globus_l_dsi_rest_prepare_write_json_callback() */

static
globus_result_t
globus_l_dsi_rest_prepare_write_form_callback(
    globus_i_dsi_rest_write_part_t     *current_part,
    globus_dsi_rest_key_array_t        *form)
{
    globus_result_t                     result = GLOBUS_SUCCESS;
    char                               *form_data = NULL;
    globus_i_dsi_rest_write_block_arg_t*block_arg = NULL;

    GlobusDsiRestEnter();
    /*
     * We format the form data to a string and then use the block write 
     * argument type, keeping the original so we can free it when we're
     * done
     */
    result = globus_i_dsi_rest_encode_form_data(
            form,
            &form_data);
    if (result != GLOBUS_SUCCESS)
    {
        goto form_encode_fail;
    }
    block_arg = malloc(sizeof(globus_i_dsi_rest_write_block_arg_t));

    *block_arg = (globus_i_dsi_rest_write_block_arg_t)
    {
        .block_data = form_data,
        .block_len = strlen(form_data),
        .offset = 0,
    };

    current_part->data_write_callback_arg = block_arg;

    result = globus_l_dsi_rest_add_part_header(
            current_part,
            strdup("Content-Type"),
            strdup("application/x-www-form-urlencoded"));

form_encode_fail:
    GlobusDsiRestExitResult(result);
    return result;
}
/* globus_l_dsi_rest_prepare_write_form_callback() */

static
globus_result_t
globus_l_dsi_rest_prepare_write_gridftp_op_callback(
    globus_i_dsi_rest_write_part_t     *current_part,
    globus_dsi_rest_gridftp_op_arg_t   *gridftp_op_arg)
{
    globus_result_t                     result = GLOBUS_SUCCESS;
    globus_i_dsi_rest_gridftp_op_arg_t *arg = NULL;
    int                                 rc = 0;

    GlobusDsiRestEnter();

    GlobusDsiRestDebug(
            "data_write_callback=globus_dsi_rest_write_gridftp_op"
            " gridftp_op=%p"
            " offset=%"GLOBUS_OFF_T_FORMAT
            " length=%"GLOBUS_OFF_T_FORMAT"\n",
            (void *) gridftp_op_arg->op,
            gridftp_op_arg->offset,
            gridftp_op_arg->length);

    arg = malloc(sizeof(globus_i_dsi_rest_gridftp_op_arg_t));

    *arg = (globus_i_dsi_rest_gridftp_op_arg_t)
    {
        .op = gridftp_op_arg->op,
        .pending_buffers_last = &arg->pending_buffers,
        .offset = gridftp_op_arg->offset,
        .eofp = &gridftp_op_arg->eof
    };
    if (gridftp_op_arg->length != (globus_off_t) -1)
    {
        arg->end_offset = arg->offset + gridftp_op_arg->length;
    }
    else
    {
        arg->end_offset = (globus_off_t) -1;
    }
    rc = globus_mutex_init(&arg->mutex, NULL);
    if (rc != GLOBUS_SUCCESS)
    {
        result = GlobusDsiRestErrorThreadFail(rc);
        goto write_mutex_init_fail;
    }
    rc = globus_cond_init(&arg->cond, NULL);
    if (rc != GLOBUS_SUCCESS)
    {
        result = GlobusDsiRestErrorThreadFail(rc);
        goto write_cond_init_fail;
    }

    current_part->data_write_callback = globus_dsi_rest_write_gridftp_op;
    current_part->data_write_callback_arg = arg;

    if (result != GLOBUS_SUCCESS)
    {
write_cond_init_fail:
        globus_mutex_destroy(&arg->mutex);
    }
    
write_mutex_init_fail:

    GlobusDsiRestExitResult(result);

    return result;
}
/* globus_l_dsi_rest_prepare_write_gridftp_op_callback() */

static
globus_result_t
globus_l_dsi_rest_prepare_read_callbacks(
    globus_i_dsi_rest_read_part_t      *current_part,
    globus_dsi_rest_read_t              data_read_callback,
    void *                              data_read_callback_arg)
{
    globus_result_t                     result = GLOBUS_SUCCESS;

    GlobusDsiRestEnter();

    current_part->data_read_callback = data_read_callback;
    current_part->data_read_callback_arg = data_read_callback_arg;
    current_part->headers = (globus_dsi_rest_key_array_t)
    {
        .count = 0,
    };

    /*
     * For the predefined read callbacks, we use internal data structures for
     * callback arguments that we create here.
     */
    if (data_read_callback == globus_dsi_rest_read_json)
    {
        result = globus_l_dsi_rest_prepare_read_json_callback(
            current_part,
            data_read_callback_arg);
    }
    else if (data_read_callback == globus_dsi_rest_read_gridftp_op)
    {
        result = globus_l_dsi_rest_prepare_read_gridftp_op(
            current_part,
            data_read_callback_arg);
    }
    else if (data_read_callback == globus_dsi_rest_read_multipart)
    {
        result = globus_l_dsi_rest_prepare_read_multipart(
            current_part,
            data_read_callback_arg);
    }

    if (result != GLOBUS_SUCCESS)
    {
        free(current_part->headers.key_value);
    }
    GlobusDsiRestExitResult(result);

    return result;
}

static
globus_result_t
globus_l_dsi_rest_prepare_read_json_callback(
    globus_i_dsi_rest_read_part_t      *current_part,
    json_t                            **jsonp)
{
    globus_i_dsi_rest_read_json_arg_t  *json_arg = NULL;
    globus_result_t                     result = GLOBUS_SUCCESS;

    GlobusDsiRestEnter();

    json_arg = malloc(sizeof(globus_i_dsi_rest_read_json_arg_t));

    if (json_arg == NULL)
    {
        result = GlobusDsiRestErrorMemory();

        goto malloc_json_arg_fail;
    }

    *json_arg = (globus_i_dsi_rest_read_json_arg_t)
    {
        .json_out = jsonp,
    };

malloc_json_arg_fail:
    current_part->data_read_callback_arg = json_arg;

    GlobusDsiRestExitResult(result);
    return result;
}
/* globus_l_dsi_rest_prepare_read_json_callback() */

static
globus_result_t
globus_l_dsi_rest_prepare_read_gridftp_op(
    globus_i_dsi_rest_read_part_t      *current_part,
    globus_dsi_rest_gridftp_op_arg_t   *gridftp_op_arg)
{
    int                                 rc = 0;
    globus_result_t                     result = GLOBUS_SUCCESS;
    globus_i_dsi_rest_gridftp_op_arg_t *wrapped_arg = NULL;

    GlobusDsiRestEnter();

    GlobusDsiRestDebug(
            "data_read_callback=globus_dsi_rest_read_gridftp_op"
            " gridftp_op=%p"
            " offset=%"GLOBUS_OFF_T_FORMAT
            " length=%"GLOBUS_OFF_T_FORMAT"\n",
            (void *) gridftp_op_arg->op,
            gridftp_op_arg->offset,
            gridftp_op_arg->length);
    
    wrapped_arg = malloc(sizeof(globus_i_dsi_rest_gridftp_op_arg_t));
    if (wrapped_arg == NULL)
    {
        result = GlobusDsiRestErrorMemory();

        goto malloc_arg_fail;
    }
    *wrapped_arg = (globus_i_dsi_rest_gridftp_op_arg_t)
    {
        .op = gridftp_op_arg->op,
        .pending_buffers_last = &wrapped_arg->pending_buffers,
        .offset = gridftp_op_arg->offset,
    };
    if (gridftp_op_arg->length != (globus_off_t) -1)
    {
        wrapped_arg->end_offset = wrapped_arg->offset
            + gridftp_op_arg->length;
    }
    rc = globus_mutex_init(&wrapped_arg->mutex, NULL);
    if (rc != GLOBUS_SUCCESS)
    {
        result = GlobusDsiRestErrorThreadFail(rc);
        goto mutex_init_fail;
    }
    rc = globus_cond_init(&wrapped_arg->cond, NULL);
    if (rc != GLOBUS_SUCCESS)
    {
        result = GlobusDsiRestErrorThreadFail(rc);
        goto cond_init_fail;
    }

    if (result != GLOBUS_SUCCESS)
    {
cond_init_fail:
        globus_mutex_destroy(&wrapped_arg->mutex);
mutex_init_fail:
        free(wrapped_arg);
malloc_arg_fail:
        wrapped_arg = NULL;
    }

    current_part->data_read_callback_arg = wrapped_arg;

    GlobusDsiRestExitResult(result);
    return result;
}
/* globus_l_dsi_rest_prepare_read_gridftp_op() */

static
globus_result_t
globus_l_dsi_rest_prepare_read_multipart(
    globus_i_dsi_rest_read_part_t      *current_part,
    globus_dsi_rest_read_multipart_arg_t
                                       *multipart_arg)
{
    globus_result_t                     result = GLOBUS_SUCCESS;
    globus_i_dsi_rest_read_multipart_arg_t
                                       *arg = NULL;

    GlobusDsiRestDebug(
            "data_read_callback=globus_dsi_rest_multipart"
            " num_parts=%zu\n",
            multipart_arg->num_parts);

    arg = malloc(sizeof(globus_i_dsi_rest_read_multipart_arg_t));
    if (arg == NULL)
    {
        result = GlobusDsiRestErrorMemory();

        goto arg_alloc_fail;
    }

    *arg = (globus_i_dsi_rest_read_multipart_arg_t)
    {
        .num_parts = multipart_arg->num_parts,
        .part_index = (size_t) -1,
        .parts = calloc(
            sizeof(globus_i_dsi_rest_read_part_t),
            multipart_arg->num_parts),
    };

    if (arg->parts == NULL)
    {
        result = GlobusDsiRestErrorMemory();
        goto parts_alloc_fail;
    }

    for (size_t i = 0; i < multipart_arg->num_parts; i++)
    {
        arg->parts[i].response_callback =
            multipart_arg->parts[i].response_callback;
        arg->parts[i].response_callback_arg =
            multipart_arg->parts[i].response_callback_arg;
        result = globus_l_dsi_rest_prepare_read_callbacks(
            &arg->parts[i],
            multipart_arg->parts[i].data_read_callback,
            multipart_arg->parts[i].data_read_callback_arg);
        if (result != GLOBUS_SUCCESS)
        {
            goto parts_prepare_fail;
        }
    }

parts_prepare_fail:
parts_alloc_fail:
arg_alloc_fail:
    current_part->data_read_callback_arg = arg;
    
    GlobusDsiRestExitResult(result);
    return result;
}
/* globus_l_dsi_rest_prepare_read_multipart() */

static
globus_result_t
globus_l_dsi_rest_add_part_header(
    globus_i_dsi_rest_write_part_t     *current_part,
    const char                         *key,
    const char                         *value)
{
    globus_result_t                     result = GLOBUS_SUCCESS;
    globus_dsi_rest_key_value_t        *tmp = NULL;

    GlobusDsiRestEnter();

    if (key == NULL || value == NULL)
    {
        result = GlobusDsiRestErrorMemory();

        goto bad_param;
    }
    tmp = realloc(
        current_part->malloced_headers.key_value,
        (current_part->malloced_headers.count + 1) *
        sizeof(globus_dsi_rest_key_value_t));

    if (tmp == NULL)
    {
        result = GlobusDsiRestErrorMemory();

        goto malloc_headers_fail;
    }
    current_part->malloced_headers.key_value = tmp;
    current_part->malloced_headers.key_value[current_part->malloced_headers.count++] =
    (globus_dsi_rest_key_value_t)
    {
        .key = key,
        .value = value,
    };


    tmp = realloc(
            current_part->headers.key_value,
            (current_part->headers.count+1)
            * sizeof(globus_dsi_rest_key_value_t));
    if (tmp == NULL)
    {
        goto realloc_headers_keyvalue_fail;
    }
    current_part->headers.key_value = tmp;
    current_part->headers.key_value[current_part->headers.count++] =
    current_part->malloced_headers.key_value[current_part->malloced_headers.count-1];

bad_param:
malloc_headers_fail:
realloc_headers_keyvalue_fail:
    GlobusDsiRestExitResult(result);

    return result;
}
/* globus_l_dsi_rest_add_part_header() */

static
void
globus_l_dsi_rest_headers_search(
    const globus_dsi_rest_key_array_t  *headers,
    const char                         *name,
    const char                        **valuep)
{
    const char                         *value = NULL;

    GlobusDsiRestEnter();

    for (size_t i = 0; i < headers->count; i++)
    {
        if (strcasecmp(headers->key_value[i].key, "Content-Type") == 0)
        {
            value = headers->key_value[i].value;
            break;
        }
    }
    *valuep = value;
    GlobusDsiRestExitPointer(value);
}
/* globus_l_dsi_rest_headers_search() */
