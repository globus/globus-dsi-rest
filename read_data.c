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
 * @file read_data.c GridFTP DSI REST Read Data Callback
 */
#endif

#include "globus_i_dsi_rest.h"

size_t 
globus_i_dsi_rest_read_data(
    char                               *buffer,
    size_t                              size,
    size_t                              nitems,
    void                               *callback_arg)
{
    size_t                              processed = 0;
    globus_result_t                     result = GLOBUS_SUCCESS;
    globus_i_dsi_rest_request_t        *request = callback_arg;

    GlobusDsiRestEnter();

    if (request->write_part.data_write_callback != NULL)
    {
        result = request->write_part.data_write_callback(
                request->write_part.data_write_callback_arg,
                buffer,
                size * nitems,
                &processed);

        request->response_bytes_downloaded += processed;

        if (result == GLOBUS_SUCCESS
            && GlobusDebugTrue(GLOBUS_DSI_REST, GLOBUS_DSI_REST_TRACE))
        {
            char                           *p = buffer;
            char                           *q = malloc(processed+1);
    
            flockfile(GlobusDebugMyFile(GLOBUS_DSI_REST));
            GlobusDsiRestTrace("data: ");
            q[processed] = '\0';
    
            for (size_t i = 0; i < processed; i++)
            {
                q[i] = isprint(p[i]) ? p[i] : '.';
            }
            GlobusDebugPrintf(GLOBUS_DSI_REST, GLOBUS_DSI_REST_TRACE, ("%s\n", q));
            funlockfile(GlobusDebugMyFile(GLOBUS_DSI_REST));
            free(q);
        }
    }
    else
    {
        result = GlobusDsiRestErrorUnexpectedData(buffer, (int)size*nitems);
    }

    if (request->result == GLOBUS_SUCCESS)
    {
        request->result = result;
    }

    GlobusDsiRestExitSizeT(processed);

    return processed;
}
/* globus_i_dsi_rest_read_data() */
