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

globus_mutex_t server_done_mutex;
globus_cond_t server_done_cond;
bool server_stop;
bool server_done;

typedef globus_result_t (*globus_dsi_rest_route_t)(
    void *route_arg,
    void *request_body,
    size_t request_body_length,
    int *response_code,
    void *response_body,
    size_t *response_body_length,
    globus_dsi_rest_key_array_t *headers);

globus_hashtable_t server_routes;

typedef struct
{
    const char                         *uri;
    globus_dsi_rest_route_t             route;
    void                               *route_arg;
}
globus_dsi_rest_route_entry_t;

globus_xio_server_t                     xio_server;
globus_xio_stack_t                      xio_stack;
globus_xio_driver_t                     tcp_driver;
globus_xio_driver_t                     http_driver;

static
void *server_thread(void *arg);

globus_result_t
globus_dsi_rest_test_server_init(
    char                              **contact)
{
    globus_result_t                     result = GLOBUS_SUCCESS;
    globus_thread_t                     thread;
    globus_xio_attr_t                   xio_attr;

    server_done = false;
    globus_mutex_init(&server_done_mutex, NULL);
    globus_cond_init(&server_done_cond, NULL);
    result = globus_hashtable_init(
            &server_routes,
            43,
            globus_hashtable_string_hash,
            globus_hashtable_string_keyeq);
    if (result != GLOBUS_SUCCESS)
    {
        goto hashtable_init_fail;
    }

    result = globus_xio_driver_load("tcp", &tcp_driver);
    if (result != GLOBUS_SUCCESS)
    {
        goto tcp_load_fail;
    }
    result = globus_xio_driver_load("http", &http_driver);
    if (result != GLOBUS_SUCCESS)
    {
        goto http_load_fail;
    }
    result = globus_xio_stack_init(&xio_stack, NULL);
    if (result != GLOBUS_SUCCESS)
    {
        goto stack_init_fail;
    }
    result = globus_xio_stack_push_driver(xio_stack, tcp_driver);
    if (result != GLOBUS_SUCCESS)
    {
        goto stack_push_fail;
    }
    result = globus_xio_stack_push_driver(xio_stack, http_driver);
    if (result != GLOBUS_SUCCESS)
    {
        goto stack_push_fail;
    }
    result = globus_xio_attr_init(
            &xio_attr);

    result = globus_xio_attr_cntl(
            xio_attr,
            NULL,
            GLOBUS_XIO_ATTR_SET_TIMEOUT_ACCEPT,
            NULL,
            &(globus_reltime_t) { .tv_sec = 1 },
            NULL);

    result = globus_xio_server_create(&xio_server, xio_attr, xio_stack);
    if (result != GLOBUS_SUCCESS)
    {
        goto server_create_fail;
    }
    result = globus_xio_server_get_contact_string(xio_server, contact);
    if (result != GLOBUS_SUCCESS)
    {
        goto contact_string_fail;
    }

    globus_thread_create(&thread, NULL, server_thread, xio_server);

    if (result != GLOBUS_SUCCESS)
    {
contact_string_fail:
        globus_xio_server_close(xio_server);
server_create_fail:
stack_push_fail:
        globus_xio_stack_destroy(xio_stack);
stack_init_fail:
        globus_xio_driver_unload(http_driver);
http_load_fail:
        globus_xio_driver_unload(tcp_driver);
tcp_load_fail:
        globus_hashtable_destroy(&server_routes);
hashtable_init_fail:
        ;
    }
    return result;
}

void
globus_dsi_rest_test_server_destroy(void)
{
    globus_mutex_lock(&server_done_mutex);
    server_stop = true;
    while (!server_done)
    {
        globus_cond_wait(&server_done_cond, &server_done_mutex);
    }
    globus_mutex_unlock(&server_done_mutex);
    globus_cond_destroy(&server_done_cond);
    globus_mutex_destroy(&server_done_mutex);

    globus_hashtable_destroy_all(
            &server_routes,
            free);
}
    
globus_result_t
globus_dsi_rest_test_server_add_route(
    const char                         *uri,
    globus_dsi_rest_route_t             route_func,
    void                               *route_arg)
{
    globus_dsi_rest_route_entry_t      *entry = NULL;
    int                                 rc = 0;
    
    entry = malloc(sizeof(globus_dsi_rest_route_entry_t));
    if (entry == NULL)
    {
        return GLOBUS_FAILURE;
    }
    *entry = (globus_dsi_rest_route_entry_t)
    {
        .uri = uri,
        .route = route_func,
        .route_arg = route_arg,
    };

    rc = globus_hashtable_insert(&server_routes, (void *) entry->uri, entry);

    if (rc != 0)
    {
        return GLOBUS_FAILURE;
    }
    else
    {
        return GLOBUS_SUCCESS;
    }
}


