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
 * @file set_request.c GridFTP DSI REST Set Request
 */
#endif

#include "globus_i_dsi_rest.h"

globus_result_t
globus_i_dsi_rest_set_request(
    CURL                               *handle,
    const char                         *method,
    const char                         *uri,
    struct curl_slist                  *headers,
    const globus_dsi_rest_callbacks_t  *callbacks)
{
    globus_result_t                     result = GLOBUS_SUCCESS;
    CURLcode                            rc = CURLE_OK;

    GlobusDsiRestEnter();

    rc = curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, NULL);
    if (strcmp(method, "HEAD") == 0)
    {
        rc = curl_easy_setopt(handle, CURLOPT_NOBODY, 1L);
    }
    else if (strcmp(method, "GET") == 0)
    {
        rc = curl_easy_setopt(handle, CURLOPT_HTTPGET, 1L);
    }
    else if (strcmp(method, "PUT") == 0)
    {
        rc = curl_easy_setopt(handle, CURLOPT_UPLOAD, 1L);
    }
    else if (strcmp(method, "POST") == 0)
    {
        rc = curl_easy_setopt(handle, CURLOPT_POST, 1L);
    }
    else if (strcmp(method, "DELETE") == 0)
    {
        rc = curl_easy_setopt(handle, CURLOPT_NOBODY, 1L);
        if (rc == CURLE_OK)
        {
            rc = curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, method);
        }
    }
    else if (callbacks->data_read_callback != NULL
            && callbacks->data_write_callback != NULL)
    {
        rc = curl_easy_setopt(handle, CURLOPT_POST, 1L);
        if (rc == CURLE_OK)
        {
            rc = curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, method);
        }
    }
    else if (callbacks->data_read_callback != NULL)
    {
        rc = curl_easy_setopt(handle, CURLOPT_HTTPGET, 1L);
        if (rc == CURLE_OK)
        {
            rc = curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, method);
        }
    }
    else if (callbacks->data_write_callback != NULL)
    {
        rc = curl_easy_setopt(handle, CURLOPT_UPLOAD, 1L);
        if (rc == CURLE_OK)
        {
            rc = curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, method);
        }
    }
    else
    {
        rc = curl_easy_setopt(handle, CURLOPT_NOBODY, 1L);
        if (rc == CURLE_OK)
        {
            rc = curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, method);
        }
    }

    if (rc == CURLE_OK)
    {
        rc = curl_easy_setopt(handle, CURLOPT_URL, uri);
    }

    if (rc == CURLE_OK)
    {
        rc = curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
    }

    if (rc != CURLE_OK)
    {
        result = GlobusDsiRestErrorCurl(rc);
    }

    GlobusDsiRestExitResult(result);
    return result;
}
/* globus_i_dsi_rest_set_request() */
