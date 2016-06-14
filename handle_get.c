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
 * @file handle_get.c GridFTP DSI REST Handle Initialization
 */
#endif

#include "globus_i_dsi_rest.h"

globus_result_t
globus_i_dsi_rest_handle_get(
    CURL                              **handlep,
    void                               *callback_arg)
{
    CURL                               *curl;
    CURLcode                            rc;
    globus_result_t                     result = GLOBUS_SUCCESS;

    GlobusDsiRestEnter();

    /* TODO: Handle cache */
    curl = curl_easy_init();

    if (curl == NULL)
    {
        rc = CURLE_OUT_OF_MEMORY;
        goto curlopt_init_fail;
    }

    rc = curl_easy_setopt(curl, CURLOPT_HEADER, 0L);
    if (rc != CURLE_OK)
    {
        goto curlopt_fail;
    }
    rc = curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    if (rc != CURLE_OK)
    {
        goto curlopt_fail;
    }
    rc = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    if (rc != CURLE_OK)
    {
        goto curlopt_fail;
    }
    rc = curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    if (rc != CURLE_OK)
    {
        goto curlopt_fail;
    }
    rc = curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    if (rc != CURLE_OK)
    {
        goto curlopt_fail;
    }
#ifndef LIBCURL_NO_CURLPROTO
    rc = curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS|CURLPROTO_HTTP);
    if (rc != CURLE_OK)
    {
        goto curlopt_fail;
    }
    rc = curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS|CURLPROTO_HTTP);
    if (rc != CURLE_OK)
    {
        goto curlopt_fail;
    }
#endif
#if LIBCURL_VERSION_NUM >= 0x072000
    rc = curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, globus_i_dsi_rest_xferinfo);
    if (rc != CURLE_OK)
    {
        goto curlopt_fail;
    }
#else
    rc = curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, globus_i_dsi_rest_progress);
    if (rc != CURLE_OK)
    {
        goto curlopt_fail;
    }
#endif
    rc = curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, globus_i_dsi_rest_header);
    if (rc != CURLE_OK)
    {
        goto curlopt_fail;
    }
    rc = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, globus_i_dsi_rest_write_data);
    if (rc != CURLE_OK)
    {
        goto curlopt_fail;
    }
    rc = curl_easy_setopt(curl, CURLOPT_READFUNCTION, globus_i_dsi_rest_read_data);
    if (rc != CURLE_OK)
    {
        goto curlopt_fail;
    }
    /*
     * TODO: any benefit from using CURLOPT_SHARE for TLS session reuse with
     * REST Apis?
     */

    /* Options below are request specific, all others are generic and
     * can maybe live between curl_easy_perform()s if we cache CURL* values.
     */
#if LIBCURL_VERSION_NUM >= 0x072000
    rc = curl_easy_setopt(curl, CURLOPT_XFERINFODATA, callback_arg);
    if (rc != CURLE_OK)
    {
        goto curlopt_fail;
    }
#else
    rc = curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, callback_arg);
    if (rc != CURLE_OK)
    {
        goto curlopt_fail;
    }
#endif
    rc = curl_easy_setopt(curl, CURLOPT_HEADERDATA, callback_arg);
    if (rc != CURLE_OK)
    {
        goto curlopt_fail;
    }
    rc = curl_easy_setopt(curl, CURLOPT_WRITEDATA, callback_arg);
    if (rc != CURLE_OK)
    {
        goto curlopt_fail;
    }
    rc = curl_easy_setopt(curl, CURLOPT_READDATA, callback_arg);
    if (rc != CURLE_OK)
    {
        goto curlopt_fail;
    }

curlopt_fail:
    if (rc != CURLE_OK)
    {
        curl_easy_cleanup(curl);
        curl = NULL;
curlopt_init_fail:
        result = GlobusDsiRestErrorCurl(rc);
    }
    *handlep = curl;

    GlobusDsiRestExitResult(result);

    return result;
}
/* globus_i_dsi_rest_handle_get() */
