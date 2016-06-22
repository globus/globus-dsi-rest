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

#include "globus_gridftp_server.h"
#include "globus_dsi_rest.h"
#include "globus_xio_http.h"

#include <stdbool.h>

#define _gfs_name __func__

static
globus_version_t local_version =
{
    0, /* major version number */
    0, /* minor version number */
};


typedef
struct globus_l_dsi_rest_handle_s
{
    globus_xio_server_t                 xio_server;
    globus_xio_handle_t                 xio_handle;
    globus_xio_stack_t                  server_stack;
    globus_thread_t                     server_thread;
    globus_xio_driver_t                 tcp_driver;
    globus_xio_driver_t                 http_driver;
    char                               *contact_string;
    char                               *root;

    bool                                terminate;
    bool                                terminate_complete;

    globus_mutex_t                      mutex;
    globus_cond_t                       cond;
}
globus_l_dsi_rest_handle_t;

static
void *
globus_l_dsi_rest_thread(
    void                               *arg)
{
    globus_l_dsi_rest_handle_t         *dsi_rest_handle = arg;
    globus_xio_data_descriptor_t        descriptor;
    size_t                              buf_size = 256;
    unsigned char                      *buf = malloc(buf_size);
    globus_size_t                       nbytes;
    char                               *encoded_uri;

    globus_mutex_lock(&dsi_rest_handle->mutex);
    while (!dsi_rest_handle->terminate)
    {
        char                       *method;
        char                       *uri;
        globus_xio_http_version_t   http_version;
        globus_hashtable_t          headers;
        globus_result_t             result;

        globus_mutex_unlock(&dsi_rest_handle->mutex);
        result = globus_xio_server_accept(
                &dsi_rest_handle->xio_handle,
                dsi_rest_handle->xio_server);
        globus_mutex_lock(&dsi_rest_handle->mutex);

        if (result != GLOBUS_SUCCESS)
        {
            continue;
        }
        result = globus_xio_open(
                dsi_rest_handle->xio_handle,
                NULL,
                NULL);
        if (result != GLOBUS_SUCCESS)
        {
            goto end_this_socket;
        }

        result = globus_xio_data_descriptor_init(&descriptor, dsi_rest_handle->xio_handle);
        if (result != GLOBUS_SUCCESS)
        {
            goto end_this_socket;
        }

        result = globus_xio_read(
                dsi_rest_handle->xio_handle,
                buf,
                0,
                0,
                &nbytes,
                descriptor);

        if (result != GLOBUS_SUCCESS)
        {
            goto end_this_socket;
        }

        result = globus_xio_data_descriptor_cntl(
                descriptor,
                dsi_rest_handle->http_driver,
                GLOBUS_XIO_HTTP_GET_REQUEST,
                &method,
                &uri,
                &http_version,
                &headers);

        globus_dsi_rest_uri_escape(uri, &encoded_uri);

        char *uripath = globus_common_create_string("%s/%s", dsi_rest_handle->root, encoded_uri);
        if (strcmp(method, "GET") == 0)
        {
            int fd;
            fd = open(uripath, O_RDONLY);
            if (fd < 0)
            {
                globus_xio_handle_cntl(
                        dsi_rest_handle->xio_handle,
                        dsi_rest_handle->http_driver,
                        GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_HEADER,
                        "Connection",
                        "Close");
                globus_xio_handle_cntl(
                        dsi_rest_handle->xio_handle,
                        dsi_rest_handle->http_driver,
                        GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_HEADER,
                        "Content-Length",
                        "0");
                globus_xio_handle_cntl(
                        dsi_rest_handle->xio_handle,
                        dsi_rest_handle->http_driver,
                        GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_STATUS_CODE,
                        404);
            }
            else
            {
                globus_size_t read_amt = 0;
                globus_xio_handle_cntl(
                        dsi_rest_handle->xio_handle,
                        dsi_rest_handle->http_driver,
                        GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_HEADER,
                        "Connection",
                        "Close");
                globus_xio_handle_cntl(
                        dsi_rest_handle->xio_handle,
                        dsi_rest_handle->http_driver,
                        GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_STATUS_CODE,
                        200);

                do
                {
                    read_amt = read(fd, buf, buf_size);
                    if (read_amt > 0)
                    {
                        globus_size_t written_amt = 0;

                        while (written_amt < read_amt)
                        {
                            globus_size_t this_write;

                            result = globus_xio_write(
                                dsi_rest_handle->xio_handle,
                                buf+written_amt,
                                read_amt-written_amt,
                                read_amt-written_amt,
                                &this_write,
                                NULL);
                            if (this_write > 0)
                            {
                                written_amt += this_write;
                            }
                            else if (result != GLOBUS_SUCCESS)
                            {
                                break;
                            }
                        }
                    }
                }
                while (read_amt > 0);
                close(fd);
            }
        }
        else
        {
            int fd = open(uripath, O_WRONLY|O_CREAT, 0700);

            if (fd < 0)
            {
                globus_xio_handle_cntl(
                        dsi_rest_handle->xio_handle,
                        dsi_rest_handle->http_driver,
                        GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_STATUS_CODE,
                        500);
                globus_xio_handle_cntl(
                        dsi_rest_handle->xio_handle,
                        dsi_rest_handle->http_driver,
                        GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_HEADER,
                        "Content-Length",
                        "0");
                globus_xio_handle_cntl(
                        dsi_rest_handle->xio_handle,
                        dsi_rest_handle->http_driver,
                        GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_HEADER,
                        "Connection",
                        "Close");
            }
            else
            {
                off_t total_written=0;

                if (nbytes > 0)
                {
                    globus_size_t written_amt = 0;

                    while (written_amt < nbytes)
                    {
                        globus_size_t this_write;

                        this_write = write(fd, buf+written_amt, nbytes-written_amt);
                        if (this_write > 0)
                        {
                            written_amt += this_write;
                            total_written += this_write;
                        }
                    }
                }
                do
                {

                    result = globus_xio_read(
                        dsi_rest_handle->xio_handle,
                        buf,
                        buf_size,
                        1,
                        &nbytes,
                        NULL);
                    if (nbytes > 0)
                    {
                        globus_size_t written_amt = 0;

                        while (written_amt < nbytes)
                        {
                            globus_size_t this_write;

                            this_write = write(fd, buf+written_amt, nbytes-written_amt);
                            if (this_write > 0)
                            {
                                written_amt += this_write;
                                total_written += this_write;
                            }
                        }
                    }
                    if (result != GLOBUS_SUCCESS)
                    {
                        if (globus_error_match(
                                globus_error_peek(result),
                                GLOBUS_XIO_MODULE,
                                GLOBUS_XIO_ERROR_EOF)
                            || globus_xio_driver_error_match(
                                    dsi_rest_handle->http_driver,
                                    globus_error_peek(result),
                                    GLOBUS_XIO_HTTP_ERROR_EOF))
                        {
                            result = GLOBUS_SUCCESS;
                            break;
                        }
                        else
                        {
                            globus_xio_handle_cntl(
                                    dsi_rest_handle->xio_handle,
                                    dsi_rest_handle->http_driver,
                                    GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_STATUS_CODE,
                                    500);
                            globus_xio_handle_cntl(
                                    dsi_rest_handle->xio_handle,
                                    dsi_rest_handle->http_driver,
                                    GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_HEADER,
                                    "Content-Length",
                                    "0");
                            globus_xio_handle_cntl(
                                    dsi_rest_handle->xio_handle,
                                    dsi_rest_handle->http_driver,
                                    GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_HEADER,
                                    "Connection",
                                    "Close");
                            goto xio_error;
                        }
                    }
                }
                while (nbytes > 0);
                close(fd);
                globus_xio_handle_cntl(
                        dsi_rest_handle->xio_handle,
                        dsi_rest_handle->http_driver,
                        GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_STATUS_CODE,
                        204);
                globus_xio_handle_cntl(
                        dsi_rest_handle->xio_handle,
                        dsi_rest_handle->http_driver,
                        GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_HEADER,
                        "Content-Length",
                        "0");
                globus_xio_handle_cntl(
                        dsi_rest_handle->xio_handle,
                        dsi_rest_handle->http_driver,
                        GLOBUS_XIO_HTTP_HANDLE_SET_RESPONSE_HEADER,
                        "Connection",
                        "Close");
            }
        }
xio_error:
        free(uripath);
        result = globus_xio_handle_cntl(
                dsi_rest_handle->xio_handle,
                dsi_rest_handle->http_driver,
                GLOBUS_XIO_HTTP_HANDLE_SET_END_OF_ENTITY);
end_this_socket:
        result = globus_xio_close(
                dsi_rest_handle->xio_handle,
                NULL);
        dsi_rest_handle->xio_handle = NULL;
    }
    dsi_rest_handle->terminate_complete = true;
    globus_cond_signal(&dsi_rest_handle->cond);
    globus_mutex_unlock(&dsi_rest_handle->mutex);

    return NULL;
}
/* globus_l_dsi_rest_thread() */

