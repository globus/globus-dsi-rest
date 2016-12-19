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

#include "globus_i_dsi_rest.h"
/**
 * @file error_is_retryable.c Check if error is transient error
 */

/**
 * @brief Check if an error is transient
 * @ingroup globus_dsi_rest
 * @details
 *     This function checks whether the error object referenced by result
 *     is a potentially transient network error. It returns true if the
 *     result does not seem to indicate a non-recoverable error.
 *
 * @param[in] result
 *     The result to check to see if the error might be handled by retrying
 *     the operation.
 */
bool
globus_dsi_rest_error_is_retryable(
    globus_result_t                     result)
{
    bool                                retryable = false;
    int                                 rc;

    GlobusDsiRestEnter();

    if (result == GLOBUS_SUCCESS)
    {
        retryable = true;
    }
    else
    {
        globus_object_t                *err = globus_error_peek(result);

        if (globus_error_match(err, GLOBUS_DSI_REST_MODULE, GLOBUS_DSI_REST_ERROR_TIME_OUT))
        {
            retryable = true;
        }
        else if (globus_error_match(err,
                    GLOBUS_DSI_REST_MODULE,
                    GLOBUS_DSI_REST_ERROR_CURL))
        {
            char *msg = globus_error_print_friendly(err);
            char *p = strstr(msg, "libcurl error ");

            if (p)
            {
                if (sscanf(p, "libcurl error %d", &rc) == 1)
                {
                    switch (rc)
                    {
                        case CURLE_COULDNT_RESOLVE_PROXY:
                        case CURLE_COULDNT_RESOLVE_HOST:
                        case CURLE_COULDNT_CONNECT:
                            retryable = true;
                            break;
                    }
                }
            }
        }
    }

    GlobusDsiRestExitBool(retryable);
    return retryable;
}
/* globus_dsi_rest_error_is_retryable() */
