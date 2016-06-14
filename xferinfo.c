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
 * @file xferinfo.c GridFTP DSI REST XferInfo callback
 */
#endif

#include "globus_i_dsi_rest.h"

int
globus_i_dsi_rest_xferinfo(
    void                               *callback_arg,
    curl_off_t                          dltotal,
    curl_off_t                          dlnow,
    curl_off_t                          ultotal,
    curl_off_t                          ulnow)
{
    globus_i_dsi_rest_request_t        *request = callback_arg;
    globus_result_t                     result;
    int                                 rc = 0;

    GlobusDsiRestEnter();
    if (request->callbacks.progress_callback != NULL)
    {
        result = request->callbacks.progress_callback(
                request->callbacks.progress_callback_arg,
                (uint64_t) dltotal,
                (uint64_t) dlnow,
                (uint64_t) ultotal,
                (uint64_t) ulnow);

        if (result != GLOBUS_SUCCESS)
        {
            rc = CURLE_ABORTED_BY_CALLBACK;
            if (request->result != GLOBUS_SUCCESS)
            {
                request->result = result;
            }
        }
    }

    GlobusDsiRestExitInt(rc);
    return rc;
}
/* globus_i_dsi_rest_xferinfo() */
