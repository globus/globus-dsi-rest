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

#include <stdbool.h>
#include <stdio.h>
#include <curl/curl.h>

#include "globus_dsi_rest.h"
#include "test-xio-server.h"

#include "uri-decode.c"

static
globus_result_t
response_callback(
    void                               *response_callback_arg,
    int                                 response_code,
    const char                         *response_status,
    const globus_dsi_rest_key_array_t  *response_headers)
{
    bool                               *okp = response_callback_arg;

    *okp = (response_code == 200);
    return GLOBUS_SUCCESS;
}
/* response_callback() */

static
globus_result_t
read_callback(
    void                               *read_callback_arg,
    void                               *buffer,
    size_t                              buffer_length)
{
    return GLOBUS_SUCCESS;
}
/* read_callback() */

static
globus_result_t
request_test_handler(
    void                               *route_arg,
    void                               *request_body,
    size_t                              request_body_length,
    int                                *response_code,
    void                               *response_body,
    size_t                             *response_body_length,
    globus_dsi_rest_key_array_t        *headers)
{
    globus_dsi_rest_key_array_t        *test = route_arg;
    globus_result_t                     result = GLOBUS_SUCCESS;
    char                                upbuf[request_body_length+1];
    bool ok=true;
    size_t j = 0;

    memcpy(upbuf, request_body, request_body_length);
    upbuf[request_body_length] = 0;

    for (char *oldt=upbuf, *t = strpbrk(upbuf, "&");
          oldt != NULL;
          oldt=t?t+1:NULL, t = strpbrk(oldt?oldt:"", "&"))
    {
        if (t)
        {
            *t = 0;
        }
        char *v = strpbrk(oldt, "=");
        if (v)
        {
            *(v++) = 0;
        }
        else
        {
            ok = false;
        }
        if (uri_decode(oldt) != GLOBUS_SUCCESS)
        {
            ok = false;
        }
        if (uri_decode(v) != GLOBUS_SUCCESS)
        {
            ok = false;
        }
        if (strcmp(test->key_value[j].key, oldt) != 0
            || strcmp(test->key_value[j].value, v) != 0)
        {
            ok = false;
        }
        j++;
    }

    if (ok)
    {
        strcpy(response_body, "ok");
        *response_body_length = 2;
        *response_code = 200;
        headers->count = 0;
    }
    else
    {
        strcpy(response_body, "not ok");
        *response_body_length = 6;
        *response_code = 400;
        headers->count = 0;
    }

    return result;
}

int main()
{
    globus_result_t                     result;
    char                               *contact_string;
    int                                 rc = 0;
    globus_dsi_rest_key_value_t         keyvals[] =
    {
        {
            .key = "n1",
            .value = "v1"
        },
        {
            .key = "n2",
            .value = "v2"
        },
        {
            .key = "n3",
            .value = "space in value"
        },
        {
            .key = "n4",
            .value = "nøn-áscîï"
        },
    };
    globus_dsi_rest_key_array_t         tests[] =
    {
        { .count = 1, .key_value = keyvals },
        { .count = 2, .key_value = keyvals },
        { .count = 3, .key_value = keyvals },
        { .count = 4, .key_value = keyvals },
    };
    char                               *test_uris[] =
    {
        "/1",
        "/2",
        "/3",
        "/4"
    };

    globus_thread_set_model("pthread");

    curl_global_init(CURL_GLOBAL_ALL);
    globus_module_activate(GLOBUS_XIO_MODULE);


    printf("1..%zu\n", sizeof(tests)/sizeof(tests[0]));
    globus_module_activate(GLOBUS_DSI_REST_MODULE);

    result = globus_dsi_rest_test_server_init(&contact_string);

    for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++)
    {
        result = globus_dsi_rest_test_server_add_route(
            test_uris[i],
            request_test_handler,
            &tests[i]);
    }

    for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++)
    {
        bool ok = true;
        char uri_fmt[] = "http://%s%s";
        size_t uri_len = snprintf(NULL, 0, uri_fmt, contact_string, test_uris[i]);
        char uri[uri_len+1];

        snprintf(uri, sizeof(uri), uri_fmt, contact_string, test_uris[i]);

        result = globus_dsi_rest_request(
            "POST",
            uri,
            NULL,
            NULL,
            &(globus_dsi_rest_callbacks_t)
            {
                .data_write_callback = globus_dsi_rest_write_form,
                .data_write_callback_arg = &tests[i],
                .data_read_callback = read_callback,
                .data_read_callback_arg = NULL,
                .response_callback = response_callback,
                .response_callback_arg = &ok,
            });

        if (result != GLOBUS_SUCCESS)
        {
            ok = false;
        }
        printf("%s %zu\n",
                ok?"ok":"not ok",
                i+1);
        if (!ok)
        {
            rc++;
        }
    }

    free(contact_string);
    globus_dsi_rest_test_server_destroy();
    globus_module_deactivate_all();
    curl_global_cleanup();
    return rc;
}
