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
 * @file uri_add_query.c GridFTP DSI REST URI Add Query
 */
#endif

#include <string.h>
#include "globus_i_dsi_rest.h"

globus_result_t
globus_dsi_rest_uri_add_query(
    const char                         *uri,
    const globus_dsi_rest_key_array_t  *query_parameters,
    char                              **complete_urip)
{
    size_t                              orig_uri_len = strlen(uri);
    size_t                              left = 0;
    size_t                              complete_uri_len = orig_uri_len;
    char                               *complete_uri = NULL, *p;
    globus_result_t                     result = GLOBUS_SUCCESS;

    GlobusDsiRestEnter();

    assert(uri != NULL);
    assert(complete_urip != NULL);

    if (query_parameters && query_parameters->count > 0)
    {
        char                            delim = '?';

        for (size_t i = 0; i < query_parameters->count; i++)
        {
            assert(query_parameters->key_value[i].key != NULL);
            assert(query_parameters->key_value[i].value != NULL);

            complete_uri_len += 2
                    + 3 * strlen(query_parameters->key_value[i].key)
                    + 3 * strlen(query_parameters->key_value[i].value);
        }
        complete_uri = malloc(complete_uri_len + 1);
        if (complete_uri == NULL)
        {
            result = GlobusDsiRestErrorMemory();

            goto memory_fail;
        }
        left = complete_uri_len + 1;
        p = complete_uri;

        strcpy(complete_uri, uri);

        left -= orig_uri_len;
        p += orig_uri_len;

        for (size_t i = 0; i < query_parameters->count; i++)
        {
            *(p++) = delim;
            assert(left > 0);
            left--;

            delim = '&';

            globus_i_dsi_rest_uri_escape(
                query_parameters->key_value[i].key,
                &p,
                &left);

            *(p++) = '=';
            assert(left > 0);
            left--;

            globus_i_dsi_rest_uri_escape(
                query_parameters->key_value[i].value,
                &p,
                &left);
        }
        *p = 0;
        assert(left > 0);
        left--;
    }
    else
    {
        complete_uri = strdup(uri);
        if (complete_uri == NULL)
        {
            result = GlobusDsiRestErrorMemory();

            goto memory_fail;
        }
    }

    GlobusDsiRestTrace("complete_uri=%s\n", complete_uri);

memory_fail:
    *complete_urip = complete_uri;

    GlobusDsiRestExitResult(result);
    return result;
}
/* globus_i_dsi_rest_uri_add_query() */
