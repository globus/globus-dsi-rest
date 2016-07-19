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
 * @file header.c GridFTP DSI REST Header callback
 */
#endif

#include "globus_i_dsi_rest.h"

size_t
globus_i_dsi_rest_header(
    char                               *buffer,
    size_t                              size,
    size_t                              nitems,
    void                               *callback_arg)
{
    globus_result_t                     result = GLOBUS_SUCCESS;
    size_t                              total = size * nitems;
    const size_t                        HEADER_ALLOC_CHUNK_SIZE = 8;
    globus_i_dsi_rest_request_t        *request = callback_arg;
    char                               *k, *v;
    char                               *kstring, *vstring;

    GlobusDsiRestEnter();

    if (request->response_code == 0 && request->response_reason[0] == 0)
    {
        /* Actually the status line, not a header */
        char bufferstring[total + 1];
        memcpy(bufferstring, buffer, total);
        bufferstring[total] = 0;

        GlobusDsiRestInfo("%s", buffer);

        int scans = sscanf(bufferstring, "HTTP/%*s %d %64s",
                &request->response_code,
                request->response_reason);

        if (scans != 2)
        {
            result = GlobusDsiRestErrorParse(bufferstring);
        }
        goto done;
    }
    else
    {
        GlobusDsiRestDebug("%.*s", (int) (size*nitems), buffer); 
    }

    if (request->response_callback == NULL)
    {
        /* Don't bother parsing if client doesn't care */
        goto done;
    }

    if (request->response_headers.key_value == NULL
        || (request->response_headers.count > 0 &&
            request->response_headers.count % HEADER_ALLOC_CHUNK_SIZE) == 0)
    {
        size_t                          count;
        globus_dsi_rest_key_value_t    *tmp;

        count = request->response_headers.count;

        tmp = realloc(request->response_headers.key_value,
                (count  + HEADER_ALLOC_CHUNK_SIZE)
                * sizeof(globus_dsi_rest_key_value_t));
        if (tmp == NULL)
        {
            result = GlobusDsiRestErrorMemory();

            if (request->result != GLOBUS_SUCCESS)
            {
                request->result = GlobusDsiRestErrorMemory();
            }
            goto done;
        }
        request->response_headers.key_value = tmp;
    }

    if (memchr(buffer, ':', total) != NULL)
    {
        k = buffer;
        v = memchr(k, ':', total);
        if (v == NULL || k == v)
        {
            request->result = GlobusDsiRestErrorParse(buffer);
            goto done;
        }
        kstring = malloc(v-k+1);
        snprintf(kstring, v-k+1, "%s", k);

        v++;
        while (*v == ' ' || *v == '\t')
        {
            v++;
        }
        vstring = malloc(total - (v-buffer) + 1);
        snprintf(vstring, total - (v-buffer) + 1, "%s", v);
        if ((v = strchr(vstring, '\r')) != NULL)
        {
            *v = 0;
        }

        request->response_headers.key_value[request->response_headers.count++] =
        (globus_dsi_rest_key_value_t) {
            .key = kstring,
            .value = vstring
        };
    }

    if (memcmp(buffer, "\r\n", 2) == 0)
    {
        if (request->response_code == 100)
        {
            /* If we get a 100 Continue response, we'll need to reinitialize
             * the headers and response code for the real response
             */
            request->response_code = 0;
            request->response_reason[0] = 0;
            for (size_t i = 0; i < request->response_headers.count; i++)
            {
                free((char *) request->response_headers.key_value[i].key);
                free((char *) request->response_headers.key_value[i].value);
            }
            free(request->response_headers.key_value);
            request->response_headers.key_value = NULL;
            request->response_headers.count = 0;
        }
        else
        {
            globus_dsi_rest_response_arg_t
                                       *response
                                      = request->response_callback_arg;

            if (request->response_callback == globus_dsi_rest_response)
            {
                response->request_bytes_uploaded =
                        request->request_bytes_uploaded;
                response->response_bytes_downloaded =
                        request->response_bytes_downloaded;
            }
            result = request->response_callback(
                request->response_callback_arg,
                request->response_code,
                request->response_reason,
                &request->response_headers);
        }
    }

done:
    if (result != GLOBUS_SUCCESS)
    {
        total = !total;
    }
    GlobusDsiRestExitSizeT(total);
    return total;
}
/* globus_i_dsi_rest_header() */
