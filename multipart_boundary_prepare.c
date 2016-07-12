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
 * @file multipart_boundary_prepare.c Prepare a multipart boundary
 */
#endif

#include "globus_i_dsi_rest.h"

globus_result_t
globus_i_dsi_rest_multipart_boundary_prepare(
    const char                         *delimiter,
    bool                                final,
    globus_dsi_rest_key_array_t        *part_header,
    char                              **boundaryp,
    size_t                             *boundary_lengthp)
{
    globus_result_t                     result = GLOBUS_SUCCESS;
    char                               *boundary = NULL;
    char                               *p = NULL;
    size_t                              boundary_length = 0;

    GlobusDsiRestEnter();

    if (!final)
    {
        boundary_length += 8 + strlen(delimiter);
        if (part_header != NULL)
        {
            for (size_t i = 0; i < part_header->count; i++)
            {
                if (part_header->key_value[i].key != NULL
                    && part_header->key_value[i].value != NULL)
                {
                    boundary_length += strlen(part_header->key_value[i].key);
                    boundary_length += strlen(part_header->key_value[i].value);
                    boundary_length += 4;
                }
            }
        }
        p = boundary = malloc(boundary_length + 1);
        if (boundary == NULL)
        {
            result = GlobusDsiRestErrorMemory();

            goto boundary_alloc_fail;
        }
        p[0] = 0;
        p += sprintf(p, "\r\n--%s\r\n", delimiter);
        if (part_header != NULL)
        {
            for (size_t i = 0; i < part_header->count; i++)
            {
                if (part_header->key_value[i].key != NULL
                    && part_header->key_value[i].value != NULL)
                {
                    p += sprintf(p, "%s: %s\r\n",
                            part_header->key_value[i].key,
                            part_header->key_value[i].value);
                }
            }
        }
        p += sprintf(p, "\r\n");
    }
    else
    {
        boundary_length = 8 + strlen(delimiter);
        boundary = malloc(boundary_length + 1);
        if (boundary == NULL)
        {
            result = GlobusDsiRestErrorMemory();

            goto boundary_alloc_fail;
        }

        sprintf(boundary, "\r\n--%s--\r\n", delimiter);
    }

boundary_alloc_fail:
    *boundaryp = boundary;
    *boundary_lengthp = boundary_length;

    GlobusDsiRestExitResult(result);
    return result;
}
/* globus_i_dsi_rest_multipart_boundary_prepare() */
