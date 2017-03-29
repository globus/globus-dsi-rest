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

/**
 * @file globus_dsi_rest.h GridFTP DSI REST Helper API
 *
 * This header describes the public interface to the GridFTP DSI REST Helper
 * API.
 *
 */

/** 
 * @mainpage
 *
 * This header describes the public interface to the GridFTP DSI REST Helper
 * API.
 *
 * It was designed with the following considerations in mind:
 *
 * - GET, PUT, POST, DELETE, PATCH are all used by Google drive API
 * - Some (not all) of the operations will send binary data
 * - Some (not all) of the operations will send json data
 * - Some (not all) of the operations receive binary data
 * - Some (not all) of the operations receive json data
 * - Some (not all) of the operations include query parameters that need
 *   URL form encoding
 * - Some operations send or receive multipart mime data
 * - Access tokens might expire, need to be able to express the need to refresh
 *   and get a new one
 * - Arbitrary headers needed for partial downloads, resumable uploads, azure
 *   storage
 *  
 * The interface for this library is described in the @ref globus_dsi_rest.h
 * header.
 */

#ifndef GLOBUS_DSI_REST_H
#define GLOBUS_DSI_REST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "globus_common.h"
#include "globus_gridftp_server.h"

/**
 * @defgroup globus_dsi_rest_data Data Types
 */

/**
 * @brief Key-Value Pairs
 * @ingroup globus_dsi_rest_data
 * @details
 * This data type is used to express both URI query parameters and HTTP
 * headers. Values are encoded as appropriate for the particular use of the
 * data.
 */
typedef 
struct globus_dsi_rest_key_value_s
{
    /** Key name (null terminated) */
    const char                     *key;
    /** Value (null terminated) */
    const char                     *value;
}
globus_dsi_rest_key_value_t;

/**
 * @brief Key-Value Pair Array
 * @ingroup globus_dsi_rest_data
 * @details
 * This data type is used to encapsulate an array of
 * globus_dsi_rest_key_value_t structs with the length of the array.
 */
typedef
struct globus_dsi_rest_key_array_s
{
    /** Number of elements in the key_value array */
    size_t                              count;
    /** Array of Key-Value pairs */
    globus_dsi_rest_key_value_t        *key_value;
}
globus_dsi_rest_key_array_t;

/**
 * @defgroup globus_dsi_rest_callback_signatures Callback Type Signatures
 */
/**
 * @brief Response Callback Signature
 * @ingroup globus_dsi_rest_callback_signatures
 * @details
 *     This is the type signature for the response callback. The response
 *     callback is called called by the request processor when it receives
 *     the HTTP response code, status, and headers. This is called after
 *     the request body is sent in the write callbacks, and before the response
 *     body is received and parsed in the read callbacks.
 *
 * @param[in] response_callback_arg
 *     DSI-specific callback argument.
 * @param[in] response_code
 *     HTTP response code. This is typically a 3 digit number (200, 404, etc),
 *     but may be the special value 0 in the case of a protocol error.
 * @param[in] response_status
 *     HTTP response status. This is typically a short string like ("Ok",
 *     "Continue", "Not Found", etc).
 * @param[in] response_headers
 *     The set of headers included in the response.
 *
 * @return
 *     The DSI should return GLOBUS_SUCCESS to allow processing to continue.
 *     Otherwise, the REST operation will be aborted and the result will be
 *     returned from the request.
 */
typedef
globus_result_t (*globus_dsi_rest_response_t) (
    void                               *response_callback_arg,
    int                                 response_code,
    const char                         *response_status,
    const globus_dsi_rest_key_array_t  *response_headers);

/**
 * @brief Data Write Callback Signature
 * @ingroup globus_dsi_rest_callback_signatures
 * @details
 *     This is the type signature for the callback to write data TO the REST
 *     server (i.e., to send the body of a request to a server). This is
 *     typically needed for POST, PATCH, or PUT requests.
 *
 *     This function is called repeatedly by the DSI REST Helper API to send a
 *     part of the data to the REST server, until the function returns an error
 *     or sets the value pointed to by amount_copied to 0.
 *
 *     Below are some specialized implementations of this function to cover
 *     some common cases for clients (such as sending data from a GridFTP
 *     control channel for a DSI operation).
 *
 * @param[in] write_callback_arg
 *     DSI-specific data passed to the request operation.
 * @param[inout] buffer
 *     A buffer provided by the DSI REST API. The DSI copies REST payload data
 *     into this buffer.
 * @param[in] buffer_length
 *     The length of the array pointed to by the buffer parameter.
 * @param[out] amount_copied
 *     The amount of data copied into buffer by this call. The DSI may set this
 *     to 0 to signal end of file.
 * @return
 *     The DSI should return GLOBUS_SUCCESS to allow processing to continue.
 *     Otherwise, the REST operation will be aborted and the result will
 *     be returned from the request.
 */
