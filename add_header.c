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
 * @file add_header.c GridFTP DSI REST Add Header
 */
#endif

#include "globus_i_dsi_rest.h"

globus_result_t
globus_i_dsi_rest_add_header(
    struct curl_slist                 **request_headers,
    const char                         *header_name,
    const char                         *header_value)
{
    globus_result_t                     result = GLOBUS_SUCCESS;
    struct curl_slist                  *tmp;

    GlobusDsiRestEnter();

    assert(request_headers != NULL);
    assert(header_name != NULL);
    assert(header_value != NULL);

    /* TODO: verify key is valid (rfc7230):
     *
     * Most HTTP header field values are defined using common syntax
     * components (token, quoted-string, and comment) separated by
     * whitespace or specific delimiting characters.  Delimiters are chosen
     * from the set of US-ASCII visual characters not allowed in a token
     * (DQUOTE and "(),/:;<=>?@[\]{}").
     *          token          = 1*tchar
     *          tchar          = "!" / "#" / "$" / "%" / "&" / "'" / "*"
     *          / "+" / "-" / "." / "^" / "_" / "`" / "|" / "~"
     *          / DIGIT / ALPHA
     *          ; any VCHAR, except delimiters
     */

    /*
     * TODO: verify value is valid either non-space US-ASCII or use 
     * quoting or RFC 5987 ext-value)
     */
    size_t len = snprintf(NULL, 0, "%s: %s",
            header_name,
            header_value);
    char str[len+1];

    snprintf(str, sizeof(str), "%s: %s",
            header_name,
            header_value);

    /* TODO: replace if header already present */
    tmp = curl_slist_append(*request_headers, str);

    if (tmp == NULL)
    {
        result = GlobusDsiRestErrorMemory();
        goto error_memory;
    }
    *request_headers = tmp;

error_memory:
    GlobusDsiRestExitResult(result);

    return result;
}
/* globus_i_dsi_rest_add_header() */
