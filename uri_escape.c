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
 * @file uri_escape.c GridFTP DSI REST URI Escape
 */
#endif

#include "globus_i_dsi_rest.h"

void
globus_i_dsi_rest_uri_escape(
    const char                         *raw,
    char                              **encodedp,
    size_t                             *availablep)
{
    char                               *encoded = *encodedp;
    size_t                              available = *availablep;
    static const char                  *encoding_table = "0123456789ABCDEF";

    while (*raw != 0)
    {
        switch (*raw)
        {
            case 'a': case 'b': case 'c': case 'd': case 'e':
            case 'f': case 'g': case 'h': case 'i': case 'j':
            case 'k': case 'l': case 'm': case 'n': case 'o':
            case 'p': case 'q': case 'r': case 's': case 't':
            case 'u': case 'v': case 'w': case 'x': case 'y':
            case 'z':

            case 'A': case 'B': case 'C': case 'D': case 'E':
            case 'F': case 'G': case 'H': case 'I': case 'J':
            case 'K': case 'L': case 'M': case 'N': case 'O':
            case 'P': case 'Q': case 'R': case 'S': case 'T':
            case 'U': case 'V': case 'W': case 'X': case 'Y':
            case 'Z':

            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':

            case '-': case '_': case '.': case '~':
                assert(available > 0);

                *(encoded++) = *(raw++);
                available--;
                break;

            case ' ':
                assert(available > 0);
                *(encoded++) = '+';
                available--;
                raw++;
                break;
                
            default:
                assert(available > 0);
                *(encoded++) = '%';
                available--;

                assert(available > 0);
                *(encoded++) = encoding_table[(((unsigned int) (*raw)) >> 4) & 0xf];
                available--;

                assert(available > 0);
                *(encoded++) = encoding_table[(((unsigned int) (*raw)) & 0xf)];
                available--;
                raw++;
                break;
        }
    }
    *encodedp = encoded;
    *availablep = available;
}
/* globus_l_dsi_rest_uri_encode() */

globus_result_t
globus_dsi_rest_uri_escape(
    const char                         *s,
    char                              **escapedp)
{
    size_t                              slen = strlen(s);
    size_t                              elen = slen*3+1;
    char                               *encoded = malloc(elen);
    char                               *save = encoded;
    char                              **p = &encoded;

    if (encoded == NULL)
    {
        return GlobusDsiRestErrorMemory();
    }

    globus_i_dsi_rest_uri_escape(s, p, &elen);
    *(*p) = 0;

    *escapedp = save;
    return GLOBUS_SUCCESS;
}
/* globus_dsi_rest_uri_escape() */