typedef
globus_result_t (*globus_dsi_rest_write_t) (
    void                               *write_callback_arg,
    void                               *buffer,
    size_t                              buffer_length,
    size_t                             *amount_copied);

/**
 * @brief Data Read Callback Signature
 * @ingroup globus_dsi_rest_callback_signatures
 * @details 
 *     This is the type signature for the callback to read data FROM the REST
 *     server (i.e., to read the body of a reply from a server). This is
 *     typically needed for GET, POST, or PATCH requests.
 *
 *     This function is called repeatedly by the DSI REST Helper API to send a
 *     part of the data from the REST server, until the function returns an
 *     error or there is no more data. In a non-error situation, the final call
 *     to this function will be with a buffer_length of 0.
 *
 *     Below are some specialized implementations of this function to cover
 *     some common cases for clients (such as receiving a json response, and
 *     pushing data from a REST response to a GridFTP control channel for a DSI
 *     operation).
 *
 * @param[in] read_callback_arg
 *     DSI-specific data passed to the request operation.
 * @param[inout] buffer
 *     A buffer provided by the DSI REST API. The DSI processes the REST
 *     payload data from this buffer.
 * @param[in] buffer_length
 *     The amount of the array pointed to by the buffer parameter. The final
 *     time this is called for a particular operation, the value is 0.
 * @return
 *     The DSI should return GLOBUS_SUCCESS to allow processing to continue.
 *     Otherwise, the REST operation will be aborted and the result will
 *     be returned from the request.
 */
typedef
globus_result_t
(*globus_dsi_rest_read_t) (
    void                               *read_callback_arg,
    void                               *buffer,
    size_t                              buffer_length);

/**
 * @brief Request Complete Callback Signature
 * @ingroup globus_dsi_rest_callback_signatures
 * @details 
 *     This is the type signature for the callback to be called upon completion
 *     of processing an entire REST request.
 *
 *     This function is called once after all other callbacks related to the
 *     request have returned and includes the final status of the request.
 *
 *
 * @param[in] complete_callback_arg
 *     DSI-specific data passed to the request operation.
 * @param[in] result
 *     The final status of the request.
 */
typedef
void
(*globus_dsi_rest_complete_t) (
    void                               *complete_callback_arg,
    globus_result_t                     result);

/**
 * @brief Request Progress Callback Signature
 * @ingroup globus_dsi_rest_callback_signatures
 * @details 
 *     This is the type signature for the callback to be called periodically
 *     during processing a REST request.
 *
 *     This function is called regularly to provide progress information for
 *     a request. This may be useful for providing application feedback, or
 *     for checking for idle timeouts.
 *
 *     Below is one specialized implementation of this function to cover
 *     the common case of an idle timeout.
 *
 * @param[in] progress_callback_arg
 *     DSI-specific data passed to the request operation.
 * @param[in] total_read
 *     Total amount of data expected to be read during processing this request
 *     if known, otherwise 0.
 * @param[in] amt_read
 *     Amount of data read so far processing this request.
 * @param[in] total_written
 *     Total amount of data expected to be writte during processing this
 *     request if known, otherwise 0.
 * @param[in] amt_written
 *     Amount of data written so far processing this request.
 *
 * @return
 *     The DSI should return GLOBUS_SUCCESS to allow processing to continue.
 *     Otherwise, the REST operation will be aborted and the result will
 *     be returned from the request.
 */
typedef
globus_result_t
(*globus_dsi_rest_progress_t) (
    void                               *progress_callback_arg,
    uint64_t                            total_read,
    uint64_t                            amt_read,
    uint64_t                            total_written,
    uint64_t                            amt_written);

