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
    uintptr_t                           timeout;
    bool                                fail;
};


struct test_case                        tests[] =
{
    {
        .method = "HEAD",
        .uri_pattern = "/1",
        .response_code = 200,
        .fail = false,
        .timeout = 3000
    },
    {
        .method = "HEAD",
        .uri_pattern = "/4",
        .response_code = 200,
        .fail = true,
        .timeout = 2000
    },
};


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

    sleep(atoi(test->uri_pattern + 1));

    *response_code = 200;
    *response_body_length = 0;
    headers->count = 0;

    return result;
}

int main()
{
    globus_result_t                     result;
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
        bool ok = true;
        char uri_fmt[] = "http://%s%s";
        size_t uri_len = strlen(contact_string) + sizeof(uri_fmt) + strlen(tests[i].uri_pattern);
        char uri[uri_len+1];
        snprintf(uri, sizeof(uri), uri_fmt, contact_string, tests[i].uri_pattern);
        result = globus_dsi_rest_request(
            tests[i].method,
            uri,
            NULL,
            NULL,
            &(globus_dsi_rest_callbacks_t)
            {
                .progress_callback = globus_dsi_rest_progress_idle_timeout,
                .progress_callback_arg = (void *) tests[i].timeout
            });

        if (tests[i].fail == (result == GLOBUS_SUCCESS))
        {
            ok = false;
        }

        printf("%s %zu - %"PRIuPTR"-%s\n",
                ok?"ok":"not ok",
                i+1,
                tests[i].timeout,
                tests[i].fail?"timeout":"no timeout");
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
