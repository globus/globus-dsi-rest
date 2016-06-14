#include "globus_i_dsi_rest.h"
#include <stdbool.h>
#include "uri-decode.c"

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

    printf("1..%zu\n", num_cases);
    globus_module_activate(GLOBUS_DSI_REST_MODULE);

    for (size_t i = 0; i < num_cases; i++)
    {
        globus_result_t result = GLOBUS_SUCCESS;
        char *encoded_key=NULL, *encoded_value=NULL;
        bool ok = true;

        result = globus_dsi_rest_uri_escape(test_cases[i].key, &encoded_key);
        if (result != GLOBUS_SUCCESS)
        {
            ok = false;
        }
        result = globus_dsi_rest_uri_escape(test_cases[i].value, &encoded_value);
        if (result != GLOBUS_SUCCESS)
        {
            ok = false;
        }

        if (encoded_key)
        {
            uri_decode(encoded_key);
            if (strcmp(encoded_key, test_cases[i].key) != 0)
            {
                ok = false;
            }
        }
        if (encoded_value)
        {
            uri_decode(encoded_value);
            if (strcmp(encoded_value, test_cases[i].value) != 0)
            {
                ok = false;
            }
        }
        free(encoded_key);
        free(encoded_value);

        printf("%s %zu - %s\n", ok?"ok":"not ok", i+1, test_names[i]);
        if (!ok)
        {
            rc++;
        }
    }

    return rc;
}
/* main() */