typedef
struct globus_dsi_rest_callbacks_s
{
    /**
     * Pointer to the response handling callback function.
     * This function is called prior to reading the response body of the
     * HTTP request.
     */
    globus_dsi_rest_response_t          response_callback;
    /**
     * DSI-specific data that is passed to the response_callback.
     * */
    void                               *response_callback_arg;

    /**
     * Pointer to the operation complete callback function.
     * This function is called after the response callback and after
     * the response (if any) has been processed by the data_read_callback.
     */
    globus_dsi_rest_complete_t          complete_callback;
    /**
     * DSI-specific data that is passed to the complete_callback.
     */
    void                               *complete_callback_arg;

    /**
     * Pointer to the request body write callback function.
     * When set, this implies that the request will send a message body. This
     * function is called repeatedly until an error occurs processing the 
     * request, it returns no more data available, or returns an error.
     */
    globus_dsi_rest_write_t             data_write_callback;
    /**
     * DSI-specific data that is passed to the data_write_callback.
     */
    void                               *data_write_callback_arg;

    /**
     * Pointer to the response body read callback function pointer. This
     * function is called repeatedly until an error occurs or no more data is
     * available in the response.
     */
    globus_dsi_rest_read_t              data_read_callback;
    /**
     * DSI-specific data that is passed to the data_write_callback.
     */
    void                               *data_read_callback_arg;
    /**
     * Pointer to the response body read callback function pointer. This
     * function is called repeatedly until an error occurs or no more data is
     * available in the response.
     */
    globus_dsi_rest_progress_t          progress_callback;
    /**
     * DSI-specific data that is passed to the data_write_callback.
     */
    void                               *progress_callback_arg;
}
globus_dsi_rest_callbacks_t;

/**
 * @defgroup globus_dsi_rest_api API Functions
 */
/**
 * @brief Perform a REST request
 * @ingroup globus_dsi_rest_api
 * @details
 * @param[in] method
 *     The HTTP method to invoke for the resource. Typically one of "GET",
 *     "PUT", "POST", "PATCH", "DELETE", or "HEAD".
 * @param[in] uri
 *     The URI of the web resource to access.
 * @param[in] query_parameters
 *     Additional query parameters to append to the request. This may be
 *     NULL. If any query parameter names or values are NULL, they are ignored.
 * @param[in] headers
 *     Additional HTTP headers to append to the request. Other headers
 *     may be added to the request by the DSI REST Helper API.
 * @param[in] callbacks
 *     Callbacks to call when processing this request.
 */
globus_result_t
globus_dsi_rest_request(
    const char                         *method,
    const char                         *uri,
    const globus_dsi_rest_key_array_t  *query_parameters,
    const globus_dsi_rest_key_array_t  *headers,
    const globus_dsi_rest_callbacks_t  *callbacks);


/**
 * @brief Add query parameters to a URI base string
 * @ingroup globus_dsi_rest_api
 * @details
 *     Encodes the query_parameters key-value pairs using the HEX+ encoding
 *     for a URI query string, and appends the result to uri, storing the
 *     result in the storage pointed to by complete_urip.
 *
 * @param[in] uri
 *     The base URI string.
 * @param[in] query_parameters
 *     Pointer to the query parameters.
 * @param[out] complete_urip
 *     Pointer to store the resulting URI string in.
 * @return
 *     On success, return GLOBUS_SUCCESS and modifies complete_urip to point
 *     the new URI. Caller is responsible for freeing that string. Otherwise,
 *     return an error result.
 */
globus_result_t
globus_dsi_rest_uri_add_query(
    const char                         *uri,
    const globus_dsi_rest_key_array_t  *query_parameters,
    char                              **complete_urip);

/**
 * @brief Escape a string
 * @ingroup globus_dsi_rest_api
 * @details
 *     Encodes the s parameter the %HH encoding for non-alphanumeric characters
 *     and replacing ' ' with +.
 *
 * @param[in] s
 *     The original string.
 * @param[out] escaped
 *     Pointer to store the resulting escaped value in.
 * @return
 *     On success, return GLOBUS_SUCCESS and modifies escaped to point
 *     the new string. Caller is responsible for freeing that string.
 *     Otherwise, return an error result.
 */
globus_result_t
globus_dsi_rest_uri_escape(
    const char                         *s,
    char                              **escaped);

bool
globus_dsi_rest_error_is_retryable(
    globus_result_t                     result);

