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
 * @file write_gridftp_op.c GridFTP DSI REST Write GridFTP Operation Callback
 */
#endif

#include "globus_i_dsi_rest.h"

static
bool
globus_l_dsi_rest_is_transfer_offset_ready(
    globus_i_dsi_rest_gridftp_op_arg_t *gridftp_op_arg);

static
bool
globus_l_dsi_rest_is_reading_complete(
    globus_i_dsi_rest_gridftp_op_arg_t *gridftp_op_arg);

static
globus_result_t
globus_l_dsi_rest_write_register_reads(
    globus_i_dsi_rest_gridftp_op_arg_t *gridftp_op);

static
globus_result_t
globus_l_dsi_rest_write_gridftp_op(
    void                               *write_callback_arg,
    void                               *buffer,
    size_t                              buffer_length,
    size_t                             *amount_copied)
{
    globus_i_dsi_rest_gridftp_op_arg_t *gridftp_op_arg = write_callback_arg;
    globus_result_t                     result = GLOBUS_SUCCESS;
    size_t                              buffer_filled = 0;
    globus_i_dsi_rest_buffer_t         *rest_buffer;

    GlobusDsiRestEnter();

    globus_mutex_lock(&gridftp_op_arg->mutex);

    while ((!globus_l_dsi_rest_is_transfer_offset_ready(gridftp_op_arg))
           && (!globus_l_dsi_rest_is_reading_complete(gridftp_op_arg)))
    {
        globus_l_dsi_rest_write_register_reads(gridftp_op_arg);

        GlobusDsiRestTrace("Waiting for current offset %"PRIu64" to be ready\n",
                      gridftp_op_arg->offset);

        globus_cond_wait(&gridftp_op_arg->cond, &gridftp_op_arg->mutex);
    }

    if (gridftp_op_arg->result != GLOBUS_SUCCESS)
    {
        result = gridftp_op_arg->result;

        goto done;
    }

    while ((buffer_filled < buffer_length)
            && globus_l_dsi_rest_is_transfer_offset_ready(gridftp_op_arg))
    {
        int                             to_copy;

        rest_buffer = gridftp_op_arg->pending_buffers;

        to_copy = buffer_length - buffer_filled;

        if (to_copy > rest_buffer->buffer_used)
        {
            to_copy = rest_buffer->buffer_used;
        }
        assert (to_copy > 0);

        memcpy(((char *)buffer)+buffer_filled, rest_buffer->buffer, to_copy);
        buffer_filled += to_copy;
        gridftp_op_arg->offset += to_copy;

        if (to_copy < rest_buffer->buffer_used)
        {
            /* Partial buffer copy */
            GlobusDsiRestTrace("partial_buffer_copy: op=%p bytes_copied=%d\n",
                    (void *) gridftp_op_arg->op,
                    to_copy);
            memmove(rest_buffer->buffer, rest_buffer->buffer + to_copy, rest_buffer->buffer_used - to_copy);
            rest_buffer->buffer_used -= to_copy;
            rest_buffer->transfer_offset += to_copy;
        }
        else
        {
            GlobusDsiRestTrace("add_to_pending_buffers: op=%p rest_buffer=%p\n",
                    (void *) gridftp_op_arg->op,
                    (void *) rest_buffer);

            gridftp_op_arg->pending_buffers = rest_buffer->next;
            if (gridftp_op_arg->pending_buffers == NULL)
            {
                gridftp_op_arg->pending_buffers_last = &gridftp_op_arg->pending_buffers;
            }
            rest_buffer->transfer_offset = UINT64_C(-1);
            rest_buffer->buffer_used = 0;

            rest_buffer->next = gridftp_op_arg->free_buffers;
            gridftp_op_arg->free_buffers = rest_buffer;
        }
    }
done:
    globus_mutex_unlock(&gridftp_op_arg->mutex);

    *amount_copied = buffer_filled;

    GlobusDsiRestExitResult(result);

    return result;
}
/* globus_l_dsi_rest_write_gridftp_op() */

