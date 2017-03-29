/*
 * Copyright 1999-2017 University of Chicago
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

globus_result_t
globus_i_dsi_rest_header_parse(
    globus_dsi_rest_key_array_t        *headers,
    char                               *buffer,
    size_t                              size)
{
    globus_result_t                     result = GLOBUS_SUCCESS;
    const size_t                        HEADER_ALLOC_CHUNK_SIZE = 8;
    char                               *k, *v;
    char                               *kstring, *vstring;

    GlobusDsiRestEnter();

    if (headers->key_value == NULL
        || (headers->count > 0 &&
            headers->count % HEADER_ALLOC_CHUNK_SIZE) == 0)
    {
        size_t                          count;
        globus_dsi_rest_key_value_t    *tmp;

        count = headers->count;

        tmp = realloc(headers->key_value,
                (count  + HEADER_ALLOC_CHUNK_SIZE)
                * sizeof(globus_dsi_rest_key_value_t));
        if (tmp == NULL)
        {
            result = GlobusDsiRestErrorMemory();

            goto done;
        }
        headers->key_value = tmp;
    }

    if (memchr(buffer, ':', size) != NULL)
    {
        k = buffer;
        v = memchr(k, ':', size);
        if (v == NULL || k == v)
        {
            result = GlobusDsiRestErrorParse(buffer);
            goto done;
        }
        kstring = malloc(v-k+1);
        snprintf(kstring, v-k+1, "%s", k);

        v++;
        while (*v == ' ' || *v == '\t' || *v == '\r' || *v == '\n')
        {
            v++;
        }
        vstring = malloc(size - (v-buffer) + 1);
        snprintf(vstring, size - (v-buffer) + 1, "%s", v);
        if ((v = strchr(vstring, '\r')) != NULL)
        {
            *v = 0;
        }

        headers->key_value[headers->count++] = (globus_dsi_rest_key_value_t)
        {
            .key = kstring,
            .value = vstring
        };
    }

done:
    GlobusDsiRestExitResult(result);

    return result;
}
/* globus_i_dsi_rest_header_parse() */