/**
 * @defgroup globus_dsi_rest_callback_specializations Callback Specializations
 */
/**
 * @brief Single-block write specialization data_write_callback_arg
 * @ingroup globus_dsi_rest_callback_specializations
 * @details
 *     A pointer to a data structure of this type must be be used as the
 *     data_write_callback_arg parameter when using the
 *     globus_dsi_rest_write_block() function as the data_write_callback
 *     to globus_dsi_rest_request().
 */
typedef
struct globus_dsi_rest_write_block_arg_s
{
    /** Data to send to the server */
    void                               *block_data;
    /** Length of the data */
    size_t                              block_len;
}
globus_dsi_rest_write_block_arg_t;

/**
 * @brief Single-block write specialization of globus_dsi_rest_write_t
 * @ingroup globus_dsi_rest_callback_specializations
 * @details
 *     This function implements the globus_dsi_rest_write_t interface
 *     and is intended to be used in the situation when the data to send to
 *     the REST server is contained in a single data array.
 * 
 *     The write_callback_arg used with this <b>MUST BE</b> a pointer to a
 *     globus_dsi_rest_write_block_arg_t.
 */
extern globus_dsi_rest_write_t const    globus_dsi_rest_write_block;

/**
 * @brief JSON write specialization of globus_dsi_rest_write_t
 * @ingroup globus_dsi_rest_callback_specializations
 * @details
 *     This function implements the globus_dsi_rest_write_t interface
 *     and is intended to be used in the situation when the data to send
 *     from the REST server is a json object. The callback will serialize the 
 *     json object and send it. The Content-Type header will be automatically
 *     set to application/json when this specialization is used, so it
 *     doesn't need to be set in the header explicitly.
 *
 *     The write_callback_arg used with this function <b>MUST BE</b> a json *.
 */
extern globus_dsi_rest_write_t const    globus_dsi_rest_write_json;

/**
 * @brief POST FORM write specialization of globus_dsi_rest_write_t
 * @ingroup globus_dsi_rest_callback_specializations
 * @details
 *     This function implements the globus_dsi_rest_write_t interface
 *     and is intended to be used in the situation when the data to send
 *     to the REST server is to be a application/x-www-form-urlencoded post body.
 *     
 *     The write_callback_arg used with this function <b>MUST BE</b> a pointer to a 
 *     globus_dsi_rest_key_array_t.
 */
extern globus_dsi_rest_write_t const    globus_dsi_rest_write_form;

/**
 * @brief GridFTP Operation write specialization data_write_callback_arg
 * @ingroup globus_dsi_rest_callback_specializations
 * @details
 *     A pointer to a data structure of this type must be be used as the
 *     data_write_callback_arg parameter when using the
 *     globus_dsi_rest_write_gridftp_op() function as the data_write_callback
 *     to globus_dsi_rest_request().
 */
typedef
struct globus_dsi_rest_gridftp_op_arg_s
{
    /** The GridFTP operation */
    globus_gfs_operation_t              op;
    /** The start offset of the data */
    globus_off_t                        offset;
    /**
     * The amount of data to read from the GridFTP op.
     * May be -1 to indicate all data. If this is not -1, then
     * the DSI must force ordered data on the gridftp op.
     */
    globus_off_t                        length;
    /**
     * This is set to true upon completion of the gridftp operation
     * if a registered read received a callback with eof=true
     */
    bool                                eof;
}
globus_dsi_rest_gridftp_op_arg_t;

/**
 * @brief GridFTP operation write specialization of globus_dsi_rest_write_t
 * @ingroup globus_dsi_rest_callback_specializations
 * @details
 *     This function implements the globus_dsi_rest_write_t interface
 *     and is intended to be used in the situation when the data to send
 *     to the REST server is coming from the data channels associated with
 *     a GridFTP DSI operation. This function will call
 *     globus_gridftp_server_register_read() and send the resulting data to
 *     the REST server.
 *
 *     The write_callback_arg used with this function
 *     <b>MUST BE</b> a pointer to a globus_dsi_rest_gridftp_op_arg_t.
 */
extern globus_dsi_rest_write_t const    globus_dsi_rest_write_gridftp_op;

/**
 * @brief Multipart write specialization data_write_callback_arg
 * @ingroup globus_dsi_rest_callback_specializations
 * @details
 *     A pointer to a data structure of this type must be be used as the
 *     data_write_callback_arg parameter when using the
 *     globus_dsi_rest_write_multipart() function as the data_write_callback
 *     to globus_dsi_rest_request().
 */
