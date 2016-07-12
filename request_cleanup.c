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
globus_l_dsi_rest_request_cleanup_part(
    globus_i_dsi_rest_part_t           *part);

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

    globus_l_dsi_rest_request_cleanup_part(&request->write_part);

    if (request->response_headers.key_value != NULL)
    {
        for (size_t i = 0; i < request->response_headers.count; i++)
        {
            free((char *) request->response_headers.key_value[i].key);
            free((char *) request->response_headers.key_value[i].value);
        }
        free(request->response_headers.key_value);
    }
    free(request);
}
/* globus_i_dsi_rest_request_cleanup() */

static
void
globus_l_dsi_rest_request_cleanup_part(
    globus_i_dsi_rest_part_t           *part)
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
        free(part->write_block_callback_arg_orig.block_data);
    }

    if (part->data_write_callback == globus_dsi_rest_write_gridftp_op)
    {
        globus_mutex_destroy(&part->gridftp_op_arg.mutex);
        globus_cond_destroy(&part->gridftp_op_arg.cond);
    }
    if (part->data_write_callback == globus_dsi_rest_write_multipart)
    {
        for (size_t i = 0; i < part->multipart_write_arg.num_parts; i++)
        {
            globus_l_dsi_rest_request_cleanup_part(
                    &part->multipart_write_arg.parts[i]);
        }
        free(part->multipart_write_arg.boundary);
        free(part->multipart_write_arg.current_boundary);
        free(part->multipart_write_arg.parts);
    }
}
