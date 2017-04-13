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
 * @file request_cleanup.c GridFTP DSI REST Request Cleanup
 */
#endif

#include "globus_i_dsi_rest.h"
static
void
globus_l_dsi_rest_request_cleanup_write_part(
    globus_i_dsi_rest_write_part_t     *part);

static
void
globus_l_dsi_rest_request_cleanup_read_part(
    globus_i_dsi_rest_read_part_t      *read_part);

void
globus_i_dsi_rest_request_cleanup(
    globus_i_dsi_rest_request_t        *request)
{
    if (request == NULL)
    {
        return;
    }
    if (request->request_headers != NULL)
    {
        curl_slist_free_all(request->request_headers);
        request->request_headers = NULL;
    }
    free(request->complete_uri);
    request->complete_uri = NULL;
    globus_i_dsi_rest_handle_release(request->handle);
    request->handle = NULL;

    globus_l_dsi_rest_request_cleanup_write_part(&request->write_part);
    globus_l_dsi_rest_request_cleanup_read_part(&request->read_part);


    free(request);
}
/* globus_i_dsi_rest_request_cleanup() */

static
void
globus_l_dsi_rest_request_cleanup_write_part(
    globus_i_dsi_rest_write_part_t     *part)
{
    free(part->headers.key_value);
    for (size_t i = 0; i < part->malloced_headers.count; i++)
    {
        free((char *) part->malloced_headers.key_value[i].key);
        free((char *) part->malloced_headers.key_value[i].value);
    }
    free(part->malloced_headers.key_value);

    if (part->data_write_callback == globus_dsi_rest_write_json
        || part->data_write_callback == globus_dsi_rest_write_form)
    {
        globus_i_dsi_rest_write_block_arg_t
                                       *arg = part->data_write_callback_arg; 
        free(arg->block_data);
        arg->block_data = NULL;

        free(arg);
        part->data_write_callback_arg = NULL;
    }
    else if (part->data_write_callback == globus_dsi_rest_write_block)
    {
        globus_i_dsi_rest_write_blocks_arg_t
                                       *arg = part->data_write_callback_arg; 
        free(arg);
        part->data_write_callback_arg = NULL;
    }
    else if (part->data_write_callback == globus_dsi_rest_write_blocks)
    {
        globus_i_dsi_rest_write_blocks_arg_t
                                       *arg = part->data_write_callback_arg; 
        free(arg->blocks);
        arg->blocks = NULL;
        free(arg);
        part->data_write_callback_arg = NULL;
    }
    else if (part->data_write_callback == globus_dsi_rest_write_gridftp_op)
    {
        globus_i_dsi_rest_gridftp_op_arg_t 
                                       *arg = part->data_write_callback_arg;

        globus_mutex_destroy(&arg->mutex);
        globus_cond_destroy(&arg->cond);
        free(arg);
        part->data_write_callback_arg = NULL;
    }
    else if (part->data_write_callback == globus_dsi_rest_write_multipart)
    {
        globus_i_dsi_rest_write_multipart_arg_t
                                       *arg = part->data_write_callback_arg;

        for (size_t i = 0; i < arg->num_parts; i++)
        {
            globus_l_dsi_rest_request_cleanup_write_part(&arg->parts[i]);
        }
        free(arg->boundary);
        free(arg->current_boundary);
        free(arg->parts);
    }
}
/* globus_l_dsi_rest_request_cleanup_write_part() */

static
void
globus_l_dsi_rest_request_cleanup_read_part(
    globus_i_dsi_rest_read_part_t      *read_part)
{
    if (read_part->headers.key_value != NULL)
    {
        for (size_t i = 0; i < read_part->headers.count; i++)
        {
            free((char *) read_part->headers.key_value[i].key);
            free((char *) read_part->headers.key_value[i].value);
        }
        free(read_part->headers.key_value);
    }

    if (read_part->data_read_callback == globus_dsi_rest_read_json)
    {
        globus_i_dsi_rest_read_json_arg_t
                                       *arg = read_part->data_read_callback_arg;
        
        if (arg != NULL)
        {
            free(arg->buffer);
            arg->buffer = NULL;

            free(arg);
        }
        read_part->data_read_callback_arg = NULL;
    }
    else if (read_part->data_read_callback == globus_dsi_rest_read_gridftp_op)
    {
        globus_i_dsi_rest_gridftp_op_arg_t
                                       *arg = read_part->data_read_callback_arg;

        globus_mutex_destroy(&arg->mutex);
        globus_cond_destroy(&arg->cond);
        free(arg);
        read_part->data_read_callback_arg = NULL;
    }
    else if (read_part->data_read_callback == globus_dsi_rest_read_multipart)
    {
        globus_i_dsi_rest_read_multipart_arg_t
                                       *arg = read_part->data_read_callback_arg;

        if (arg->parts != NULL)
        {
            for (size_t i = 0; i < arg->num_parts; i++)
            {
                globus_l_dsi_rest_request_cleanup_read_part(
                        &arg->parts[i]);
            }
            free(arg->parts);
            arg->parts = NULL;
            arg->num_parts = 0;
        }

        free(arg->boundary);
        arg->boundary = NULL;

        free(arg->boundary_buffer);
        arg->boundary_buffer = NULL;

        free(arg);

        read_part->data_read_callback_arg = NULL;
    }
}
/* globus_l_dsi_rest_request_cleanup_read_part() */
