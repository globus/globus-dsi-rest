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
 * @file encode_form_data.c GridFTP DSI REST Encode Form Data
 */
#endif

#include "globus_i_dsi_rest.h"

globus_result_t
globus_i_dsi_rest_encode_form_data(
    const globus_dsi_rest_key_array_t  *form_fields,
    char                              **form_datap)
{
    globus_result_t                     result = GLOBUS_SUCCESS;
    size_t                              form_len = 1;
    char                               *form_data, *p;

    GlobusDsiRestEnter();

    for (size_t i = 0; i < form_fields->count; i++)
    {
        form_len += 3 * strlen(form_fields->key_value[i].key)
                 +  3 * strlen(form_fields->key_value[i].value) 
                 +  2;
    }
    form_data = malloc(form_len);
    if (form_data == NULL)
    {
        result = GlobusDsiRestErrorMemory();

        goto done;
    }
    p = form_data;

    for (size_t i = 0; i < form_fields->count; i++)
    {
        if (i > 0)
        {
            *(p++) = '&';
            form_len--;
        }

        globus_i_dsi_rest_uri_escape(
                form_fields->key_value[i].key,
                &p,
                &form_len);
        *(p++) = '=';
        form_len--;
        globus_i_dsi_rest_uri_escape(
                form_fields->key_value[i].value,
                &p,
                &form_len);
    }
    *p = 0;

    *form_datap = form_data;
done:
    GlobusDsiRestExitResult(result);

    return result;
}
/* globus_i_dsi_rest_encode_form_data() */
