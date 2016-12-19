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
#include "version.h"

const char *                            globus_i_dsi_rest_debug_level_names[] =
{
    [GLOBUS_DSI_REST_DATA]            = "DATA",
    [GLOBUS_DSI_REST_TRACE]           = "TRACE",
    [GLOBUS_DSI_REST_INFO]            = "INFO",
    [GLOBUS_DSI_REST_DEBUG]           = "DEBUG",
    [GLOBUS_DSI_REST_WARN]            = "WARN",
    [GLOBUS_DSI_REST_ERROR]           = "ERROR"
};

globus_mutex_t                          globus_i_dsi_rest_handle_cache_mutex;
size_t                                  globus_i_dsi_rest_handle_cache_index;
CURL                                   *globus_i_dsi_rest_handle_cache[GLOBUS_I_DSI_REST_HANDLE_CACHE_SIZE];
CURLSH                                 *globus_i_dsi_rest_share;
static globus_rw_mutex_t                globus_l_dsi_rest_share_share_lock;
static globus_rw_mutex_t                globus_l_dsi_rest_share_cookie_lock;
static globus_rw_mutex_t                globus_l_dsi_rest_share_dns_lock;
static globus_rw_mutex_t                globus_l_dsi_rest_share_ssl_lock;

static
struct globus_l_dsi_rest_write_lock_owners_s
{
    globus_thread_t                     share_lock_owner;
    globus_thread_t                     cookie_lock_owner;
    globus_thread_t                     dns_lock_owner;
    globus_thread_t                     ssl_lock_owner;
}
globus_l_dsi_rest_write_lock_owners;

static
void
globus_l_dsi_rest_share_lock(
    CURL                               *handle,
    curl_lock_data                      data,
    curl_lock_access                    access,
    void *                              userptr);

static
void
globus_l_dsi_rest_share_unlock(
    CURL                               *handle,
    curl_lock_data                      data,
    void *                              userptr);

GlobusDebugDefine(GLOBUS_DSI_REST);