/*************************************************************************
 *  start
 *  -----
 *  This function is called when a new session is initialized, ie a user 
 *  connects to the server.  This hook gives the dsi an opportunity to
 *  set internal state that will be threaded through to all other
 *  function calls associated with this session.  And an opportunity to
 *  reject the user.
 *
 *  finished_info.info.session.session_arg should be set to an DSI
 *  defined data structure.  This pointer will be passed as the void *
 *  user_arg parameter to all other interface functions.
 * 
 *  NOTE: at nice wrapper function should exist that hides the details 
 *        of the finished_info structure, but it currently does not.  
 *        The DSI developer should jsut follow this template for now
 ************************************************************************/
static
void
globus_l_dsi_rest_start(
    globus_gfs_operation_t              op,
    globus_gfs_session_info_t *         session_info)
{
    globus_l_dsi_rest_handle_t         *dsi_rest_handle;
    globus_result_t                     result = GLOBUS_SUCCESS;

    dsi_rest_handle = malloc(sizeof(globus_l_dsi_rest_handle_t));
    if (dsi_rest_handle == NULL)
    {
        result = GlobusGFSErrorMemory("dsi_rest_handle");
        goto handle_malloc_fail;
    }

    result = globus_xio_driver_load("tcp", &dsi_rest_handle->tcp_driver);
    if (result != GLOBUS_SUCCESS)
    {
        goto tcp_load_fail;
    }
    result = globus_xio_driver_load("http", &dsi_rest_handle->http_driver);
    if (result != GLOBUS_SUCCESS)
    {
        goto http_load_fail;
    }
    result = globus_xio_stack_init(&dsi_rest_handle->server_stack, NULL);
    if (result != GLOBUS_SUCCESS)
    {
        goto stack_init_fail;
    }

    globus_mutex_init(&dsi_rest_handle->mutex, NULL);
    globus_cond_init(&dsi_rest_handle->cond, NULL);

    dsi_rest_handle->terminate = false;
    dsi_rest_handle->terminate_complete = false;

    result = globus_xio_stack_push_driver(
            dsi_rest_handle->server_stack,
            dsi_rest_handle->tcp_driver);
    if (result != GLOBUS_SUCCESS)
    {
        goto stack_push_fail;
    }
    result = globus_xio_stack_push_driver(
            dsi_rest_handle->server_stack,
            dsi_rest_handle->http_driver);
    if (result != GLOBUS_SUCCESS)
    {
        goto stack_push_fail;
    }
    globus_xio_attr_t attr;
    result = globus_xio_attr_init(&attr);

    globus_reltime_t delay;
    GlobusTimeReltimeSet(delay, 1,0);

    result = globus_xio_attr_cntl(
            attr,
            NULL,
            GLOBUS_XIO_ATTR_SET_TIMEOUT_ACCEPT,
            NULL,
            &delay,
            NULL);

    result = globus_xio_server_create(
            &dsi_rest_handle->xio_server,
            attr,
            dsi_rest_handle->server_stack);
    if (result != GLOBUS_SUCCESS)
    {
        goto server_create_fail;
    }
    result = globus_xio_server_get_contact_string(
            dsi_rest_handle->xio_server,
            &dsi_rest_handle->contact_string);
    if (result != GLOBUS_SUCCESS)
    {
        goto get_contact_fail;
    }
    globus_gridftp_server_get_config_string(
            op,
            &dsi_rest_handle->root);
    if (dsi_rest_handle->root == NULL)
    {
        result = GlobusGFSErrorMemory("root");
        goto get_root_fail;
    }
    globus_thread_create(
            &dsi_rest_handle->server_thread,
            NULL,
            globus_l_dsi_rest_thread,
            dsi_rest_handle);

    if (result != GLOBUS_SUCCESS)
    {
get_root_fail:
        free(dsi_rest_handle->contact_string);
get_contact_fail:
        globus_xio_server_close(dsi_rest_handle->xio_server);
server_create_fail:
stack_push_fail:
        globus_xio_stack_destroy(dsi_rest_handle->server_stack);
stack_init_fail:
        globus_xio_driver_unload(dsi_rest_handle->http_driver);
http_load_fail:
        globus_xio_driver_unload(dsi_rest_handle->tcp_driver);
tcp_load_fail:
handle_malloc_fail:
        free(dsi_rest_handle);
        dsi_rest_handle = NULL;
    }

    globus_gridftp_server_finished_session_start(
            op,
            result,
            dsi_rest_handle,
            NULL,
            NULL);
}
/* globus_l_dsi_rest_start() */


