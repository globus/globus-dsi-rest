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

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file write_form.c GridFTP DSI REST Write Form Data
 */
#endif

#include "globus_i_dsi_rest.h"

static
globus_result_t
globus_l_dsi_rest_write_form(
    void                               *write_callback_arg,
    void                               *buffer,
    size_t                              buffer_length,
    size_t                             *amount_copied)
{
    globus_result_t                     result;

    GlobusDsiRestEnter();

    result = globus_dsi_rest_write_block(
            write_callback_arg, buffer, buffer_length, amount_copied);

    GlobusDsiRestExitResult(result);

    return result;
}
/* globus_l_dsi_rest_write_form() */

globus_dsi_rest_write_t const           globus_dsi_rest_write_form
                                      = globus_l_dsi_rest_write_form;
