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

static
globus_result_t
write_callback(
    void                               *write_callback_arg,
    void                               *buffer,
    size_t                              buffer_length,
    size_t                             *amount_copied)
{
    char                              **json_stringp = write_callback_arg;
    char                               *json_string = *json_stringp;
    size_t                              len = strlen(json_string);

    if (len > buffer_length)
    {
        len = buffer_length;
    }
    memcpy(buffer, json_string, len);
    *json_stringp = json_string + len;
    *amount_copied = len;

    return GLOBUS_SUCCESS;
}
/* write_callback() */

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
    globus_result_t                     result = GLOBUS_SUCCESS;

    memcpy(response_body, request_body, request_body_length);
    *response_body_length = request_body_length;
    headers->count = 1;
    headers->key_value = malloc(sizeof(globus_dsi_rest_key_value_t));
    if (headers->key_value == NULL)
    {
        result = GLOBUS_FAILURE;
    }
    else
    {
        headers->key_value[0].key = "Content-Type";
        headers->key_value[0].value = "application/json; charset=UTF-8";
    }
    *response_code = 200;

    return result;
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
        "{\"object\":                   {\"a\": \"b\"}}",
        "non-json-string"
    };

    globus_thread_set_model("pthread");

    curl_global_init(CURL_GLOBAL_ALL);
    globus_module_activate(GLOBUS_XIO_MODULE);


    printf("1..%zu\n", sizeof(json_tests)/sizeof(json_tests[0]));
    globus_module_activate(GLOBUS_DSI_REST_MODULE);

    result = globus_dsi_rest_test_server_init(&contact_string);

    for (size_t i = 0; i < sizeof(json_tests)/sizeof(json_tests[0]); i++)
    {
        result = globus_dsi_rest_test_server_add_route(
            "/echo",
            request_test_handler,
            &json_tests[i]);
    }

    for (size_t i = 0; i < sizeof(json_tests)/sizeof(json_tests[0]); i++)
    {
        json_t *json, *json_out;
        bool last_test = i == (sizeof(json_tests)/sizeof(json_tests[0]) - 1);
        bool ok = true, transport_ok = true, download_ok = true;
        char uri_fmt[] = "http://%s/echo";
        size_t uri_len = strlen(contact_string) + sizeof(uri_fmt);
        char uri[uri_len+1];
        snprintf(uri, sizeof(uri), uri_fmt, contact_string);

        json = json_loads(json_tests[i], 0, NULL);

        if ((!json) && !last_test)
        {
            return 99;
        }
        char *p = json_tests[i];
        result = globus_dsi_rest_request(
            "POST",
            uri,
            NULL,
            NULL,
            &(globus_dsi_rest_callbacks_t)
            {
                .data_write_callback = write_callback,
                .data_write_callback_arg = &p,
                .data_read_callback = globus_dsi_rest_read_json,
                .data_read_callback_arg = &json_out
            });

        if (last_test)
        {
            if (result == GLOBUS_SUCCESS)
            {
                ok = transport_ok = false;
            }
            else
            {
                char *errstr = globus_error_print_friendly(
                    globus_error_peek(result));
                printf("Non-json parse error: %s\n", errstr);
                free(errstr);
            }
        }
        else
        {
            if (result != GLOBUS_SUCCESS)
            {
                ok = transport_ok = false;
            }
            if (!json_equal(json, json_out))
            {
                ok = download_ok = false;
            }
        }

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
        json_decref(json_out);
    }

    free(contact_string);
    globus_dsi_rest_test_server_destroy();
    globus_module_deactivate_all();
    curl_global_cleanup();
    return rc;
}
