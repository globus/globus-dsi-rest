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
 * @file read_gridftp_op.c GridFTP DSI REST Read GridFTP Operation Callback
 */
#endif

#include "globus_i_dsi_rest.h"
#include "globus_gridftp_server.h"

static
globus_result_t
globus_l_dsi_rest_send_pending(
    globus_i_dsi_rest_gridftp_op_arg_t *gridftp_op_arg);

static
void
globus_l_dsi_rest_gridftp_write_callback(
    globus_gfs_operation_t              op,
    globus_result_t                     result,
    globus_byte_t                      *buffer,
    globus_size_t                       nbytes,
    void                               *user_arg);

static
globus_result_t
globus_l_dsi_rest_read_gridftp_op(
    void                               *read_callback_arg,
    void                               *buffer,
    size_t                              buffer_length)
{
    globus_i_dsi_rest_gridftp_op_arg_t *gridftp_op_arg = read_callback_arg;
    globus_result_t                     result = GLOBUS_SUCCESS;
    bool                                eof = (buffer_length == 0);

    GlobusDsiRestEnter();

    globus_mutex_lock(&gridftp_op_arg->mutex);

    do
    {
        size_t                          copy_size;
        globus_i_dsi_rest_buffer_t     *current_buffer;
        int                             optimal_concurrency = 0;

        globus_gridftp_server_get_optimal_concurrency(
                gridftp_op_arg->op,
                &optimal_concurrency);

        while (gridftp_op_arg->registered_buffers_count >= optimal_concurrency)
        {
            globus_cond_wait(&gridftp_op_arg->cond, &gridftp_op_arg->mutex);
        }

        current_buffer = globus_i_dsi_rest_buffer_get(
            gridftp_op_arg,
            buffer_length);

        if (current_buffer == NULL)
        {
            gridftp_op_arg->result = GlobusDsiRestErrorMemory();

            goto out;
        }

        if (buffer_length > (
                current_buffer->buffer_len - current_buffer->buffer_used))
        {
            copy_size = current_buffer->buffer_len
                      - current_buffer->buffer_used;
        }
        else
        {
            copy_size = buffer_length;
        }

        assert(eof || copy_size > 0);

        memcpy(
            current_buffer->buffer + current_buffer->buffer_used,
            buffer,
            copy_size);

        current_buffer->buffer_used += copy_size;
        buffer = ((char *) buffer) + copy_size;
        buffer_length -= copy_size;

        if (eof || current_buffer->buffer_used == current_buffer->buffer_len)
        {
            *gridftp_op_arg->pending_buffers_last = current_buffer;
            gridftp_op_arg->pending_buffers_last = &current_buffer->next;
            gridftp_op_arg->current_buffer = NULL;
        }

        if (gridftp_op_arg->pending_buffers != NULL)
        {
            result = globus_l_dsi_rest_send_pending(gridftp_op_arg);
            if (result != GLOBUS_SUCCESS)
            {
                goto send_fail;
            }
        }
    }
    while (buffer_length > 0);

send_fail:
    if (eof)
    {
        while (gridftp_op_arg->registered_buffers_count != 0)
        {
            globus_cond_wait(
                    &gridftp_op_arg->cond,
                    &gridftp_op_arg->mutex);
        }
    }

    globus_mutex_unlock(&gridftp_op_arg->mutex);
out:
    GlobusDsiRestExitResult(result);
    return result;
}
/* globus_l_dsi_rest_read_gridftp_op() */

/**
 * @brief Send all pending buffers to the GridFTP data channel
 * @details
 *     This function sends all buffers in the pending_buffers list to the
 *     GridFTP data channel. These data in these buffers was passed from the
 *     rest server to this module in globus_dsi_rest_read_gridftp_op()
 *
 * @param gridftp_op_arg
 *     Pointer to the current state of the gridftp read. The state is 
 *     modified by this function as the buffers are registered to be written
 *     to the GridFTP data channel.
 * @return
 *     This function returns GLOBUS_SUCCESS unless attempts to write
 *     the data fail, in which case it returns an error result.
 */