static
int
globus_l_dsi_rest_activate(void)
{
    int rc;
    rc = curl_global_init(CURL_GLOBAL_ALL);
    if (rc != CURLE_OK)
    {
        goto fail;
    }
    rc = globus_module_activate(GLOBUS_COMMON_MODULE);
    if (rc != GLOBUS_SUCCESS)
    {
        goto activate_fail;
    }
    rc = globus_mutex_init(&globus_i_dsi_rest_handle_cache_mutex, NULL);
    if (rc != GLOBUS_SUCCESS)
    {
        goto mutex_init_fail;
    }
    rc = globus_rw_mutex_init(&globus_l_dsi_rest_share_share_lock, NULL);
    if (rc != GLOBUS_SUCCESS)
    {
        goto share_share_lock_init_fail;
    }
    rc = globus_rw_mutex_init(&globus_l_dsi_rest_share_cookie_lock, NULL);
    if (rc != GLOBUS_SUCCESS)
    {
        goto share_cookie_lock_init_fail;
    }
    rc = globus_rw_mutex_init(&globus_l_dsi_rest_share_dns_lock, NULL);
    if (rc != GLOBUS_SUCCESS)
    {
        goto share_dns_lock_init_fail;
    }
    rc = globus_rw_mutex_init(&globus_l_dsi_rest_share_ssl_lock, NULL);
    if (rc != GLOBUS_SUCCESS)
    {
        goto share_ssl_lock_init_fail;
    }
    globus_i_dsi_rest_share = curl_share_init();
    if (globus_i_dsi_rest_share == NULL)
    {
        rc = GLOBUS_FAILURE;
        goto share_init_fail;
    }
    rc = curl_share_setopt(
            globus_i_dsi_rest_share,
            CURLSHOPT_LOCKFUNC,
            globus_l_dsi_rest_share_lock);
    if (rc != CURLE_OK && rc != CURLSHE_BAD_OPTION)
    {
        goto share_setopt_fail;
    }
    rc = curl_share_setopt(
            globus_i_dsi_rest_share,
            CURLSHOPT_UNLOCKFUNC,
            globus_l_dsi_rest_share_unlock);
    if (rc != CURLE_OK && rc != CURLSHE_BAD_OPTION)
    {
        goto share_setopt_fail;
    }
    rc = curl_share_setopt(
            globus_i_dsi_rest_share,
            CURLSHOPT_USERDATA,
            &globus_l_dsi_rest_write_lock_owners);
    if (rc != CURLE_OK && rc != CURLSHE_BAD_OPTION)
    {
        goto share_setopt_fail;
    }
    rc = curl_share_setopt(
            globus_i_dsi_rest_share,
            CURLSHOPT_SHARE,
            CURL_LOCK_DATA_COOKIE);
    if (rc != CURLE_OK && rc != CURLSHE_BAD_OPTION)
    {
        goto share_setopt_fail;
    }
    rc = curl_share_setopt(
            globus_i_dsi_rest_share,
            CURLSHOPT_SHARE,
            CURL_LOCK_DATA_DNS);
    if (rc != CURLE_OK && rc != CURLSHE_BAD_OPTION)
    {
        goto share_setopt_fail;
    }
    rc = curl_share_setopt(
            globus_i_dsi_rest_share,
            CURLSHOPT_SHARE,
            CURL_LOCK_DATA_SSL_SESSION);
    if (rc != CURLE_OK && rc != CURLSHE_BAD_OPTION)
    {
        goto share_setopt_fail;
    }
    rc = GLOBUS_SUCCESS;
    globus_i_dsi_rest_handle_cache_index = 0;

    GlobusDebugInit(GLOBUS_DSI_REST, DATA TRACE INFO DEBUG WARN ERROR);

    if (rc != 0)
    {
share_setopt_fail:
        curl_share_cleanup(globus_i_dsi_rest_share);
share_init_fail:
        globus_rw_mutex_destroy(&globus_l_dsi_rest_share_ssl_lock);
share_ssl_lock_init_fail:
        globus_rw_mutex_destroy(&globus_l_dsi_rest_share_dns_lock);
share_dns_lock_init_fail:
        globus_rw_mutex_destroy(&globus_l_dsi_rest_share_cookie_lock);
share_cookie_lock_init_fail:
        globus_rw_mutex_destroy(&globus_l_dsi_rest_share_share_lock);
share_share_lock_init_fail:
        globus_mutex_destroy(&globus_i_dsi_rest_handle_cache_mutex);
mutex_init_fail:
        globus_module_deactivate(GLOBUS_COMMON_MODULE);
    }
activate_fail:
fail:
    return rc;
}
/* globus_l_dsi_rest_activate() */

static
int
globus_l_dsi_rest_deactivate(void)
{
    globus_mutex_lock(&globus_i_dsi_rest_handle_cache_mutex);
    while (globus_i_dsi_rest_handle_cache_index > 0)
    {
        curl_easy_cleanup(globus_i_dsi_rest_handle_cache[--globus_i_dsi_rest_handle_cache_index]);
    }
    globus_mutex_unlock(&globus_i_dsi_rest_handle_cache_mutex);
    curl_share_cleanup(globus_i_dsi_rest_share);

    globus_rw_mutex_destroy(&globus_l_dsi_rest_share_ssl_lock);
    globus_rw_mutex_destroy(&globus_l_dsi_rest_share_dns_lock);
    globus_rw_mutex_destroy(&globus_l_dsi_rest_share_cookie_lock);
    globus_rw_mutex_destroy(&globus_l_dsi_rest_share_share_lock);
    globus_mutex_destroy(&globus_i_dsi_rest_handle_cache_mutex);

    globus_module_deactivate(GLOBUS_COMMON_MODULE);
    curl_global_cleanup();

    GlobusDebugDestroy(GLOBUS_DSI_REST);
    return 0;
}
/* globus_l_dsi_rest_deactivate() */