/*************************************************************************
 *  destroy
 *  -------
 *  This is called when a session ends, ie client quits or disconnects.
 *  The dsi should clean up all memory they associated wit the session
 *  here. 
 ************************************************************************/
static
void
globus_l_dsi_rest_destroy(
    void *                              user_arg)
{
    globus_l_dsi_rest_handle_t         *dsi_rest_handle = user_arg;

    if (dsi_rest_handle)
    {
        globus_mutex_lock(&dsi_rest_handle->mutex);

        dsi_rest_handle->terminate = true;

        while (!dsi_rest_handle->terminate_complete)
        {
            globus_cond_wait(&dsi_rest_handle->cond, &dsi_rest_handle->mutex);
        }
        globus_mutex_unlock(&dsi_rest_handle->mutex);

        free(dsi_rest_handle->contact_string);
        globus_xio_server_close(dsi_rest_handle->xio_server);
        globus_xio_stack_destroy(dsi_rest_handle->server_stack);
        globus_xio_driver_unload(dsi_rest_handle->http_driver);
        globus_xio_driver_unload(dsi_rest_handle->tcp_driver);
        free(dsi_rest_handle);
    }
}

/*************************************************************************
 *  recv
 *  ----
 *  This interface function is called when the client requests that a
 *  file be transfered to the server.
 *
 *  To receive a file the following functions will be used in roughly
 *  the presented order.  They are described in more detail with the
 *  GridFTP server documentation.
 *
 *      globus_gridftp_server_begin_transfer();
 *      globus_gridftp_server_register_read();
 *      globus_gridftp_server_finished_transfer();
 *
 ************************************************************************/
