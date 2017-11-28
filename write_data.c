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


    if (GlobusDebugTrue(GLOBUS_DSI_REST, GLOBUS_DSI_REST_DATA))
    {
        const char                     *p = ptr;
        FILE                           *fp = NULL;

        fp = GlobusDebugMyFile(GLOBUS_DSI_REST);
        flockfile(fp);

        GlobusDsiRestData("buffer=%p buffer_size=%zu\n",
            (void *) ptr,
            size*nmemb);

        GlobusDsiRestData("data: ");

        for (size_t i = 0; i < data_processed; i++)
        {
            fputc(isprint(p[i]) ? p[i] : '.', fp);
        }
        fputc('\n', fp);
        funlockfile(fp);
        fp = NULL;
    }
    request->response_bytes_downloaded += (size * nmemb);

    GlobusDsiRestDebug(
        "response_code=%d "
        "data_processed=%zu "
        "request_bytes_uploaded=%"GLOBUS_OFF_T_FORMAT" "
        "response_bytes_downloaded=%"GLOBUS_OFF_T_FORMAT"\n",
        request->response_code,
        data_processed,
        request->request_bytes_uploaded,
        request->response_bytes_downloaded);

    if (request->response_code >= 300
        && request->read_part.data_read_callback
            == globus_dsi_rest_read_gridftp_op)
    {
        globus_object_t                *error_obj = NULL;

        /* Don't write non-2xy content to gridftp data channel */
        error_obj = GlobusDsiRestErrorUnexpectedDataObject(
            ptr, (int)size*nmemb);

        globus_error_set_long_desc(
            error_obj,
            "%.*s",
            (int) (size * nmemb),
            ptr);

        result = globus_error_put(error_obj);
    }
    else if (request->read_part.data_read_callback != NULL)
    {
        result = request->read_part.data_read_callback(
                request->read_part.data_read_callback_arg,
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
