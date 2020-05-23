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
#include <jansson.h>

#include "globus_dsi_rest.h"
#include "test-xio-server.h"

struct json_reader_s
{
    char                                buffer[256];
    size_t                              offset;
    json_t                             *json;
};

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

globus_result_t
read_callback(
    void                               *read_callback_arg,
    void                               *buffer,
    size_t                              buffer_length)
{
    struct json_reader_s               *json_out = read_callback_arg;
    globus_result_t                     result = GLOBUS_SUCCESS;


    assert (buffer_length <= (sizeof(json_out->buffer) - json_out->offset));

    memcpy(&json_out->buffer[json_out->offset], buffer, buffer_length);
    json_out->offset += buffer_length;

    return result;
}

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
    memcpy(response_body, request_body, request_body_length);
    *response_body_length = request_body_length;
    *response_code = 200;
    headers->count = 0;

    return GLOBUS_SUCCESS;
}

int main()
{
    globus_result_t                     result;
    char                               *contact_string;
    int                                 rc = 0;
    char                               *json_tests[] =
    {
        "{\"intval\":                   42}",
        "{\"stringval\":                \"some string\"}",
        "{\"floatval\":                 42.0}",
        "{\"arrayval\":                 [\"a\", \"b\", \"c\"]}",
        "{\"object\":                   {\"a\": \"b\"}}"
    };

    globus_thread_set_model("pthread");

    curl_global_init(CURL_GLOBAL_ALL);
    globus_module_activate(GLOBUS_XIO_MODULE);


    printf("1..%zu\n", sizeof(json_tests)/sizeof(json_tests[0]));
    globus_module_activate(GLOBUS_DSI_REST_MODULE);

    result = globus_dsi_rest_test_server_init(&contact_string);

    result = globus_dsi_rest_test_server_add_route(
        "/echo",
        request_test_handler,
        NULL);

    for (size_t i = 0; i < sizeof(json_tests)/sizeof(json_tests[0]); i++)
    {
        json_t *json;
        struct json_reader_s  json_out = {.offset=0};
        bool ok = true, transport_ok = true, download_ok = true;
        char uri_fmt[] = "http://%s/echo";
        size_t uri_len = strlen(contact_string) + sizeof(uri_fmt);
        char uri[uri_len+1];
        snprintf(uri, sizeof(uri), uri_fmt, contact_string);

        json = json_loads(json_tests[i], 0, NULL);

        if (!json)
        {
            return 99;
        }
        result = globus_dsi_rest_request(
            "POST",
            uri,
            NULL,
            NULL,
            &(globus_dsi_rest_callbacks_t)
            {
                .data_write_callback = globus_dsi_rest_write_json,
                .data_write_callback_arg = json,
                .data_read_callback = read_callback,
                .data_read_callback_arg = &json_out
            });

        if (result != GLOBUS_SUCCESS)
        {
            ok = transport_ok = false;
        }
        json_out.json = json_loads(json_out.buffer, 0, NULL);

        if (!json_equal(json, json_out.json))
        {
            ok = download_ok = false;
        }
        json_decref(json);
        json_decref(json_out.json);

        printf("%s %zu - %s %s%s\n",
                ok?"ok":"not ok",
                i+1,
                json_tests[i],
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