static
void
globus_l_dsi_rest_recv(
    globus_gfs_operation_t              op,
    globus_gfs_transfer_info_t *        transfer_info,
    void *                              user_arg)
{
    globus_dsi_rest_gridftp_op_arg_t    gridftp_op_arg = {.op=op};
    globus_l_dsi_rest_handle_t         *dsi_rest_handle = user_arg;
    char                               *uri;
    globus_result_t                     result;

    uri = globus_common_create_string(
            "http://%s/%s",
            dsi_rest_handle->contact_string,
            transfer_info->pathname+1);

    if (uri == NULL)
    {
        result = GlobusGFSErrorMemory("globus_common_create_string");
        goto create_uri_fail;
    }

    globus_gridftp_server_begin_transfer(op, 0, NULL);
    do
    {
        globus_gridftp_server_get_write_range(
                gridftp_op_arg.op,
                &gridftp_op_arg.offset,
                &gridftp_op_arg.length);

        if (gridftp_op_arg.length != 0)
        {
            result = globus_dsi_rest_request(
                    "PUT",
                    uri,
                    NULL,
                    NULL,
                    &(globus_dsi_rest_callbacks_t)
                    {
                        .data_write_callback = globus_dsi_rest_write_gridftp_op,
                        .data_write_callback_arg = &gridftp_op_arg,
                    });
        }
    }
    while (gridftp_op_arg.length != 0 && gridftp_op_arg.length != -1);

create_uri_fail:
    globus_gridftp_server_finished_transfer(op, result);
    free(uri);
}

