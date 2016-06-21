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
#include "globus_xio_http.h"

struct test_case
{
    char                               *method;
    char                               *uri_pattern;
    int                                 response_code;
    char                               *location;
    globus_dsi_rest_key_array_t         desired_headers;
};
struct test_case                       *global_tests;
size_t                                  global_tests_count;


globus_xio_driver_t                     http_driver;

void *server_thread(void *arg)
{
    globus_xio_server_t                 xio_server = arg;
    bool                                end_server = false;

    while (!end_server)
    {
        globus_xio_handle_t             xio_handle;
        globus_xio_data_descriptor_t    descriptor;
        globus_result_t                 result;
        
        result = globus_xio_server_accept(&xio_handle, xio_server);

        if (result != GLOBUS_SUCCESS)
        {
            continue;
        }
        result = globus_xio_open(xio_handle, NULL, NULL);

        while (xio_handle != NULL)
        {
            char                       *method;
            char                       *uri;
            globus_xio_http_version_t   http_version;
            globus_hashtable_t          headers;
            globus_size_t               nbytes;
            unsigned char               upbuf[64];
            struct test_case            bad_test = 
            {
                .response_code = 500
            };
            struct test_case           *test = &bad_test;

            result = globus_xio_data_descriptor_init(&descriptor, xio_handle);
            if (result != GLOBUS_SUCCESS)
            {
                goto end_this_socket;
            }

            result = globus_xio_read(
                    xio_handle,
                    upbuf,
                    sizeof(upbuf),
                    0,
                    &nbytes,
                    descriptor);

            result = globus_xio_data_descriptor_cntl(
                    descriptor,
                    http_driver,
                    GLOBUS_XIO_HTTP_GET_REQUEST,
                    &method,
                    &uri,
                    &http_version,
                    &headers);

            if (result != GLOBUS_SUCCESS)
            {
                globus_xio_close(xio_handle, NULL);
                goto end_this_socket;
            }
            for (size_t i = 0; i < global_tests_count; i++)
            {

                if (strcmp(method, global_tests[i].method) == 0
                    && strcmp(global_tests[i].uri_pattern, uri) == 0)
                {
                    test = &global_tests[i];
                    break;
                }
            }
            if (strcmp(test->uri_pattern, "/ENDSERVER") == 0)
            {
                end_server = true;
            }

            globus_xio_handle_cntl(
                    xio_handle,
                    http_driver,
                    GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_STATUS_CODE,
                    test->response_code);

            if (test->location)
            {
                globus_xio_handle_cntl(
                        xio_handle,
                        http_driver,
                        GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_HEADER,
                        "Location",
                        test->location);
            }

            result = globus_xio_handle_cntl(xio_handle,
                        http_driver,
                        GLOBUS_XIO_HTTP_HANDLE_SET_END_OF_ENTITY);

        end_this_socket:
            if (descriptor != NULL)
            {
                globus_xio_data_descriptor_destroy(descriptor);
            }
            globus_xio_close(xio_handle, NULL);
            xio_handle = NULL;
            continue;
        }
    }
    return 0;
}

int main()
{
    globus_result_t                     result;
    globus_xio_server_t                 xio_server;
    globus_xio_driver_t                 tcp_driver;
    globus_xio_stack_t                  xio_stack;
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
    global_tests = tests;
    global_tests_count = sizeof(tests)/sizeof(tests[0]);

    globus_thread_set_model("pthread");

    curl_global_init(CURL_GLOBAL_ALL);
    globus_module_activate(GLOBUS_XIO_MODULE);

    printf("1..%zu\n", sizeof(tests)/sizeof(tests[0]));
    globus_module_activate(GLOBUS_DSI_REST_MODULE);

    result = globus_xio_driver_load("tcp", &tcp_driver);
    if (result != GLOBUS_SUCCESS)
    {
        return 99;
    }
    result = globus_xio_driver_load("http", &http_driver);
    if (result != GLOBUS_SUCCESS)
    {
        return 99;
    }
    result = globus_xio_stack_init(&xio_stack, NULL);
    if (result != GLOBUS_SUCCESS)
    {
        return 99;
    }
    result = globus_xio_stack_push_driver(xio_stack, tcp_driver);
    if (result != GLOBUS_SUCCESS)
    {
        return 99;
    }
    result = globus_xio_stack_push_driver(xio_stack, http_driver);
    if (result != GLOBUS_SUCCESS)
    {
        return 99;
    }
    result = globus_xio_server_create(&xio_server, NULL, xio_stack);
    if (result != GLOBUS_SUCCESS)
    {
        return 99;
    }
    result = globus_xio_server_get_contact_string(xio_server, &contact_string);
    if (result != GLOBUS_SUCCESS)
    {
        return 99;
    }

    globus_thread_t thread;
    globus_thread_create(&thread, NULL, server_thread, xio_server);

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
                    if (strcmp(
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

    curl_global_cleanup();
    return rc;
}