static
void
globus_l_dsi_rest_gridftp_read_callback(
    globus_gfs_operation_t              op,
    globus_result_t                     result,
    globus_byte_t                      *buffer,
    globus_size_t                       nbytes,
    globus_off_t                        offset,
    globus_bool_t                       eof,
    void                               *user_arg)
{
    globus_i_dsi_rest_gridftp_op_arg_t *gridftp_op_arg = user_arg;
    globus_i_dsi_rest_buffer_t         *rest_buffer;
    globus_i_dsi_rest_buffer_t         *tmp;
    globus_i_dsi_rest_buffer_t        **prev_next;
    bool                                signal = eof;

    GlobusDsiRestEnter();

    GlobusDsiRestDebug("op=%p result=%#x buffer=%p nbytes=%zd offset=%"PRIuMAX" eof=%d user_arg=%p\n",
            (void *) op,
            result,
            (void *) buffer,
            (size_t) nbytes,
            (uintmax_t) offset,
            (int) eof,
            user_arg);

    globus_mutex_lock(&gridftp_op_arg->mutex);

    gridftp_op_arg->eof |= eof;

    /* Remove from the registered buffer list */
    rest_buffer = gridftp_op_arg->registered_buffers;
    prev_next = &gridftp_op_arg->registered_buffers;

    while (rest_buffer != NULL)
    {
        if (rest_buffer->buffer == buffer)
        {
            *prev_next = rest_buffer->next;
            break;
        }
        prev_next = &rest_buffer->next;
        rest_buffer = rest_buffer->next;
    }

    assert(rest_buffer != NULL);

    /* Update the info about this buffer */
    rest_buffer->transfer_offset = offset;
    rest_buffer->buffer_used = nbytes;

    globus_gridftp_server_update_bytes_recvd(
            gridftp_op_arg->op,
            nbytes);

    /*
     * If something bad occurred, we will discard this block and flag
     * the request with an error
     */
    if (gridftp_op_arg->result != GLOBUS_SUCCESS)
    {
        gridftp_op_arg->eof = true;
        goto bad_read;
    }

    if (rest_buffer->buffer_used == 0)
    {
        /* empty buffer */
        rest_buffer->transfer_offset = UINT64_C(-1);
        rest_buffer->buffer_used = UINT64_C(-1);

        rest_buffer->next = gridftp_op_arg->free_buffers;
        gridftp_op_arg->free_buffers = rest_buffer;
    }
    else
    {
        /* Add it to the pending buffer list (in order) */
        tmp = gridftp_op_arg->pending_buffers;
        prev_next = &gridftp_op_arg->pending_buffers;
        while (tmp != NULL)
        {
            if ((tmp->transfer_offset + tmp->buffer_used)
                        <= rest_buffer->transfer_offset)
            {
                prev_next = &tmp->next;
                tmp = tmp->next;
            }
            else
            {
                break;
            }
        }
        /* tmp is the first buffer in pending that starts after rest_buffer 
         * (may be null) */
        rest_buffer->next = tmp;
        if (tmp == NULL)
        {
            gridftp_op_arg->pending_buffers_last = &rest_buffer->next;
        }

        *prev_next = rest_buffer;

        signal = true;
    }

    if (signal)
    {
        globus_cond_signal(&gridftp_op_arg->cond);
    }
    globus_mutex_unlock(&gridftp_op_arg->mutex);

    GlobusDsiRestExit();
    return;

bad_read:
    /* Since this was a bad read, we'll ignore the data */
    rest_buffer->buffer_used = 0;
    rest_buffer->transfer_offset = 0;
    rest_buffer->next = gridftp_op_arg->free_buffers;
    gridftp_op_arg->free_buffers = rest_buffer;

    globus_cond_signal(&gridftp_op_arg->cond);
    globus_mutex_unlock(&gridftp_op_arg->mutex);

    GlobusDsiRestExit();
    return;
}
/* globus_l_dsi_rest_gridftp_read_callback() */

static
globus_result_t
globus_l_dsi_rest_write_register_reads(
    globus_i_dsi_rest_gridftp_op_arg_t *gridftp_op_arg)
{
    globus_i_dsi_rest_buffer_t         *buffer;
    globus_size_t                       optimal_blocksize;
    int                                 optimal_concurrency;
    int                                 currently_registered;
    globus_result_t                     result = GLOBUS_SUCCESS;

    GlobusDsiRestEnter();

    buffer = gridftp_op_arg->registered_buffers;
    currently_registered = 0;

    while (buffer != NULL)
    {
        currently_registered++;
        buffer = buffer->next;
    }
    GlobusDsiRestTrace(
            "Currently %d blocks registered\n",
            currently_registered);

    if (gridftp_op_arg->result == GLOBUS_SUCCESS && !gridftp_op_arg->eof)
    {
        /* Reads still might be useful */
        globus_gridftp_server_get_block_size(
                gridftp_op_arg->op,
                &optimal_blocksize);

        globus_gridftp_server_get_optimal_concurrency(
                gridftp_op_arg->op,
                &optimal_concurrency);
        
        GlobusDsiRestTrace("Optimal is %d blocks\n", optimal_concurrency);

        for (int i = currently_registered; i < optimal_concurrency; i++)
        {
            buffer = globus_i_dsi_rest_buffer_get(
                    gridftp_op_arg, (size_t) optimal_blocksize);
            gridftp_op_arg->current_buffer = NULL;

            if (buffer == NULL)
            {
                result = GlobusDsiRestErrorMemory();

                goto buffer_fail;
            }
            buffer->transfer_offset = (uint64_t) -1;

            assert(buffer->buffer_used == 0);

            GlobusDsiRestTrace("Registering read with rest_buffer=%p\n",
                          (void *) buffer);
            result = globus_gridftp_server_register_read(
                    gridftp_op_arg->op,
                    buffer->buffer,
                    buffer->buffer_len,
                    globus_l_dsi_rest_gridftp_read_callback,
                    gridftp_op_arg);
            if (result != GLOBUS_SUCCESS)
            {
                goto register_fail;
            }
            buffer->next = gridftp_op_arg->registered_buffers;
            gridftp_op_arg->registered_buffers = buffer;
        }
    }

register_fail:
buffer_fail:
    GlobusDsiRestExit();
    return result;
}
/* globus_l_dsi_rest_write_register_reads() */

static
bool
globus_l_dsi_rest_is_transfer_offset_ready(
    globus_i_dsi_rest_gridftp_op_arg_t *gridftp_op_arg)
{
    const globus_i_dsi_rest_buffer_t   *dsi_rest_buffer;
    bool                                b;
    
    dsi_rest_buffer = gridftp_op_arg->pending_buffers;

    b = (dsi_rest_buffer != NULL) &&
           (dsi_rest_buffer->transfer_offset == gridftp_op_arg->offset);

    GlobusDsiRestExitBool(b);

    return b;
}
/* globus_l_dsi_rest_is_transfer_offset_ready() */

static
bool
globus_l_dsi_rest_is_reading_complete(
    globus_i_dsi_rest_gridftp_op_arg_t *gridftp_op_arg)
{
    bool                                b;
    GlobusDsiRestEnter();

    b = gridftp_op_arg->eof && (gridftp_op_arg->registered_buffers == NULL);

    GlobusDsiRestExitBool(b);
    return b;
}
/* globus_l_dsi_rest_is_reading_complete() */

globus_dsi_rest_write_t const           globus_dsi_rest_write_gridftp_op
                                      = globus_l_dsi_rest_write_gridftp_op;
