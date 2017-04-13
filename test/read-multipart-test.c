/*
 * Copyright 1999-2015 University of Chicago
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
    const char                         *body =
    "--batch_1zBmD2VAgTc_AAjSFEW5ztQ\r\n"
    "Content-Type: application/json\r\n"
    "Content-ID:\r\n response-1\r\n"
    "\r\n"
    "{\r\n"
    "\"one\": 1\r\n"
    "}\r\n"
    "\r\n"
    "--batch_1zBmD2VAgTc_AAjSFEW5ztQ\r\n"
    "Content-ID: response-2\r\n"
    "Content-Type: application/json\r\n"
    "\r\n"
    "[ 1,\r2,\r\n3]\r\n"
    "\r\n"
    "--batch_1zBmD2VAgTc_AAjSFEW5ztQ--\r\n";

    strcpy(response_body, body);

    *response_body_length = strlen(response_body);
    headers->count = 1;
    headers->key_value = malloc(sizeof(globus_dsi_rest_key_value_t));
    if (headers->key_value == NULL)
    {
        result = GLOBUS_FAILURE;
    }
    else
    {
        headers->key_value[0].key = "Content-Type";
        headers->key_value[0].value = "multipart/mixed; boundary=batch_1zBmD2VAgTc_AAjSFEW5ztQ";
    }
    *response_code = 200;

    return result;
}

int main()
{
    globus_result_t                     result = GLOBUS_SUCCESS;
    char                               *contact_string = NULL;
    char                               *uri = NULL;
    json_t                             *json1 = NULL, *json2 = NULL;
    char                               *jstring1 = NULL, *jstring2 = NULL;
    globus_dsi_rest_response_arg_t      response_arg_1 = {0};
    globus_dsi_rest_response_arg_t      response_arg_2 = {0};
    int                                 rc = 0;

    globus_thread_set_model("pthread");

    curl_global_init(CURL_GLOBAL_ALL);
    globus_module_activate(GLOBUS_XIO_MODULE);


    printf("1..1\n");
    globus_module_activate(GLOBUS_DSI_REST_MODULE);

    result = globus_dsi_rest_test_server_init(&contact_string);
    if (result != GLOBUS_SUCCESS)
    {
        rc = 99;
        goto error;
    }

    result = globus_dsi_rest_test_server_add_route(
        "/multipart",
        request_test_handler,
        NULL);
    if (result != GLOBUS_SUCCESS)
    {
        rc = 99;
        goto error;
    }

    uri = globus_common_create_string("http://%s/multipart", contact_string);

    if (uri == NULL)
    {
        rc = 99;
        perror("globus_common_create_string");
        goto error;
    }

    response_arg_1.desired_headers = (globus_dsi_rest_key_array_t)
    {
        .count=1,
        .key_value = (globus_dsi_rest_key_value_t[])
        {
            {.key="Content-Id"},
        },
    };
    response_arg_2.desired_headers = (globus_dsi_rest_key_array_t)
    {
        .count=1,
        .key_value = (globus_dsi_rest_key_value_t[])
        {
            {.key="Content-Id"},
        },
    };
    result = globus_dsi_rest_request(
        "GET",
        uri,
        NULL,
        NULL,
        &(globus_dsi_rest_callbacks_t)
        {
            .data_read_callback = globus_dsi_rest_read_multipart,
            .data_read_callback_arg = &(globus_dsi_rest_read_multipart_arg_t)
            {
                .num_parts = 2,
                .parts = (struct globus_dsi_rest_read_part_s[])
                {
                    {
                        .response_callback = globus_dsi_rest_response,
                        .response_callback_arg = &response_arg_1,
                        .data_read_callback = globus_dsi_rest_read_json,
                        .data_read_callback_arg = &json1,
                    },
                    {
                        .response_callback = globus_dsi_rest_response,
                        .response_callback_arg = &response_arg_2,
                        .data_read_callback = globus_dsi_rest_read_json,
                        .data_read_callback_arg = &json2,
                    }
                }
            },
        });
    if (result != GLOBUS_SUCCESS)
    {
        goto error;
        rc = 1;
    }

    jstring1 = json_dumps(json1, JSON_COMPACT);
    if (strcmp(jstring1, "{\"one\":1}") != 0)
    {
        fprintf(stderr, "# json %s doesn't match {\"one\":1}\n", jstring1);
        goto error;
        rc = 1;
    }
    free(jstring1);
    jstring1 = NULL;
    json_decref(json1);

    if (strcmp(
        response_arg_1.desired_headers.key_value[0].value,
        "response-1") != 0)
    {
        fprintf(
            stderr, 
            "# Content-Id \"%s\" doesn't match \"%s\"\n",
            response_arg_1.desired_headers.key_value[0].value,
            "response-1");
        rc = 1;
        goto error;
    }

    free((char *) response_arg_1.desired_headers.key_value[0].value);

    jstring2 = json_dumps(json2, JSON_COMPACT);
    if (strcmp(jstring2, "[1,2,3]") != 0)
    {
        fprintf(stderr, "# json %s doesn't match [1,2,3]\n", jstring2);
        rc = 1;
        goto error;
    }
    free(jstring2);
    json_decref(json2);

    if (strcmp(
        response_arg_2.desired_headers.key_value[0].value,
        "response-2") != 0)
    {
        fprintf(
            stderr, 
            "# Content-Id \"%s\" doesn't match \"%s\"\n",
            response_arg_2.desired_headers.key_value[0].value,
            "response-2");
        rc = 1;
        goto error;
    }
    free((char *) response_arg_2.desired_headers.key_value[0].value);

    free(uri);
    free(contact_string);
    globus_dsi_rest_test_server_destroy();
    globus_module_deactivate_all();
    curl_global_cleanup();

error:
    printf("%s\n", rc == 0 ? "ok" :  "not ok");
    if (result != GLOBUS_SUCCESS)
    {
        char                           *errstr = NULL;

        errstr = globus_error_print_friendly(globus_error_peek(result));

        if (errstr != NULL)
        {
            fprintf(stderr, "Error: %s\n", errstr);
            free(errstr);
        }
    }
    return rc;
}
