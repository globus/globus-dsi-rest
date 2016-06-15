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
 * @file response.c GridFTP DSI REST Response Callback
 */
#endif

#include "globus_i_dsi_rest.h"

static
globus_result_t
globus_l_dsi_rest_response(
    void                               *response_callback_arg,
    int                                 response_code,
    const char                         *response_status,
    const globus_dsi_rest_key_array_t  *response_headers)
{
    globus_dsi_rest_response_arg_t     *response_arg = response_callback_arg;
    globus_result_t                     result = GLOBUS_SUCCESS;

    GlobusDsiRestEnter();

    response_arg->response_code = response_code;

    for (size_t i = 0; i < response_arg->desired_headers.count; i++)
    {
        globus_dsi_rest_key_value_t    *desired;
        desired = &response_arg->desired_headers.key_value[i]; 
        desired->value = NULL;

        for (size_t j = 0; j < response_headers->count; j++)
        {
            globus_dsi_rest_key_value_t*response_header;
            response_header = &response_headers->key_value[j];

            if (strcasecmp(desired->key, response_header[j].key) == 0)
            {
                desired->value = strdup(response_header[j].value);
                if (desired->value == NULL)
                {
                    result = GlobusDsiRestErrorMemory();

                    goto strdup_fail;
                }
            }
        }
    }

strdup_fail:
    GlobusDsiRestExitResult(result);
    return result;
}
/* globus_l_dsi_rest_response() */

globus_dsi_rest_response_t const        globus_dsi_rest_response
                                      = globus_l_dsi_rest_response;
