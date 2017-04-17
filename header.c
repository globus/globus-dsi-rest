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
    globus_i_dsi_rest_request_t        *request = callback_arg;

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

    if (request->response_callback == NULL
        && request->read_part.data_read_callback
            != globus_dsi_rest_read_multipart)
    {
        /* Don't bother parsing if client doesn't care */
        goto done;
    }

    result = globus_i_dsi_rest_header_parse(
        &request->read_part.headers,
        buffer,
        total);

    if (result != GLOBUS_SUCCESS)
    {
        if (request->result == GLOBUS_SUCCESS)
        {
            request->result = result;
        }
        goto done;
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
            for (size_t i = 0; i < request->read_part.headers.count; i++)
            {
                free((char *) request->read_part.headers.key_value[i].key);
                free((char *) request->read_part.headers.key_value[i].value);
            }
            free(request->read_part.headers.key_value);
            request->read_part.headers.key_value = NULL;
            request->read_part.headers.count = 0;
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
            if (request->read_part.data_read_callback
                == globus_dsi_rest_read_multipart)
            {
                const char             *boundary = NULL;
                const char             *boundary_end = NULL;
                globus_i_dsi_rest_read_multipart_arg_t
                                       *read_multipart
                                       = request->read_part.data_read_callback_arg;

                for (size_t i = 0; i < request->read_part.headers.count; i++)
                {
                    if (strcasecmp(
                            request->read_part.headers.key_value[i].key,
                            "Content-Type") == 0
                        && strncasecmp(
                            request->read_part.headers.key_value[i].value,
                            "multipart/",
                            strlen("multipart/")) == 0)
                    {
                        boundary = strcasestr(
                            request->read_part.headers.key_value[i].value,
                            "boundary=");
                        boundary += strlen("boundary=");
                        if (*boundary == '"')
                        {
                            boundary++;
                            for (size_t b = 0; boundary[b] != 0; b++)
                            {
                                if (boundary[b] == '"')
                                {
                                    boundary_end = &boundary[b-1];
                                }
                                if (boundary[b] == '\\')
                                {
                                    b++;
                                }
                            }
                        }
                        else
                        {
                            for (size_t b = 0; boundary[b] != 0; b++)
                            {
                                if (isspace(boundary[b]))
                                {
                                    boundary_end = &boundary[b-1];
                                    break;
                                }
                            }
                        }
                    }
                }
                if (boundary != NULL)
                {
                    if (boundary_end != NULL)
                    {
                        read_multipart->boundary =
                            globus_common_create_string(
                                "%.*s",
                                (int) (boundary_end - boundary),
                                boundary);
                    }
                    else
                    {
                        read_multipart->boundary =
                            globus_common_create_string(
                                "%s", boundary);
                    }

                    read_multipart->boundary_length = strlen(
                        read_multipart->boundary);
                    /* \r\n--boundary--\r\n\0 */
                    read_multipart->boundary_buffer_length = 
                        read_multipart->boundary_length + 9;
                    read_multipart->boundary_buffer = malloc(
                        read_multipart->boundary_buffer_length);
                    read_multipart->boundary_buffer_offset = 0;
                }
            }
            if (request->response_callback != NULL)
            {
                result = request->response_callback(
                    request->response_callback_arg,
                    request->response_code,
                    request->response_reason,
                    &request->read_part.headers);
            }
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
