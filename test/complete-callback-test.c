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
    bool                                fail;
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
        .fail = false
    },
    {
        .method = "HEAD",
        .uri_pattern = "/fail",
        .fail = true,
    },
    {
        .method = "HEAD",
        .uri_pattern = "/ENDSERVER",
        .fail = false,
    },
};

globus_mutex_t server_done_mutex;
globus_cond_t server_done_cond;
bool server_done;

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

globus_xio_driver_t                     http_driver;

static
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

            if (result != GLOBUS_SUCCESS)
            {
                goto end_this_socket;
            }

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
                goto end_this_socket;
            }
            if (strcmp(uri, "/ENDSERVER") == 0)
            {
                end_server = true;
                goto end_this_socket;
            }
            else if (strcmp(uri, "/good") == 0)
            {
                globus_xio_handle_cntl(
                        xio_handle,
                        http_driver,
                        GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_STATUS_CODE,
                        200);
            }
            else
            {
                globus_xio_handle_cntl(
                        xio_handle,
                        http_driver,
                        GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_STATUS_CODE,
                        500);
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
        }
    }
    globus_xio_server_close(xio_server);

    globus_mutex_lock(&server_done_mutex);
    server_done = true;
    globus_cond_signal(&server_done_cond);
    globus_mutex_unlock(&server_done_mutex);

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
    globus_mutex_lock(&server_done_mutex);
    while (!server_done)
    {
        globus_cond_wait(&server_done_cond, &server_done_mutex);
    }
    globus_mutex_unlock(&server_done_mutex);
    free(contact_string);
    globus_module_deactivate_all();
    curl_global_cleanup();
    return rc;
}
