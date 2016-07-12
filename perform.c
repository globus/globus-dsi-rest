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
 * @file perform.c GridFTP DSI REST URI Perform REST Operation
 */
#endif

#include "globus_i_dsi_rest.h"

static
void *
globus_l_dsi_rest_perform_thread(
    void                               *thread_arg);

static
globus_result_t
globus_l_dsi_rest_perform(
    globus_i_dsi_rest_request_t        *request);

globus_result_t
globus_i_dsi_rest_perform(
    globus_i_dsi_rest_request_t        *request)
{
    int                                 rc;
    GlobusDsiRestEnter();

    if (request->complete_callback != NULL)
    {
        rc = globus_thread_create(
                &request->thread,
                NULL,
                globus_l_dsi_rest_perform_thread,
                request);
        if (rc != GLOBUS_SUCCESS)
        {
            return GlobusDsiRestErrorThreadFail(rc);
        }
        return GLOBUS_SUCCESS;
    }

    return globus_l_dsi_rest_perform(request);
}
/* globus_i_dsi_rest_perform() */

static
void *
globus_l_dsi_rest_perform_thread(
    void                               *thread_arg)
{
    globus_i_dsi_rest_request_t        *request = thread_arg;
    globus_result_t                     result;

    result = globus_l_dsi_rest_perform(request);

    request->complete_callback(
            request->complete_callback_arg,
            (result!=GLOBUS_SUCCESS) ? result : request->result);

    globus_i_dsi_rest_request_cleanup(request);

    return NULL;
}
/* globus_l_dsi_rest_perform_thread() */

static
globus_result_t
globus_l_dsi_rest_perform(
    globus_i_dsi_rest_request_t        *request)
{
    CURLcode                            rc = CURLE_OK;
    globus_result_t                     result = GLOBUS_SUCCESS;

    /* Perform request */
    rc = curl_easy_perform(request->handle);
    if (rc != CURLE_OK)
    {
        result = GlobusDsiRestErrorCurl(rc);

        goto perform_fail;
    }
    if (request->data_read_callback != NULL)
    {
        result = request->data_read_callback(
                request->data_read_callback_arg,
                "",
                0);
    }

perform_fail:
    if (request->result == GLOBUS_SUCCESS)
    {
        request->result = result;
    }
    GlobusDsiRestExitResult(result);
    return result;
}
/* globus_l_dsi_rest_perform() */