static
void *server_thread(void *arg)
{
    globus_xio_server_t                 xio_server = arg;

    globus_mutex_lock(&server_done_mutex);
    while (!server_stop)
    {
        globus_xio_handle_t             xio_handle;
        globus_xio_data_descriptor_t    descriptor;
        globus_result_t                 result;
        globus_dsi_rest_route_entry_t  *route = NULL;
        
        globus_mutex_unlock(&server_done_mutex);
        result = globus_xio_server_accept(&xio_handle, xio_server);
        globus_mutex_lock(&server_done_mutex);

        if (result != GLOBUS_SUCCESS)
        {
            continue;
        }
        result = globus_xio_open(xio_handle, NULL, NULL);

        while (xio_handle != NULL)
        {
            char                       *method = NULL;
            char                       *uri = NULL;
            globus_xio_http_version_t   http_version = 0;
            globus_hashtable_t          headers = NULL;
            globus_size_t               nbytes = 0;
            unsigned char               upbuf[4096];
            unsigned char               downbuf[4096];
            size_t                      downbytes = 0;
            int                         response_code = 500;
            globus_size_t               read_total = 0;
            bool                        eof = false;
            globus_dsi_rest_key_array_t response_headers = {.count = 0};

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

            read_total = nbytes;

            if (result != GLOBUS_SUCCESS &&
                (globus_error_match(
                    globus_error_peek(result),
                    GLOBUS_XIO_MODULE,
                    GLOBUS_XIO_ERROR_EOF)
                || globus_xio_driver_error_match(
                        http_driver,
                        globus_error_peek(result),
                        GLOBUS_XIO_HTTP_ERROR_EOF)))
            {
                eof = true;
                result = GLOBUS_SUCCESS;
            }

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
            if (strcmp(method, "HEAD") == 0
                || strcmp(method, "GET") == 0
                || strcmp(method, "DELETE") == 0
                || strcmp(method, "EXEC") == 0)
            {
                eof = true;
            }
            while (!eof)
            {

                if (read_total >= sizeof(upbuf))
                {
                    result = GLOBUS_FAILURE;
                    break;
                }

                result = globus_xio_read(
                        xio_handle,
                        upbuf+read_total,
                        sizeof(upbuf)-read_total,
                        0,
                        &nbytes,
                        NULL);
                read_total += nbytes;
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
                        eof = true;
                    }
                    else
                    {
                        char *errstr = globus_error_print_friendly(globus_error_peek(result));
                        fprintf(stderr, "READ ERROR: %s\n", errstr);
                        free(errstr);
                        break;
                    }
                }
            }

            route = globus_hashtable_lookup(
                    &server_routes,
                    uri);

            if (route != NULL)
            {
                result = route->route(
                        route->route_arg,
                        upbuf, read_total,
                        &response_code,
                        downbuf, &downbytes,
                        &response_headers);
            }

            if (result != GLOBUS_SUCCESS || route == NULL)
            {
                globus_xio_handle_cntl(
                        xio_handle,
                        http_driver,
                        GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_STATUS_CODE,
                        500);

                globus_xio_handle_cntl(
                        xio_handle,
                        http_driver,
                        GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_HEADER,
                        "Connection",
                        "close");
            }
            else
            {
                globus_xio_handle_cntl(
                        xio_handle,
                        http_driver,
                        GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_STATUS_CODE,
                        response_code);
                globus_xio_handle_cntl(
                        xio_handle,
                        http_driver,
                        GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_HEADER,
                        "Connection",
                        "close");

                for (size_t i = 0; i < response_headers.count; i++)
                {
                    globus_xio_handle_cntl(
                            xio_handle,
                            http_driver,
                            GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_HEADER,
                            response_headers.key_value[i].key,
                            response_headers.key_value[i].value);

                }
                free(response_headers.key_value);

                if (downbytes != 0)
                {
                    result = globus_xio_write(xio_handle,
                            downbuf,
                            downbytes,
                            downbytes,
                            &nbytes, NULL);
                    result = globus_xio_handle_cntl(xio_handle,
                            http_driver,
                            GLOBUS_XIO_HTTP_HANDLE_SET_END_OF_ENTITY);
                }
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
    globus_xio_server_close(xio_server);

    server_done = true;
    globus_cond_signal(&server_done_cond);
    globus_mutex_unlock(&server_done_mutex);

    return 0;
}