typedef
struct globus_dsi_rest_write_multipart_arg_s
{
    /** Number of parts for the multipart message */
    size_t                              num_parts;

    struct globus_dsi_rest_write_part_s
    {
        /**
         * A globus_dsi_rest_write_t function pointer to write the data for
         * this part.
         */
        globus_dsi_rest_write_t         data_write_callback;
        /**
         * An argument to pass to the data_write_callback
         * function pointer
         */
        void                           *data_write_callback_arg;
        /**
         * Array of num_parts elements, each containing a set of headers to add
         * to the beginning of the part message. Any element in the array may
         * contain an empty set of headers.
         */
        globus_dsi_rest_key_array_t     part_header;
    } *parts;
}
globus_dsi_rest_write_multipart_arg_t;

/**
 * @brief Multipart write specialization of globus_dsi_rest_write_t
 * @ingroup globus_dsi_rest_callback_specializations
 * @details
 *     This function implements the globus_dsi_rest_write_t interface
 *     and is intended to be used in the situation when the data to send
 *     to the REST server is multipart message.
 *
 *     The write_callback_arg used with this function
 *     <b>MUST BE</b> a pointer to a globus_dsi_rest_write_multipart_arg_t.
 *
 *     If the headers for the message contain a Content-Type
 *     type that is a multipart type, then the header is scanned for a boundary.
 *     If no Content-Type header is included, a multipart/related header is
 *     used and a boundary is created internally. If the Content-Type is
 *     included and is a non-multipart type, then no boundary is used, and the
 *     different parts are concatenated without any boundary.
 *
 *     The part_writer, part_writer_arg, and part_header arrays all contain
 *     num_parts elements. 
 *
 *     For each part, this function serializes the part_header array as
 *     a set of HTTP headers for the message part. The array may be empty,
 *     either to include no headers for a part, or because this message is
 *     not a multipart/related message.
 *
 *     Then, this function sends the data by calling the part_writer repeatedly,
 *     passing the part_writer_arg as normal. When that function returns 0
 *     bytes written or an error, then this function will (if defined) write
 *     the mulitpart boundary, and then continue to the next element in the
 *     array.
 *
 *     This function may be used to compose any of the other available write
 *     specializations into a multipart message.
 */
extern globus_dsi_rest_write_t const    globus_dsi_rest_write_multipart;

/**
 * @brief Multipart write specialization data_read_callback_arg
 * @ingroup globus_dsi_rest_callback_specializations
 * @details
 *     A pointer to a data structure of this type must be be used as the
 *     data_read_callback_arg parameter when using the
 *     globus_dsi_rest_read_multipart() function as the data_read_callback
 *     to globus_dsi_rest_request().
 */
typedef
struct globus_dsi_rest_read_multipart_arg_s
{
    /** Number of parts for the multipart message */
    size_t                              num_parts;

    struct globus_dsi_rest_read_part_s
    {
        /**
         * An (optional) pointer to a function to call with the
         * headers for this part. The code passed to this will be 0,
         * and the status will be NULL.
         */
        globus_dsi_rest_response_t      response_callback;
        /**
         * An argument to pass to the response_callback
         * funciton pointer. This may be NULL.
         */
        void                           *response_callback_arg;
        /**
         * A globus_dsi_rest_read_t function pointer to read the data for this
         * part.
         */
        globus_dsi_rest_read_t          data_read_callback;
        /**
         * An argument to pass to the data_read_callback 
         * function pointer. This may be NULL.
         */
        void                           *data_read_callback_arg;
    } *parts;
}
globus_dsi_rest_read_multipart_arg_t;

/**
 * @brief Multipart read specialization of globus_dsi_rest_read_t
 * @ingroup globus_dsi_rest_callback_specializations
 * @details
 *     This function implements the globus_dsi_rest_read_t interface
 *     and is intended to be used in the situation when the data to receive
 *     from the REST server is a multipart message.
 *
 *     The read_callback_arg passed to this function <b>MUST BE</b> a
 *     globus_dsi_rest_read_multipart_arg_t * cast to a void *. This function
 *     will cause the request to fail if the Content-Type of the response does
 *     not start with "multipart/".
 */
