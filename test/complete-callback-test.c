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
    bool                                fail;
    int                                 response_code;
    globus_mutex_t                      mutex;
    globus_cond_t                       cond;
    globus_result_t                     result;
    bool                                done;
};


struct test_case                        tests[] =
{
    {
        .method = "HEAD",
        .uri_pattern = "/good",
        .response_code = 200,
        .fail = false
    },
    {
        .method = "HEAD",
        .uri_pattern = "/fail",
        .response_code = 500,
        .fail = true,
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
    if (response_code != 200)
    {
        return GLOBUS_FAILURE;
    }
    return GLOBUS_SUCCESS;
}
/* response_callback() */

static
void
complete_callback(
    void                               *complete_callback_arg,
    globus_result_t                     result)
{
    struct test_case                   *test = complete_callback_arg;

    globus_mutex_lock(&test->mutex);
    test->result = result;
    test->done = true;
    globus_cond_signal(&test->cond);
    globus_mutex_unlock(&test->mutex);

}
/* complete_callback() */

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

    *response_code = test->response_code;
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
        bool ok = true, register_ok = true, callback_ok = true;
        char uri_fmt[] = "http://%s%s";
        size_t uri_len = strlen(contact_string) + sizeof(uri_fmt) + strlen(tests[i].uri_pattern);
        char uri[uri_len+1];
        snprintf(uri, sizeof(uri), uri_fmt, contact_string, tests[i].uri_pattern);
        globus_mutex_init(&tests[i].mutex, NULL);
        globus_cond_init(&tests[i].cond, NULL);
        result = globus_dsi_rest_request(
            tests[i].method,
            uri,
            NULL,
            NULL,
            &(globus_dsi_rest_callbacks_t)
            {
                .response_callback = response_callback,
                .response_callback_arg = &tests[i],
                .complete_callback = complete_callback,
                .complete_callback_arg = &tests[i],
            });

        if (result != GLOBUS_SUCCESS)
        {
            ok = register_ok = false;
        }

        globus_mutex_lock(&tests[i].mutex);
        while (!tests[i].done)
        {
            globus_cond_wait(&tests[i].cond, &tests[i].mutex);
        }
        globus_mutex_unlock(&tests[i].mutex);

        globus_mutex_destroy(&tests[i].mutex);
        globus_cond_destroy(&tests[i].cond);

        if (tests[i].fail == (tests[i].result == GLOBUS_SUCCESS))
        {
            ok = callback_ok = false;
        }

        printf("%s %zu - %s%s%s\n",
                ok?"ok":"not ok",
                i+1,
                tests[i].uri_pattern,
                register_ok? "" : " register_fail",
                callback_ok? "" : " callback_fail");
        if (!ok)
        {
            rc++;
        }
    }
    globus_dsi_rest_test_server_destroy();

    free(contact_string);
    globus_module_deactivate_all();
    curl_global_cleanup();
    return rc;
}
