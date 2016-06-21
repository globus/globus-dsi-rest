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
    char                               *upload_body;
    char                               *download_body;
    char                               *location;
    size_t                              client_read_offset;
    size_t                              client_write_offset;
    size_t                              server_read_offset;
    size_t                              server_write_offset;
};


struct test_case                        tests[] =
{
    {
        .method = "PUT",
        .uri_pattern = "/write-block-test/put",
        .response_code = 204,
        .upload_body = "data",
    },
    {
        .method = "EXEC",
        .uri_pattern = "/ENDSERVER",
        .response_code = 500,
    },
};

globus_xio_driver_t                     http_driver;

static
globus_result_t
response_callback(
    void                               *response_callback_arg,
    int                                 response_code,
    const char                         *response_status,
    const globus_dsi_rest_key_array_t  *response_headers)
{
    struct test_case                   *test = response_callback_arg;
    const globus_dsi_rest_key_value_t  *header;

    if (response_code != test->response_code)
    {
        return GLOBUS_FAILURE;
    }
    if (test->location != NULL)
    {
        for (size_t i = 0; i < response_headers->count; i++)
        {
            header = &response_headers->key_value[i];

            if (strcasecmp(header->key, "location") == 0)
            {
                if (strcmp(test->location, header->value) != 0)
                {
                    return GLOBUS_FAILURE;
                }
                else
                {
                    return GLOBUS_SUCCESS;
                }
            }
        }
        return GLOBUS_FAILURE;
    }
    return GLOBUS_SUCCESS;
}
/* response_callback() */

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
                globus_xio_close(xio_handle, NULL);
                goto end_this_socket;
            }
            if (strcmp(uri, "/ENDSERVER") == 0)
            {
                end_server = true;
            }

            for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++)
            {

                if (strcmp(method, tests[i].method) == 0
                    && strcmp(tests[i].uri_pattern, uri) == 0)
                {
                    test = &tests[i];
                    break;
                }
            }

            if (test->upload_body != NULL)
            {
                size_t body_len = strlen(test->upload_body);

                if (nbytes > 0)
                {
                    test->server_read_offset = nbytes;
                }
                while (test->server_read_offset < body_len)
                {
                    result = globus_xio_read(
                            xio_handle,
                            upbuf+test->server_read_offset,
                            sizeof(upbuf)-test->server_read_offset,
                            1,
                            &nbytes,
                            NULL);
                    test->server_read_offset += nbytes;
                    if (result != GLOBUS_SUCCESS)
                    {
                        if (globus_error_match(
                                globus_error_peek(result),
                                GLOBUS_XIO_MODULE,
                                GLOBUS_XIO_ERROR_EOF)
                            || globus_xio_driver_error_match(
                                    http_driver,
                                    globus_error_peek(result),
                                    GLOBUS_XIO_HTTP_ERROR_EOF))
                        {
                            result = GLOBUS_SUCCESS;
                            break;
                        }
                        else
                        {
                            char *errstr = globus_error_print_friendly(globus_error_peek(result));
                            fprintf(stderr, "%s\n", errstr);
                            goto end_this_socket;
                        }
                    }
                }
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
            globus_xio_handle_cntl(
                    xio_handle,
                    http_driver,
                    GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_HEADER,
                    "Connection",
                    "Close");

            char *download_body = test->download_body;

            if (download_body != NULL)
            {
                result = globus_xio_write(xio_handle,
                        (unsigned char *) download_body,
                        strlen(download_body),
                        strlen(download_body),
                        &nbytes, NULL);
            }


        end_this_socket:
            if (descriptor != NULL)
            {
                globus_xio_data_descriptor_destroy(descriptor);
            }
            globus_xio_close(xio_handle, NULL);
            xio_handle = NULL;
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
                .data_write_callback = tests[i].upload_body? globus_dsi_rest_write_block : NULL,
                .data_write_callback_arg =
                        &(globus_dsi_rest_write_block_arg_t)
                        {
                            .block_data = tests[i].upload_body,
                            .block_len = tests[i].upload_body?strlen(tests[i].upload_body):0,
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

    curl_global_cleanup();
    return rc;
}