extern globus_dsi_rest_read_t const     globus_dsi_rest_read_multipart;

/**
 * @brief JSON read specialization of globus_dsi_rest_read_t
 * @ingroup globus_dsi_rest_callback_specializations
 * @details
 *     This function implements the globus_dsi_rest_read_t interface
 *     and is intended to be used in the situation when the data to receive
 *     from the REST server is a json object. The callback will parse the 
 *     data.
 *
 *     The read_callback_arg passed to this function <b>MUST BE</b> a
 *     json ** cast to a void *. This function will cause the request to
 *     fail if the Content-Type of the response is not "application/json" or if
 *     the content is not parseable as json.
 */
extern globus_dsi_rest_read_t const     globus_dsi_rest_read_json;

/**
 * @brief GridFTP operation read specialization of globus_dsi_rest_read_t
 * @ingroup globus_dsi_rest_callback_specializations
 * @details
 *     This function implements the globus_dsi_rest_read_t interface
 *     and is intended to be used in the situation when the data to receive
 *     from the REST server is to be sent to the data channels associated with
 *     a GridFTP DSI operation. This function will call
 *     globus_gridftp_server_register_write() and send the resulting data to
 *     the GridFTP data channel.
 *
 *     The read_callback_arg used with this function <b>MUST BE</b> a
 *     pointer to a globus_dsi_rest_gridftp_op_arg_t.
 */
extern globus_dsi_rest_read_t const     globus_dsi_rest_read_gridftp_op;

/**
 * @brief Idle timeout specialization of globus_dsi_rest_progress_t
 * @ingroup globus_dsi_rest_callback_specializations
 * @details
 *     This function implements the globus_dsi_rest_progress_t interface
 *     and is intended to be used to provide an idle timeout implementation.
 *
 *     The progress_callback_arg used with this function <b>MUST BE</b> a
 *     uintptr_t with its value meaning the number of milliseconds without
 *     any data read or written that triggers the timeout.
 */
extern globus_dsi_rest_progress_t const globus_dsi_rest_progress_idle_timeout;


/**
 * @brief Response specialization response_callback_arg
 * @ingroup globus_dsi_rest_callback_specializations
 * @details
 *     A pointer to a data structure of this type must be be used as the
 *     response_callback_arg parameter when using the
 *     globus_dsi_rest_response function as the response_callback
 *     to globus_dsi_rest_request().
 */
typedef
struct globus_dsi_rest_response_arg_s
{
    /** Value of the HTTP response code */
    int                                 response_code;
    /** Array of header names is updated by globus_dsi_rest_response with
     * their values if present, or NULL if not
     */
    globus_dsi_rest_key_array_t         desired_headers;
    /**
     * The number of bytes of the request body which were sent to the server.
     * This may not be the amount the server received if some network error
     * occurred.
     */
    off_t                               request_bytes_uploaded;
    /**
     * The number of bytes of the request body which were received from
     * the server. * This may not be the amount the server sent if some network
     * error occurred.
     */
    off_t                               response_bytes_downloaded;
}
globus_dsi_rest_response_arg_t;

/**
 * @brief Response parsing specialization of globus_dsi_rest_response_t
 * @ingroup globus_dsi_rest_callback_specializations
 * @details
 *     This function implements the globus_dsi_rest_response_t interface
 *     and is intended to be used to provide the HTTP status value and
 *     values of certain headers. This is intended to be used when
 *     a application wants to receive the response code and specific header
 *     values without having to implement callbacks.
 *
 *     The response_callback_arg used with this function <b>MUST BE</b> a
 *     pointer to a globus_dsi_rest_response_arg_t structure, with the
 *     desired_headers array initialized with a count and a key_value array
 *     containing only the key values initialized to the names of the headers
 *     that the application wants filled in.
 *
 *     When this function is used, the response_code will be set to the HTTP
 *     response code, and the values in the desired_headers.key_value array
 *     will be updated for each of the keys which was present in the HTTP
 *     response.
 */
extern globus_dsi_rest_response_t const globus_dsi_rest_response;

extern globus_module_descriptor_t       globus_i_dsi_rest_module;

#define GLOBUS_DSI_REST_MODULE (&globus_i_dsi_rest_module)

#ifdef __cplusplus
}
#endif

#endif /* GLOBUS_DSI_REST_H */
