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
 * @file buffer_get.c GridFTP DSI REST Get Buffer
 */
#endif

#include "globus_i_dsi_rest.h"

globus_i_dsi_rest_buffer_t *
globus_i_dsi_rest_buffer_get(
    globus_i_dsi_rest_gridftp_op_arg_t *gridftp_op_arg,
    size_t                              size)
{
    globus_i_dsi_rest_buffer_t         *buffer = gridftp_op_arg->current_buffer;
    GlobusDsiRestEnter();

    if (buffer == NULL)
    {
        if (gridftp_op_arg->free_buffers == NULL)
        {
            globus_i_dsi_rest_buffer_t *new_buffer;
            globus_size_t               optimal_blocksize;

            globus_gridftp_server_get_block_size(
                    gridftp_op_arg->op,
                    &optimal_blocksize);

            if (size < optimal_blocksize)
            {
                size = optimal_blocksize;
            }

            if (size > (SIZE_MAX - sizeof(globus_i_dsi_rest_buffer_t)))
            {
                size = SIZE_MAX - sizeof(globus_i_dsi_rest_buffer_t);
            }
            new_buffer = malloc(sizeof(globus_i_dsi_rest_buffer_t) + size);

            if (new_buffer == NULL)
            {
                return NULL;
            }
            new_buffer->buffer_len = size;
            new_buffer->buffer_used = 0;
            new_buffer->next = NULL;
            new_buffer->transfer_offset = UINT64_C(-1);

            gridftp_op_arg->free_buffers = new_buffer;
        }

        buffer = gridftp_op_arg->current_buffer = gridftp_op_arg->free_buffers;
        gridftp_op_arg->free_buffers = gridftp_op_arg->free_buffers->next;
        gridftp_op_arg->current_buffer->next = NULL;
    }
    GlobusDsiRestExitPointer(buffer);
    return buffer;
}
/* globus_i_dsi_rest_buffer_get() */
