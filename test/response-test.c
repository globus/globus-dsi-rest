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
#include "globus_xio.h"
#include "test-xio-server.h"

struct test_case
{
    char                               *method;
    char                               *uri_pattern;
    int                                 response_code;
    char                               *location;
    globus_dsi_rest_key_array_t         desired_headers;
};

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
    struct test_case                   *test = route_arg;
    globus_result_t                     result = GLOBUS_SUCCESS;

    *response_body_length = 0;
    *response_code = test->response_code;

    if (test->location)
    {
        headers->count = 1;
        headers->key_value = malloc(sizeof(globus_dsi_rest_key_value_t));
        if (headers->key_value == NULL)
        {
            result = GLOBUS_FAILURE;
        }
        else
        {
            headers->key_value[0].key = "Location";
            headers->key_value[0].value = test->location;
        }
    }
    return result;
}

int main()
{
    globus_result_t                     result;
    char                               *contact_string;
    int                                 rc = 0;
    struct test_case                    tests[] =
    {
        {
            .method = "HEAD",
            .uri_pattern = "/response-test/200-requested-but-no-location",
            .response_code = 200,
            .desired_headers = (globus_dsi_rest_key_array_t)
            {
                .count = 1,
                .key_value = (globus_dsi_rest_key_value_t[])
                {
                    { .key = "location" },
                },
            },
        },
        {
            .method = "HEAD",
            .uri_pattern = "/response-test/201-requested-location",
            .response_code = 201,
            .location = "/201-location",
            .desired_headers = (globus_dsi_rest_key_array_t)
            {
                .count = 1,
                .key_value = (globus_dsi_rest_key_value_t[])
                {
                    { .key = "location" },
                },
            },
        },
        {
            .method = "HEAD",
            .uri_pattern = "/response-test/202-requested-location-other-no-location",
            .response_code = 202,
            .desired_headers = (globus_dsi_rest_key_array_t)
            {
                .count = 2,
                .key_value = (globus_dsi_rest_key_value_t[])
                {
                    { .key = "location" },
                    { .key = "x-globus-not-present" },
                },
            },
        },
        {
            .method = "HEAD",
            .uri_pattern = "/response-test/203-requested-location-other",
            .response_code = 203,
            .location = "/203-location",
            .desired_headers = (globus_dsi_rest_key_array_t)
            {
                .count = 2,
                .key_value = (globus_dsi_rest_key_value_t[])
                {
                    { .key = "location" },
                    { .key = "x-globus-not-present" },
                },
            },
        },
        {
            .method = "HEAD",
            .uri_pattern = "/response-test/204-requested-other-location",
            .response_code = 204,
            .location = "/204-location",
            .desired_headers = (globus_dsi_rest_key_array_t)
            {
                .count = 2,
                .key_value = (globus_dsi_rest_key_value_t[])
                {
                    { .key = "x-globus-not-present" },
                    { .key = "location" },
                },
            },
        },
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
            tests[i].uri_pattern,
            request_test_handler,
            &tests[i]);
    }

    for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++)
    {
        bool ok = true, transport_ok = true, response_ok = true, headers_ok = true;
        char uri_fmt[] = "http://%s%s";
        size_t uri_len = strlen(contact_string) + sizeof(uri_fmt) + strlen(tests[i].uri_pattern);
        char uri[uri_len+1];
        globus_dsi_rest_response_arg_t  response_arg =
        {
            .desired_headers = tests[i].desired_headers
        };

        snprintf(uri, sizeof(uri), uri_fmt, contact_string, tests[i].uri_pattern);

        result = globus_dsi_rest_request(
            tests[i].method,
            uri,
            NULL,
            NULL,
            &(globus_dsi_rest_callbacks_t)
            {
                .response_callback = globus_dsi_rest_response,
                .response_callback_arg = &response_arg,
            });

        if (result != GLOBUS_SUCCESS)
        {
            char *errstr = globus_error_print_friendly(globus_error_peek(result));
            fprintf(stderr, "request result: %s\n", errstr);
            ok = transport_ok = false;
        }
        if (tests[i].response_code != response_arg.response_code)
        {
            ok = response_ok = false;
        }
        for (size_t j = 0; j < response_arg.desired_headers.count; j++)
        {
            if (strcasecmp(response_arg.desired_headers.key_value[j].key, "location") == 0)
            {
                if (tests[i].location == NULL)
                {
                    if (response_arg.desired_headers.key_value[j].value != NULL)
                    {
                        ok = headers_ok = false;
                    }
                }
                else
                {
                    if (response_arg.desired_headers.key_value[j].value == NULL
                        || strcmp(
                                response_arg.desired_headers.key_value[j].value,
                                tests[i].location) != 0)
                    {
                        ok = headers_ok = false;
                    }
                }
            }
            else
            {
                if (response_arg.desired_headers.key_value[j].value != NULL)
                {
                    ok = headers_ok = false;
                }
            }
        }

        printf("%s %zu - %s %s%s%s%s\n",
                ok?"ok":"not ok",
                i+1,
                tests[i].method,
                tests[i].uri_pattern,
                transport_ok?"":" transport_fail",
                response_ok?"":" response_fail",
                headers_ok?"":" headers_fail");
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
