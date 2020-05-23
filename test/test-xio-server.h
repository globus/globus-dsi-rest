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

typedef globus_result_t (*globus_dsi_rest_route_t)(
    void *route_arg,
    void *request_body,
    size_t request_body_length,
    int *response_code,
    void *response_body,
    size_t *response_body_length,
    globus_dsi_rest_key_array_t *headers);

globus_result_t
globus_dsi_rest_test_server_init(
    char                              **contact);

void
globus_dsi_rest_test_server_destroy(void);

globus_result_t
globus_dsi_rest_test_server_add_route(
    const char                         *uri,
    globus_dsi_rest_route_t             route_func,
    void                               *route_arg);
