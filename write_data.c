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
 * @file write_data.c GridFTP DSI REST Write Data callback
 */
#endif

#include "globus_i_dsi_rest.h"

size_t
globus_i_dsi_rest_write_data(
    char                               *ptr,
    size_t                              size,
    size_t                              nmemb,
    void                               *callback_arg)
{
    globus_i_dsi_rest_request_t        *request = callback_arg;
    globus_result_t                     result;
    size_t                              data_processed = size * nmemb;

    GlobusDsiRestEnter();

    GlobusDsiRestTrace("buffer=%p buffer_size=%d\n",
            (void *) ptr,
            size*nmemb);

    if (GlobusDebugTrue(GLOBUS_DSI_REST, GLOBUS_DSI_REST_TRACE))
    {
        char                           *p = ptr;
        for (size_t i = 0; i < data_processed; i++)
        {
            GlobusDebugPrintf(
                GLOBUS_DSI_REST, GLOBUS_DSI_REST_TRACE,
                ("%c", isprint(p[i]) ? p[i] : '.'));
        }
    }

    if (request->callbacks.data_read_callback != NULL)
    {
        result = request->callbacks.data_read_callback(
                request->callbacks.data_read_callback_arg,
                ptr,
                data_processed);
    }
    else
    {
        result = GlobusDsiRestErrorUnexpectedData(ptr, (int)size*nmemb);
    }
    if (request->result == GLOBUS_SUCCESS)
    {
        request->result = result;
    }
    if (result != GLOBUS_SUCCESS)
    {
        data_processed = !data_processed;
    }

    GlobusDsiRestExitSizeT(data_processed);

    return data_processed;
}
/* globus_i_dsi_rest_write_data() */
