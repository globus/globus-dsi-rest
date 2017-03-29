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
 * @file write_multipart.c GridFTP DSI REST Write Multipart Callback
 */
#endif

#include "globus_i_dsi_rest.h"

static
void
globus_l_dsi_rest_write_boundary(
    char                               *buffer,
    size_t                              buffer_length,
    char                               *boundary,
    size_t                              boundary_length,
    size_t                             *amount_copied);

static
globus_result_t
globus_l_dsi_rest_prepare_boundary(
    const char                         *delimiter,
    bool                                final,
    globus_dsi_rest_key_array_t        *part_header,
    char                              **boundaryp,
    size_t                             *boundary_lengthp);


static
globus_result_t
globus_l_dsi_rest_write_multipart(
    void                               *write_callback_arg,
    void                               *buffer,
    size_t                              buffer_length,
    size_t                             *amount_copied)
{
    globus_i_dsi_rest_write_multipart_arg_t
                                       *state = write_callback_arg;
    globus_result_t                     result = GLOBUS_SUCCESS;
    size_t                              this_copy = 0;
    size_t                              copied = 0;

    GlobusDsiRestEnter();

    /* If we were mid-boundary, write it */
    if (state->current_boundary != NULL)
    {
        globus_l_dsi_rest_write_boundary(
                buffer,
                buffer_length,
                state->current_boundary + state->current_boundary_offset,
                state->current_boundary_length - state->current_boundary_offset,
                &this_copy);

        state->current_boundary_offset += this_copy;
        copied += this_copy;

        assert(copied <= buffer_length);

        if (state->current_boundary_offset == state->current_boundary_length)
        {
            free(state->current_boundary);
            state->current_boundary = NULL;
            state->current_boundary_offset = 0;
            state->current_boundary_length = 0;
        }
    }
    for (;
        copied < buffer_length && state->part_index < state->num_parts;
        )
    {
        /* Call next part write function with remaining buffer */
        result = state->parts[state->part_index].data_write_callback(
                state->parts[state->part_index].data_write_callback_arg,
                ((char *) buffer) + copied,
                buffer_length - copied,
                &this_copy);

        if (result != GLOBUS_SUCCESS)
        {
            break;
        }

        copied += this_copy;
        assert(copied <= buffer_length);

        if (this_copy == 0)
        {
            /* If we finished the part, prepare boundary */
            if (state->boundary != NULL)
            {
                bool                    final =
                        (state->part_index == state->num_parts-1);

                result = globus_l_dsi_rest_prepare_boundary(
                        state->boundary,
                        final,
                        final
                            ? NULL
                            : &state->parts[state->part_index+1].headers,
                        &state->current_boundary,
                        &state->current_boundary_length);
                if (result != GLOBUS_SUCCESS)
                {
                    goto prepare_boundary_error;
                }
                state->part_index++;
            }
            /* Copy boundary if space in buffer */
            if (state->current_boundary != NULL)
            {
                globus_l_dsi_rest_write_boundary(
                        ((char *)buffer)+copied,
                        buffer_length-copied,
                        state->current_boundary
                            + state->current_boundary_offset,
                        state->current_boundary_length
                            - state->current_boundary_offset,
                        &this_copy);

                state->current_boundary_offset += this_copy;
                copied += this_copy;

                assert(state->current_boundary_offset
                        <= state->current_boundary_length);
                assert(copied <= buffer_length);

                if (state->current_boundary_offset == state->current_boundary_length)
                {
                    free(state->current_boundary);
                    state->current_boundary = NULL;
                    state->current_boundary_offset = 0;
                    state->current_boundary_length = 0;
                }
            }
        }
    }

prepare_boundary_error:
    *amount_copied = copied;

    GlobusDsiRestExitResult(result);

    return result;
}
/* globus_l_dsi_rest_write_multipart() */

static
void
globus_l_dsi_rest_write_boundary(
    char                               *buffer,
    size_t                              buffer_length,
    char                               *boundary,
    size_t                              boundary_length,
    size_t                             *amount_copied)
{
    size_t                              to_copy = buffer_length;

    if (to_copy > boundary_length)
    {
        to_copy = boundary_length;
    }

    memcpy(buffer, boundary, to_copy);
    *amount_copied = to_copy;
}
/* globus_l_dsi_rest_write_boundary() */

static
globus_result_t
globus_l_dsi_rest_prepare_boundary(
    const char                         *delimiter,
    bool                                final,
    globus_dsi_rest_key_array_t        *part_header,
    char                              **boundaryp,
    size_t                             *boundary_lengthp)
{
    globus_result_t                     result = GLOBUS_SUCCESS;
    char                               *boundary = NULL;
    char                               *p = NULL;
    size_t                              boundary_length = 0;

    GlobusDsiRestEnter();

    if (!final)
    {
        boundary_length += 8 + strlen(delimiter);
        if (part_header != NULL)
        {
            for (size_t i = 0; i < part_header->count; i++)
            {
                if (part_header->key_value[i].key != NULL
                    && part_header->key_value[i].value != NULL)
                {
                    boundary_length += strlen(part_header->key_value[i].key);
                    boundary_length += strlen(part_header->key_value[i].value);
                    boundary_length += 4;
                }
            }
        }
        p = boundary = malloc(boundary_length + 1);
        if (boundary == NULL)
        {
            result = GlobusDsiRestErrorMemory();

            goto boundary_alloc_fail;
        }
        p[0] = 0;
        p += sprintf(p, "\r\n--%s\r\n", delimiter);
        if (part_header != NULL)
        {
            for (size_t i = 0; i < part_header->count; i++)
            {
                if (part_header->key_value[i].key != NULL
                    && part_header->key_value[i].value != NULL)
                {
                    p += sprintf(p, "%s: %s\r\n",
                            part_header->key_value[i].key,
                            part_header->key_value[i].value);
                }
            }
        }
        p += sprintf(p, "\r\n");
    }
    else
    {
        boundary_length = 8 + strlen(delimiter);
        boundary = malloc(boundary_length + 1);
        if (boundary == NULL)
        {
            result = GlobusDsiRestErrorMemory();

            goto boundary_alloc_fail;
        }

        sprintf(boundary, "\r\n--%s--\r\n", delimiter);
    }

boundary_alloc_fail:
    *boundaryp = boundary;
    *boundary_lengthp = boundary_length;

    GlobusDsiRestExitResult(result);
    return result;
}
/* globus_l_dsi_rest_prepare_boundary() */

globus_dsi_rest_write_t const           globus_dsi_rest_write_multipart
                                      = globus_l_dsi_rest_write_multipart;
