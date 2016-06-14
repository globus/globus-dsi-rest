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

#include "uri-decode.c"

globus_xio_driver_t                     http_driver;

globus_dsi_rest_key_value_t test_cases[] =
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
            unsigned char               upbuf[256];

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
                goto end_this_socket;
            }

            size_t server_read_offset = nbytes;

            do
            {
                result = globus_xio_read(
                        xio_handle,
                        upbuf+server_read_offset,
                        sizeof(upbuf)-server_read_offset,
                        1,
                        &nbytes,
                        NULL);
                if (result == GLOBUS_SUCCESS)
                {
                    server_read_offset += nbytes;
                }
            }
            while (result == GLOBUS_SUCCESS);

            bool ok=true;
            size_t j = 0;
            for (char *oldt=(char*)upbuf, *t = strpbrk((char*)upbuf, "&");
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
                if (strcmp(test_cases[j].key, oldt) != 0 || strcmp(test_cases[j].value, v) != 0)
                {
                    ok = false;
                }
                j++;
            }

            globus_xio_handle_cntl(
                    xio_handle,
                    http_driver,
                    GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_STATUS_CODE,
                    ok?"200":"400");

            result = globus_xio_write(xio_handle,
                    ok?(globus_byte_t*)"ok":(globus_byte_t*)"not ok",
                    ok?2:6,
                    ok?2:6,
                    &nbytes, NULL);

        end_this_socket:
            if (descriptor != NULL)
            {
                globus_xio_data_descriptor_destroy(descriptor);
            }
            //if (result != GLOBUS_SUCCESS)
            {
                globus_xio_close(xio_handle, NULL);
                xio_handle = NULL;
                continue;
            }
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

    globus_thread_set_model("pthread");

    curl_global_init(CURL_GLOBAL_ALL);
    globus_module_activate(GLOBUS_XIO_MODULE);


    printf("1..%zu\n", sizeof(test_cases)/sizeof(test_cases[0]));
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

    for (size_t i = 0; i < sizeof(test_cases)/sizeof(test_cases[0]); i++)
    {
        bool ok = true;
        char uri_fmt[] = "http://%s/%zu";
        size_t uri_len = snprintf(NULL, 0, uri_fmt, contact_string, i);
        char uri[uri_len+1];
        globus_dsi_rest_key_array_t     form_data =
        {
            .count = i+1,
            .key_value = test_cases
        };

        snprintf(uri, sizeof(uri), uri_fmt, contact_string, i);

        result = globus_dsi_rest_request(
            "POST",
            uri,
            NULL,
            NULL,
            &(globus_dsi_rest_callbacks_t)
            {
                .data_write_callback = globus_dsi_rest_write_form,
                .data_write_callback_arg = &form_data,
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

    curl_global_cleanup();
    return rc;
}
