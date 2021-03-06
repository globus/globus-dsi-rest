#include "globus_i_dsi_rest.h"
#include <stdbool.h>

int
main()
{
    int rc = 0;
    globus_dsi_rest_key_value_t test_cases[] =
    {
        {
            .key = "n1",
            .value = "v1"
        },
        {
            .key = "n2",
            .value = "v2"
        },
        {
            .key = "n3",
            .value = "space in value"
        },
        {
            .key = "n4",
            .value = "nøn-áscîï"
        },
    };
    char *test_names[] = {
        "single",
        "double",
        "space in value",
        "non-ascii"
    };
    size_t num_cases = sizeof(test_cases)/sizeof(test_cases[0]);
    globus_dsi_rest_key_array_t test_array =
    {
        .count = 0,
        .key_value = test_cases
    };

    printf("1..%zu\n", num_cases);
    globus_module_activate(GLOBUS_DSI_REST_MODULE);

    for (size_t i = 0; i < num_cases; i++)
    {
        bool ok = true;
        bool set_ok = true;
        bool count_ok = true;
        size_t result_count = 0;
        globus_i_dsi_rest_request_t request = {.request_headers = NULL};
        test_array.count = i+1;

        globus_result_t result;
        result = globus_i_dsi_rest_compute_headers(
                &request.request_headers,
                &test_array);

        if (result != GLOBUS_SUCCESS)
        {
            ok = set_ok = false;
        }

        for (struct curl_slist *s = request.request_headers; s != NULL; s = s->next)
        {
            fprintf(stderr, "%s\n", s->data);
            result_count++;
        }
        if (result_count != i+1)
        {
            ok = count_ok = false;
        }
        curl_slist_free_all(request.request_headers);

        printf("%s %zu - %s%s%s\n",
            ok ? "ok" : "not ok",
            i+1,
            test_names[i],
            set_ok ? "" : " set_fail",
            count_ok ? "" : " count_fail");
        if (!ok)
        {
            rc++;
        }
    }

    return rc;
}
/* main() */
