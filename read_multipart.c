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
 * @file read_multipart.c GridFTP DSI REST Read Multipart Response Callback
 */
#endif

#include "globus_i_dsi_rest.h"

static
globus_result_t
globus_l_dsi_rest_read_multipart(
    void                               *read_callback_arg,
    void                               *buffer,
    size_t                              buffer_length)
{
    globus_i_dsi_rest_read_multipart_arg_t
                                       *state = read_callback_arg;
    globus_result_t                     result = GLOBUS_SUCCESS;
    char                                boundary[state->boundary_length+9];

    GlobusDsiRestEnter();

    while (buffer_length > 0)
    {
        size_t                          boundary_length = 0;
        size_t                          scanned = 0;

        /*
         * This breaks the input buffer into chunks
         * based on the current boundary marker, keeping potential
         * boundary markers in a buffer if needed.
         */
        if (state->need_header)
        {
            /* Boundary between the header section and the body of the part */
            boundary_length = snprintf(
                boundary,
                sizeof(boundary),
                "\r\n\r\n");
        }
        else if (state->part_index == ((size_t) - 1))
        {
            /* Initial boundary (before 1st part) is --${boundary}\r\n */
            boundary_length = snprintf(
                boundary,
                sizeof(boundary),
                "--%s\r\n",
                state->boundary);
        }
        else if(state->part_index == (state->num_parts - 1))
        {
            /* Final boundary is \r\n--${boundary}--\r\n */
            boundary_length = snprintf(
                boundary,
                sizeof(boundary),
                "\r\n--%s--\r\n",
                state->boundary);
        }
        else
        {
            /* Other boundaries are \r\n--${boundary}\r\n */
            boundary_length = snprintf(
                boundary,
                sizeof(boundary),
                "\r\n--%s\r\n",
                state->boundary);
        }
        assert (boundary_length < sizeof(boundary));

        for (size_t i = 0;
            (i < buffer_length) && (state->match_counter < boundary_length);
            i++)
        {
            const char                  *b = buffer;

            scanned++;

            if (b[i] != boundary[state->match_counter])
            {
                state->match_counter = 0;
                /*
                 * Non-match. If there was anything in the boundary buffer,
                 * pass it on
                 */
                if (state->boundary_buffer_offset > 0)
                {
                    if (state->part_index != (size_t) -1)
                    {
                        globus_i_dsi_rest_read_part_t
                                       *part = &state->parts[state->part_index];
                        part->data_read_callback(
                            part->data_read_callback_arg,
                            state->boundary_buffer,
                            state->boundary_buffer_offset);
                    }
                    state->boundary_buffer_offset = 0;
                }
            }
            if (b[i] == boundary[state->match_counter])
            {
                state->match_counter++;
            }
        }

        GlobusDsiRestDebug(
            "Scanned %zu bytes, match_counter=%zu\n",
            scanned,
            state->match_counter);

        if (state->need_header)
        {
            char                       *header_buffer = NULL;
            header_buffer = realloc(
                state->header_buffer,
                state->header_buffer_offset + scanned + 1);

            state->header_buffer = header_buffer;

            memcpy(
                header_buffer,
                buffer,
                scanned);

            state->header_buffer_offset += scanned;
            state->header_buffer[state->header_buffer_offset] = 0;

            if (state->match_counter == boundary_length)
            {
                char                   *start = state->header_buffer;
                char                   *next = start;
                char                   *crlf = NULL;

                if (state->parts[state->part_index].response_callback != NULL)
                {
                    for (crlf = strstr(next, "\r\n");
                        crlf != NULL;
                        next = crlf+2, crlf = strstr(next, "\r\n"))
                    {
                        if (crlf[2] != ' ' && crlf[2] != '\t')
                        {
                            result = globus_i_dsi_rest_header_parse(
                                &state->parts[state->part_index].headers,
                                start,
                                crlf-start);
                            start = crlf+2;
                        }
                    }
                    state->parts[state->part_index].response_callback(
                        state->parts[state->part_index].response_callback_arg,
                        0,
                        NULL,
                        &state->parts[state->part_index].headers);
                }

                state->need_header = false;
                state->header_buffer_offset = 0;
                state->match_counter = 0;
            }
        }
        else if (state->match_counter == boundary_length)
        {
            globus_i_dsi_rest_read_part_t
                                       *part = NULL;
            /* Complete boundary reached */
            if (state->part_index == (size_t) -1)
            {
                GlobusDsiRestDebug("Finished preface\n");
                state->part_index++;
            }
            else
            {
                GlobusDsiRestData(
                    "Finished parsing part %zu: %.*s\n",
                    state->part_index,
                    (int) scanned - boundary_length,
                    buffer);
                part = &state->parts[state->part_index];
                if (scanned > boundary_length)
                {
                    part->data_read_callback(
                        part->data_read_callback_arg,
                        buffer,
                        scanned - boundary_length);
                }
                part->data_read_callback(
                    part->data_read_callback_arg,
                    buffer,
                    0);
                state->part_index++;
            }
            if (state->part_index < state->num_parts)
            {
                state->need_header = true;
            }
            state->match_counter = 0;
        }
        else if (state->match_counter > 0)
        {
            /*
             * Partial boundary match, keep the data in the boundary
             * buffer until we are certain
             */
            memcpy(
                state->boundary_buffer + state->boundary_buffer_offset,
                buffer,
                scanned);
            state->boundary_buffer_offset += scanned;
        }
        else
        {
            /*
             * Not a boundary match, pass to the read data
             * callback
             */
            if (state->part_index != (size_t) -1)
            {
                globus_i_dsi_rest_read_part_t
                                       *part = &state->parts[state->part_index];
                part->data_read_callback(
                    part->data_read_callback_arg,
                    buffer,
                    buffer_length);
            }
            state->boundary_buffer_offset = 0;
        }
        buffer = ((char *)buffer) + scanned;
        buffer_length -= scanned;
    }

    GlobusDsiRestExitResult(result);

    return result;
}
/* globus_l_dsi_rest_read_multipart() */

globus_dsi_rest_read_t const            globus_dsi_rest_read_multipart
                                      = globus_l_dsi_rest_read_multipart;
