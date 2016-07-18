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

struct test_case
{
    char                               *method;
    char                               *uri;
    bool                                fail;
    bool                                retryable;
};


struct test_case                        tests[] =
{
    {
        .method = "HEAD",
        .uri = "http://invalid.globus.org/invalid",
        .fail = true,
        .retryable = true,
    },
    {
        .method = "HEAD",
        .uri = "http://localhost:6/invalid",
        .fail = true,
        .retryable = true,
    },
};

int main()
{
    globus_result_t                     result;
    int                                 rc = 0;

    globus_thread_set_model("pthread");

    curl_global_init(CURL_GLOBAL_ALL);
    globus_module_activate(GLOBUS_XIO_MODULE);

    printf("1..%zu\n", sizeof(tests)/sizeof(tests[0]));
    globus_module_activate(GLOBUS_DSI_REST_MODULE);

    for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++)
    {
        int ok = true, transfer_ok = true, retryable_ok = true;

        result = globus_dsi_rest_request(
            tests[i].method,
            tests[i].uri,
            NULL,
            NULL,
            &(globus_dsi_rest_callbacks_t)
            {
                .response_callback = NULL,
            });

        if (result != GLOBUS_SUCCESS
            && !tests[i].fail)
        {
            ok = transfer_ok = false;
        }

        if (tests[i].fail && result != GLOBUS_SUCCESS)
        {
            ok = retryable_ok = (globus_dsi_rest_error_is_retryable(result) == tests[i].retryable);
        }


        printf("%s %zu - %s%s%s\n",
                ok?"ok":"not ok",
                i+1,
                tests[i].uri,
                transfer_ok? "" : " transfer_fail",
                retryable_ok? "" : " retryable_ok_fail");
        if (!ok)
        {
            rc++;
        }
    }
    globus_module_deactivate_all();
    curl_global_cleanup();
    return rc;
}