static
void
globus_l_dsi_rest_share_lock(
    CURL                               *handle,
    curl_lock_data                      data,
    curl_lock_access                    access,
    void *                              userptr)
{
    struct globus_l_dsi_rest_write_lock_owners_s
                                       *owners = userptr;
    globus_rw_mutex_t                  *lock = NULL;
    globus_thread_t                    *owner = NULL;

    assert (data == CURL_LOCK_DATA_SHARE
            || data == CURL_LOCK_DATA_COOKIE
            || data == CURL_LOCK_DATA_DNS
            || data == CURL_LOCK_DATA_SSL_SESSION);
    assert (access == CURL_LOCK_ACCESS_SHARED
            || access == CURL_LOCK_ACCESS_SINGLE);

    switch (data)
    {
        case CURL_LOCK_DATA_SHARE:
            lock = &globus_l_dsi_rest_share_share_lock;
            owner = &owners->share_lock_owner;
            break;
        case CURL_LOCK_DATA_COOKIE:
            lock = &globus_l_dsi_rest_share_cookie_lock;
            owner = &owners->cookie_lock_owner;
            break;
        case CURL_LOCK_DATA_DNS:
            lock = &globus_l_dsi_rest_share_dns_lock;
            owner = &owners->dns_lock_owner;
            break;
        case CURL_LOCK_DATA_SSL_SESSION:
            lock = &globus_l_dsi_rest_share_ssl_lock;
            owner = &owners->ssl_lock_owner;
            break;
        default:
            return;
    }
    if (access == CURL_LOCK_ACCESS_SHARED)
    {
        globus_rw_mutex_readlock(lock);
    }
    else
    {
        globus_rw_mutex_writelock(lock);
        *owner = globus_thread_self();
    }
}
/* globus_l_dsi_rest_share_lock() */

static
void
globus_l_dsi_rest_share_unlock(
    CURL                               *handle,
    curl_lock_data                      data,
    void *                              userptr)
{
    struct globus_l_dsi_rest_write_lock_owners_s
                                       *owners = userptr;
    globus_rw_mutex_t                  *lock = NULL;
    globus_thread_t                    *owner = NULL;

    assert (data == CURL_LOCK_DATA_SHARE
            || data == CURL_LOCK_DATA_COOKIE
            || data == CURL_LOCK_DATA_DNS
            || data == CURL_LOCK_DATA_SSL_SESSION);

    switch (data)
    {
        case CURL_LOCK_DATA_SHARE:
            lock = &globus_l_dsi_rest_share_share_lock;
            owner = &owners->share_lock_owner;
            break;
        case CURL_LOCK_DATA_COOKIE:
            lock = &globus_l_dsi_rest_share_cookie_lock;
            owner = &owners->cookie_lock_owner;
            break;
        case CURL_LOCK_DATA_DNS:
            lock = &globus_l_dsi_rest_share_dns_lock;
            owner = &owners->dns_lock_owner;
            break;
        case CURL_LOCK_DATA_SSL_SESSION:
            lock = &globus_l_dsi_rest_share_ssl_lock;
            owner = &owners->ssl_lock_owner;
            break;
        default:
            return;
    }
    if (globus_thread_equal(*owner, globus_thread_self()))
    {
        memset(owner, 0, sizeof(globus_thread_t));
        globus_rw_mutex_writeunlock(lock);
    }
    else
    {
        globus_rw_mutex_readunlock(lock);
    }
}
/* globus_l_dsi_rest_share_unlock() */

globus_module_descriptor_t
globus_i_dsi_rest_module =
{
    .module_name = "globus_dsi_rest",
    .activation_func = globus_l_dsi_rest_activate,
    .deactivation_func = globus_l_dsi_rest_deactivate,
    .version = &local_version
};
