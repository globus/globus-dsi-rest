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


#include "globus_i_dsi_rest.h"

globus_result_t
uri_decode(char *s)
{
    const char unhex[] = 
    {
        ['0'] = 0,
        ['1'] = 1,
        ['2'] = 2,
        ['3'] = 3,
        ['4'] = 4,
        ['5'] = 5,
        ['6'] = 6,
        ['7'] = 7,
        ['8'] = 8,
        ['9'] = 9,
        ['a'] = 10, ['A'] = 10,
        ['b'] = 11, ['B'] = 11,
        ['c'] = 12, ['C'] = 12,
        ['d'] = 13, ['D'] = 13,
        ['e'] = 14, ['E'] = 14,
        ['f'] = 15, ['F'] = 15
    };

    char *r = s;
    char *w = s;

    while (*r)
    {
        if (*r == '%')
        {
            r++;

            if (!isxdigit(*r))
            {
                return GLOBUS_FAILURE;
            }
            int c = unhex[(unsigned char) *(r++)] << 4;
            if (!isxdigit(*r))
            {
                return GLOBUS_FAILURE;
            }
            c |= unhex[(unsigned char) *(r++)];
            *(w++) = c;
        }
        else if (*r == '+')
        {
            r++;
            *(w++) = ' ';
        }
        else
        {
            *(w++) = *(r++);
        }
    }
    *w = 0;
    return GLOBUS_SUCCESS;
}
