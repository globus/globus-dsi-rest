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

struct test_case
{
    char                               *method;
    char                               *uri_pattern;
    int                                 response_code;
    char                               *upload_body1;
    char                               *upload_body2;
    char                               *download_body;
    size_t                              client_read_offset;
    size_t                              client_write_offset;
};


struct test_case                        tests[] =
{
    {
        .method = "PUT",
        .uri_pattern = "/write-block-test/put",
        .response_code = 204,
        .upload_body1 = "data1",
        .upload_body2 = "data2",
    },
};

static
globus_result_t
response_callback(
    void                               *response_callback_arg,
    int                                 response_code,
    const char                         *response_status,
    const globus_dsi_rest_key_array_t  *response_headers)
{
    struct test_case                   *test = response_callback_arg;

    if (response_code != test->response_code)
    {
        return GLOBUS_FAILURE;
    }
    return GLOBUS_SUCCESS;
}
/* response_callback() */

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
    size_t                              body1_len = strlen(test->upload_body1);
    size_t                              body2_len = strlen(test->upload_body2);
    globus_result_t                     result = GLOBUS_SUCCESS;

    if ((request_body_length != (body1_len + body2_len))
        || memcmp(request_body, test->upload_body1, body1_len) != 0
        || memcmp((char *)request_body + body1_len, test->upload_body2, body2_len) != 0)
    {
        result = GLOBUS_FAILURE;
    }

    if (test->download_body != NULL)
    {
        strcpy(response_body, test->download_body);
        *response_body_length = strlen(test->download_body);
    }

    *response_code = test->response_code;

    return result;
}

int main()
{
    globus_result_t                     result;
    globus_dsi_rest_key_array_t         headers =
    {
        .count = 1,
        .key_value = (globus_dsi_rest_key_value_t[])
        {
            {
                .key = "Content-Type",
                .value = "text/plain"
            }
        }
    };
    char                               *contact_string;
    int                                 rc = 0;

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
        bool ok = true, transport_ok = true, download_ok = true;
        char uri_fmt[] = "http://%s%s";
        size_t uri_len = strlen(contact_string) + sizeof(uri_fmt) + strlen(tests[i].uri_pattern);
        char uri[uri_len+1];
        snprintf(uri, sizeof(uri), uri_fmt, contact_string, tests[i].uri_pattern);
        result = globus_dsi_rest_request(
            tests[i].method,
            uri,
            NULL,
            &headers,
            &(globus_dsi_rest_callbacks_t)
            {
                .data_write_callback = globus_dsi_rest_write_blocks,
                .data_write_callback_arg = &(globus_dsi_rest_write_blocks_arg_t)
                {
                    .blocks = (globus_dsi_rest_write_block_arg_t[])
                    {
                        {
                            .block_data = tests[i].upload_body1,
                            .block_len = strlen(tests[i].upload_body1),
                        },
                        {
                            .block_data = tests[i].upload_body2,
                            .block_len = strlen(tests[i].upload_body2),
                        },
                    },
                    .block_count=2,
                },
                .response_callback = response_callback,
                .response_callback_arg = &tests[i]
            });


        if (result != GLOBUS_SUCCESS)
        {
            char *errstr = globus_error_print_friendly(globus_error_peek(result));
            fprintf(stderr, "Transport error: %s\n", errstr);
            ok = transport_ok = false;
        }
        if (tests[i].download_body != NULL
            && tests[i].client_read_offset != strlen(tests[i].download_body))
        {
            ok = download_ok = false;
        }

        printf("%s %zu - %s %s%s%s\n",
                ok?"ok":"not ok",
                i+1,
                tests[i].method,
                tests[i].uri_pattern,
                transport_ok?"":" transport_fail",
                download_ok?"":" download_fail");
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
