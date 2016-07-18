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
 * @file read_json.c GridFTP DSI REST Read JSON Callback
 */
#endif

#include "globus_i_dsi_rest.h"

static
globus_result_t
globus_l_dsi_rest_read_json(
    void                               *read_callback_arg,
    void                               *buffer,
    size_t                              buffer_length)
{
    const size_t                        JSON_BLOCK_SIZE = 256;
    globus_i_dsi_rest_read_json_arg_t  *jdata = read_callback_arg;
    globus_result_t                     result = GLOBUS_SUCCESS;
    json_error_t                        error;

    GlobusDsiRestEnter();

    if (buffer_length > 0)
    {
        /* accumulate json data into a buffer */
        size_t                          available;
        size_t                          increase;
        void                           *resized;

        available = jdata->buffer_len - jdata->buffer_used;

        if (available < buffer_length+1)
        {
            increase = (((buffer_length+1 - available) / JSON_BLOCK_SIZE) + 1) * JSON_BLOCK_SIZE;

            resized = realloc(jdata->buffer, increase + jdata->buffer_len);

            if (resized == NULL)
            {
                result = GlobusDsiRestErrorMemory();
                goto done;
            }
            jdata->buffer = resized;
            jdata->buffer_len += increase;
        }
        memcpy(jdata->buffer + jdata->buffer_used, 
                buffer,
                buffer_length);

        jdata->buffer_used += buffer_length;
        jdata->buffer[jdata->buffer_used] = 0;
    }
    else if (jdata->buffer_used > 0) 
    {
        /* Final read, parse json */
        GlobusDsiRestDebug(jdata->buffer);
        *jdata->json_out = json_loadb(jdata->buffer, jdata->buffer_used, 0, &error);
        if (*jdata->json_out == NULL)
        {
            result = GlobusDsiRestErrorJson(&error);
        }
    }
    else
    {
        *jdata->json_out = NULL;
    }

done:
    GlobusDsiRestExitResult(result);

    return result;
}
/* globus_l_dsi_rest_read_json() */

globus_dsi_rest_read_t const            globus_dsi_rest_read_json
                                      = globus_l_dsi_rest_read_json;
