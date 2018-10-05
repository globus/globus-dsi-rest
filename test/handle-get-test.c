#include "globus_i_dsi_rest.h"
#include <stdbool.h>

int
main()
{
    int rc = 0;
    globus_i_dsi_rest_request_t requests[10] = { { .handle = NULL } };
    size_t cases = sizeof(requests)/sizeof(requests[0]);

    printf("1..%zu\n", cases);
    globus_module_activate(GLOBUS_DSI_REST_MODULE);

    for (size_t i = 0; i < cases; i++)
    {
        bool ok = true;
        bool get_ok = true;
        bool distinct_ok = true;
        globus_result_t result = GLOBUS_SUCCESS;

        result = globus_i_dsi_rest_handle_get(&requests[i].handle, &requests[i]);

        if (result != GLOBUS_SUCCESS)
        {
            ok = false;
            get_ok = false;
        }

        for (size_t j = 0; j <= i; j++)
        {
            if (i != j && requests[i].handle == requests[j].handle)
            {
                ok = distinct_ok = false;
            }
        }

        printf("%s %zu%s%s%s\n",
            ok ? "ok" : "not ok",
            i+1,
            ok ? "" : " -",
            get_ok ? "" : " get_fail",
            distinct_ok ? "" : "distinct_fail");
        if (!ok)
        {
            rc++;
        }
    }
    for (size_t i = 0; i < cases; i++)
    {
        globus_i_dsi_rest_handle_release(requests[i].handle);
    }

    return rc;
}
/* main() */
