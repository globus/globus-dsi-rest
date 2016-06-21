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
    if (request->callbacks.data_write_callback == globus_dsi_rest_write_gridftp_op
        || request->callbacks.data_read_callback == globus_dsi_rest_read_gridftp_op)
    {
        globus_cond_destroy(&request->gridftp_op_arg.cond);
        globus_mutex_destroy(&request->gridftp_op_arg.mutex);
    }
    if (request->callbacks.data_write_callback == globus_dsi_rest_write_json
        || request->callbacks.data_write_callback == globus_dsi_rest_write_form)
    {
        free(request->write_block_callback_arg_orig.block_data);
        request->write_block_callback_arg_orig.block_data = NULL;
    }
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
