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
void
globus_l_dsi_rest_count_buffers(
    globus_i_dsi_rest_buffer_t         *buffer,
    int                                *currentp,
    globus_off_t                       *bytes_availablep,
    globus_off_t                       *bytes_usedp);

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

    if (gridftp_op_arg->result == GLOBUS_SUCCESS)
    {
        gridftp_op_arg->result = result;
    }

    while ((!globus_l_dsi_rest_is_transfer_offset_ready(gridftp_op_arg))
           && (!globus_l_dsi_rest_is_reading_complete(gridftp_op_arg)))
    {
        int                             currently_registered = 0;
        globus_off_t                    bytes_registered = 0;
        int                             currently_pending = 0;
        globus_off_t                    bytes_pending = 0;

        globus_l_dsi_rest_write_register_reads(gridftp_op_arg);

        globus_l_dsi_rest_count_buffers(
                gridftp_op_arg->registered_buffers,
                &currently_registered,
                &bytes_registered,
                NULL);
        globus_l_dsi_rest_count_buffers(
                gridftp_op_arg->pending_buffers,
                &currently_pending,
                NULL,
                &bytes_pending);

        GlobusDsiRestDebug(
            "waiting: "
            "op=%p "
            "wait_offset=%"PRIu64" "
            "result=%#x "
            "eof=%s "
            "currently_registered=%d "
            "bytes_registered=%"GLOBUS_OFF_T_FORMAT" "
            "currently_pending=%d "
            "bytes_pending=%"GLOBUS_OFF_T_FORMAT"\n",
            (void *) gridftp_op_arg->op,
            gridftp_op_arg->offset,
            gridftp_op_arg->result,
            gridftp_op_arg->eof ? "true" : "false",
            currently_registered,
            bytes_registered,
            currently_pending,
            bytes_pending);

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

    GlobusDsiRestLogResult(GLOBUS_DSI_REST_TRACE, result);

    GlobusDsiRestDebug(
        "op=%p "
        "result=%#x "
        "buffer=%p "
        "nbytes=%zd "
        "offset=%"PRIuMAX" "
        "eof=%d "
        "user_arg=%p\n",
        (void *) op,
        result,
        (void *) buffer,
        (size_t) nbytes,
        (uintmax_t) offset,
        (int) eof,
        user_arg);

    globus_mutex_lock(&gridftp_op_arg->mutex);

    *gridftp_op_arg->eofp |= eof;
    if (gridftp_op_arg->result == GLOBUS_SUCCESS)
    {
        gridftp_op_arg->result = result;
    }

    if (gridftp_op_arg->end_offset != 0 &&
        (offset + nbytes) == (gridftp_op_arg->end_offset))
    {
        signal = eof = true;
    }


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

    if (GlobusDebugTrue(GLOBUS_DSI_REST, GLOBUS_DSI_REST_TRACE))
    {
        int                             currently_pending = 0;
        globus_off_t                    bytes_pending = 0;

        globus_l_dsi_rest_count_buffers(
                gridftp_op_arg->pending_buffers,
                &currently_pending,
                NULL,
                &bytes_pending);

        GlobusDsiRestTrace(
                "op=%p currently_pending=%d bytes_pending=%"GLOBUS_OFF_T_FORMAT" end_offset=%"GLOBUS_OFF_T_FORMAT"\n",
                (void *) gridftp_op_arg->op,
                currently_pending,
                bytes_pending,
                gridftp_op_arg->end_offset);
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
    globus_off_t                        bytes_registered = 0;
    int                                 optimal_concurrency;
    int                                 currently_registered;
    globus_result_t                     result = GLOBUS_SUCCESS;

    GlobusDsiRestEnter();


    if (gridftp_op_arg->result == GLOBUS_SUCCESS && !gridftp_op_arg->eof)
    {
        globus_l_dsi_rest_count_buffers(
                gridftp_op_arg->registered_buffers,
                &currently_registered,
                &bytes_registered,
                NULL);

        GlobusDsiRestTrace(
                "op=%p currently_registered=%d bytes_registered=%"GLOBUS_OFF_T_FORMAT" end_offset=%"GLOBUS_OFF_T_FORMAT"\n",
                (void *) gridftp_op_arg->op,
                currently_registered,
                bytes_registered,
                gridftp_op_arg->end_offset);

        /* Reads still might be useful */
        globus_gridftp_server_get_block_size(
                gridftp_op_arg->op,
                &optimal_blocksize);

        globus_gridftp_server_get_optimal_concurrency(
                gridftp_op_arg->op,
                &optimal_concurrency);
        
        GlobusDsiRestTrace("op=%p optimal_blocksize=%zu optimal_concurrency=%d\n",
                (void *) gridftp_op_arg->op,
                (size_t) optimal_blocksize,
                optimal_concurrency);

        for (int i = currently_registered; i < optimal_concurrency; i++)
        {
            globus_off_t                this_read = 0;
            globus_off_t                remaining = optimal_blocksize;

            if (gridftp_op_arg->end_offset != (globus_off_t)-1)
            {
                /* DSI requested partial transfer. This only works 
                 * if the DSI forces ordering on the read callbacks
                 */
                remaining = gridftp_op_arg->end_offset
                        - gridftp_op_arg->offset
                        - bytes_registered;
                if (remaining == 0)
                {
                    break;
                }
            }
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

            this_read = buffer->buffer_len;
            if (remaining < buffer->buffer_len)
            {
                this_read = remaining;
            }

            GlobusDsiRestDebug("register_read: "
                    "op=%p "
                    "rest_buffer=%p "
                    "buffer=%p "
                    "length=%"GLOBUS_OFF_T_FORMAT"\n",
                    (void *) gridftp_op_arg->op,
                    (void *) buffer,
                    (void *) buffer->buffer,
                    this_read);

            result = globus_gridftp_server_register_read(
                    gridftp_op_arg->op,
                    buffer->buffer,
                    this_read,
                    globus_l_dsi_rest_gridftp_read_callback,
                    gridftp_op_arg);
            if (result != GLOBUS_SUCCESS)
            {
                goto register_fail;
            }

            bytes_registered += this_read;

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
    
    GlobusDsiRestEnter();
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

static
void
globus_l_dsi_rest_count_buffers(
    globus_i_dsi_rest_buffer_t         *buffer,
    int                                *currentp,
    globus_off_t                       *bytes_availablep,
    globus_off_t                       *bytes_usedp)
{
    int                                 current = 0;
    globus_off_t                        bytes_available = 0;
    globus_off_t                        bytes_used = 0;

    while (buffer != NULL)
    {
        current++;
        bytes_available += buffer->buffer_len - buffer->buffer_used;
        bytes_used += buffer->buffer_used;

        buffer = buffer->next;
    }
    *currentp = current;
    if (bytes_availablep != NULL)
    {
        *bytes_availablep = bytes_available;
    }
    if (bytes_usedp != NULL)
    {
        *bytes_usedp = bytes_used;
    }
}
/* globus_l_dsi_rest_count_buffers() */

globus_dsi_rest_write_t const           globus_dsi_rest_write_gridftp_op
                                      = globus_l_dsi_rest_write_gridftp_op;
