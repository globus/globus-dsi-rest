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
 * @file progress_idle_timeout.c GridFTP DSI REST Idle Timeout
 */
#endif

#include "globus_i_dsi_rest.h"

static
globus_result_t
globus_l_dsi_rest_progress_idle_timeout(
    void                               *progress_callback_arg,
    uint64_t                            total_read,
    uint64_t                            amt_read,
    uint64_t                            total_written,
    uint64_t                            amt_written)
{
    globus_i_dsi_rest_idle_arg_t       *idle_arg = progress_callback_arg;
    globus_abstime_t                    now;
    globus_reltime_t                    idle;
    uintptr_t                           idle_msec;

    GlobusTimeAbstimeGetCurrent(now);
    if (amt_read != 0 || amt_written != 0)
    {
        idle_arg->last_activity = now;
        return GLOBUS_SUCCESS;
    }
    GlobusTimeAbstimeDiff(idle, now, idle_arg->last_activity);

    idle_msec = idle.tv_sec * 1000 + idle.tv_usec / 1000;
    if (idle_msec > idle_arg->idle_timeout)
    {
        return GlobusDsiRestErrorTimeOut();
    }
    return GLOBUS_SUCCESS;
}
/* globus_l_dsi_rest_progess_idle_timeout() */

globus_dsi_rest_progress_t const        globus_dsi_rest_progress_idle_timeout
                                      = globus_l_dsi_rest_progress_idle_timeout;
