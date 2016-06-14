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
 * @file compute_headers.c GridFTP DSI REST Compute Headers
 */
#endif

#include "globus_i_dsi_rest.h"

globus_result_t
globus_i_dsi_rest_compute_headers(
    struct  curl_slist                **request_headers,
    const globus_dsi_rest_key_array_t  *headers)
{
    globus_result_t                     result = GLOBUS_SUCCESS;

    assert(request_headers != NULL);

    if (headers == NULL)
    {
        goto no_headers;
    }

    GlobusDsiRestEnter();

    for (size_t i = 0; i < headers->count; i++)
    {
        /* Allow NULL key or value for placeholders that aren't used */
        if (headers->key_value[i].key &&
            headers->key_value[i].value)
        {
            result = globus_i_dsi_rest_add_header(
                    request_headers,
                    headers->key_value[i].key,
                    headers->key_value[i].value);
            if (result != GLOBUS_SUCCESS)
            {
                break;
            }
        }
    }

    if (result != GLOBUS_SUCCESS)
    {
        curl_slist_free_all(*request_headers);
        *request_headers = NULL;
    }
no_headers:
    GlobusDsiRestExitResult(result);

    return result;
}
/* globus_i_dsi_rest_compute_headers() */
