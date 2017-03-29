/*
 * Copyright 1999-2017 University of Chicago
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
 * @file write_block.c GridFTP DSI REST Write Single Block Callback
 */
#endif

#include "globus_i_dsi_rest.h"

static
globus_result_t
globus_l_dsi_rest_write_block(
    void                               *write_callback_arg,
    void                               *buffer,
    size_t                              buffer_length,
    size_t                             *amount_copied)
{
    globus_i_dsi_rest_write_block_arg_t*block = write_callback_arg;
    char                               *data = NULL;
    size_t                              amt = 0;
    globus_result_t                     result = GLOBUS_SUCCESS;

    GlobusDsiRestEnter();

    data = block->block_data + block->offset;
    amt = block->block_len - block->offset;

    if (amt > buffer_length)
    {
        amt = buffer_length;
    }
    memcpy(buffer, data, amt);

    *amount_copied = amt;
    block->offset += amt;

    GlobusDsiRestExitResult(result);

    return result;
}
/* globus_l_dsi_rest_write_block() */

globus_dsi_rest_write_t const           globus_dsi_rest_write_block
                                      = globus_l_dsi_rest_write_block;
