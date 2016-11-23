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
 * @file handle_release.c GridFTP DSI REST Handle Destruction
 */
#endif

#include "globus_i_dsi_rest.h"

void
globus_i_dsi_rest_handle_release(
    CURL                               *handle)
{
    GlobusDsiRestEnter();

    if (handle != NULL)
    {
        curl_easy_reset(handle);
    }

    globus_mutex_lock(&globus_i_dsi_rest_handle_cache_mutex);
    if (globus_i_dsi_rest_handle_cache_index < GLOBUS_I_DSI_REST_HANDLE_CACHE_SIZE)
    {
        if (handle != NULL)
        {
            globus_i_dsi_rest_handle_cache[
                    globus_i_dsi_rest_handle_cache_index++] = handle;
            handle = NULL;
        }
    }
    globus_mutex_unlock(&globus_i_dsi_rest_handle_cache_mutex);

    if (handle != NULL)
    {
        curl_easy_cleanup(handle);
    }

    GlobusDsiRestExit();
    return;
}
/* globus_i_dsi_rest_handle_release() */