static
globus_result_t
globus_l_dsi_rest_send_pending(
    globus_i_dsi_rest_gridftp_op_arg_t *gridftp_op_arg)
{
    globus_result_t                     result = GLOBUS_SUCCESS;

    GlobusDsiRestEnter();

    while (gridftp_op_arg->pending_buffers)
    {
        globus_i_dsi_rest_buffer_t     *buffer;

        buffer = gridftp_op_arg->pending_buffers;

        GlobusDsiRestDebug(
            "register_write op=%p "
            "rest_buffer=%p "
            "buffer=%p "
            "length=%"GLOBUS_OFF_T_FORMAT" "
            "offset=%"GLOBUS_OFF_T_FORMAT"\n",
            gridftp_op_arg->op,
            (void *) buffer,
            (void *) buffer->buffer,
            (globus_off_t) buffer->buffer_used,
            (globus_off_t) gridftp_op_arg->offset);
        result = globus_gridftp_server_register_write(
                gridftp_op_arg->op,
                buffer->buffer,
                buffer->buffer_used,
                gridftp_op_arg->offset,
                -1,
                globus_l_dsi_rest_gridftp_write_callback,
                gridftp_op_arg);

        if (result != GLOBUS_SUCCESS)
        {
            if (gridftp_op_arg->result == GLOBUS_SUCCESS)
            {
                gridftp_op_arg->result = result;
            }
            goto out;
        }
        gridftp_op_arg->registered_buffers_count++;
        gridftp_op_arg->offset += buffer->buffer_used;
        gridftp_op_arg->pending_buffers = buffer->next;

        if (gridftp_op_arg->pending_buffers == NULL)
        {
            gridftp_op_arg->pending_buffers_last =
                &gridftp_op_arg->pending_buffers;
        }
        buffer->next = gridftp_op_arg->registered_buffers;
        gridftp_op_arg->registered_buffers = buffer;
    }
out:
    GlobusDsiRestExitResult(result);

    return result;
}
/* globus_l_dsi_rest_send_pending() */

/**
 * @brief GridFTP write complete callback
 * @details
 *     This function is called by the GridFTP server when it has completed
 *     processing a data buffer passed to
 *     globus_gridftp_server_register_write(). Since this is called as a
 *     Globus callback, the GridFTP state mutex must be locked during this
 *     function. If this callback is for the last outstanding write, then
 *     the condition variable for the state is signalled.
 *
 * @param[in] op
 *     The GridFTP operation related to the write.
 * @param[in] result
 *     The result of the write.
 * @param[in] buffer
 *     The data array written.
 * @param[in] nbytes
 *     The amount of data written.
 * @param[inout] user_arg
 *     Pointer to the current state of the gridftp read.
 *
 * @return void
 */
static
void
globus_l_dsi_rest_gridftp_write_callback(
    globus_gfs_operation_t              op,
    globus_result_t                     result,
    globus_byte_t                      *buffer,
    globus_size_t                       nbytes,
    void                               *user_arg)
{
    globus_i_dsi_rest_gridftp_op_arg_t *gridftp_op_arg = user_arg;
    globus_i_dsi_rest_buffer_t         *ptr, **last_next;

    GlobusDsiRestEnter();

    GlobusDsiRestDebug(
        "write_callback op=%p "
        "buffer=%p "
        "length=%"GLOBUS_OFF_T_FORMAT"\n",
        gridftp_op_arg->op,
        (void *) buffer,
        (globus_off_t) nbytes);

    globus_mutex_lock(&gridftp_op_arg->mutex);

    ptr = gridftp_op_arg->registered_buffers;
    last_next = &gridftp_op_arg->registered_buffers;

    while (ptr != NULL)
    {
        if (ptr->buffer == buffer)
        {
            /* Clear used offset and return to the free_buffers list */
            ptr->buffer_used = 0;
            *last_next = ptr->next;
            ptr->next = gridftp_op_arg->free_buffers;
            gridftp_op_arg->free_buffers = ptr;
            break;
        }
        last_next = &ptr->next;
        ptr = ptr->next;
    }
    gridftp_op_arg->registered_buffers_count--;
    globus_cond_signal(&gridftp_op_arg->cond);

    globus_mutex_unlock(&gridftp_op_arg->mutex);

    GlobusDsiRestExit();
}
/* globus_l_dsi_rest_gridftp_write_callback() */

globus_dsi_rest_read_t const            globus_dsi_rest_read_gridftp_op
                                      = globus_l_dsi_rest_read_gridftp_op;