/*************************************************************************
 *  send
 *  ----
 *  This interface function is called when the client requests to receive
 *  a file from the server.
 *
 *  To send a file to the client the following functions will be used in roughly
 *  the presented order.  They are doced in more detail with the
 *  gridftp server documentation.
 *
 *      globus_gridftp_server_begin_transfer();
 *      globus_gridftp_server_register_write();
 *      globus_gridftp_server_finished_transfer();
 *
 ************************************************************************/
static
void
globus_l_dsi_rest_send(
    globus_gfs_operation_t              op,
    globus_gfs_transfer_info_t *        transfer_info,
    void *                              user_arg)
{
    globus_dsi_rest_gridftp_op_arg_t    gridftp_op_arg = {.op=op };
    globus_l_dsi_rest_handle_t         *dsi_rest_handle = user_arg;
    char                               *uri;
    globus_result_t                     result;

    uri = globus_common_create_string(
            "http://%s/%s",
            dsi_rest_handle->contact_string,
            transfer_info->pathname+1);

    if (uri == NULL)
    {
        result = GlobusGFSErrorMemory("globus_common_create_string");
        goto create_uri_fail;
    }

    globus_gridftp_server_begin_transfer(op, 0, NULL);

    do
    {
        globus_gridftp_server_get_read_range(
                gridftp_op_arg.op,
                &gridftp_op_arg.offset,
                &gridftp_op_arg.length);
        if (gridftp_op_arg.length != 0)
        {
            result = globus_dsi_rest_request(
                    "GET",
                    uri,
                    NULL,
                    NULL,
                    &(globus_dsi_rest_callbacks_t)
                    {
                        .data_read_callback = globus_dsi_rest_read_gridftp_op,
                        .data_read_callback_arg = &gridftp_op_arg,
                    });
        }
    }
    while (gridftp_op_arg.length != 0);

create_uri_fail:
    globus_gridftp_server_finished_transfer(op, result);
    free(uri);
}

static
int
globus_l_dsi_rest_activate(void);


static
int
globus_l_dsi_rest_deactivate(void);

/*
 *  no need to change this
 */
static
globus_gfs_storage_iface_t              globus_l_dsi_rest_dsi_iface = 
{
    .descriptor                       = GLOBUS_GFS_DSI_DESCRIPTOR_BLOCKING
                                      | GLOBUS_GFS_DSI_DESCRIPTOR_SENDER,
    .init_func                        = globus_l_dsi_rest_start,
    .destroy_func                     = globus_l_dsi_rest_destroy,
    .send_func                        = globus_l_dsi_rest_send,
    .recv_func                        = globus_l_dsi_rest_recv,
};

/*
 *  no need to change this
 */
GlobusExtensionDefineModule(globus_gridftp_server_dsi_rest) =
{
    "globus_gridftp_server_dsi_rest",
    globus_l_dsi_rest_activate,
    globus_l_dsi_rest_deactivate,
    NULL,
    NULL,
    &local_version
};

static
int
globus_l_dsi_rest_activate(void)
{
    int result = GLOBUS_SUCCESS;
    globus_module_activate(GLOBUS_DSI_REST_MODULE);
    globus_extension_registry_add(
        GLOBUS_GFS_DSI_REGISTRY,
        "dsi_rest",
        GlobusExtensionMyModule(globus_gridftp_server_dsi_rest),
        &globus_l_dsi_rest_dsi_iface);
    return result;
}

static
int
globus_l_dsi_rest_deactivate(void)
{
    globus_extension_registry_remove(
        GLOBUS_GFS_DSI_REGISTRY, "dsi_rest");
    globus_module_deactivate(GLOBUS_DSI_REST_MODULE);

    return 0;
}
