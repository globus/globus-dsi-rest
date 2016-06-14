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
        bool ok = true;
        char *form_data;
        globus_dsi_rest_key_array_t  key_array = 
        {
            .count = i+1,
            .key_value = test_cases
        };

        result = globus_i_dsi_rest_encode_form_data(
                &key_array,
                &form_data);
        if (result != GLOBUS_SUCCESS)
        {
            ok = false;
            goto skip_verify;
        }

        size_t j = 0;
        char *cp = strdup(form_data);
        for (char *oldt=form_data, *t = strpbrk(form_data, "&");
              oldt != NULL;
              oldt=t?t+1:NULL, t = strpbrk(oldt?oldt:"", "&"))
        {
            if (t)
            {
                *t = 0;
            }
            char *v = strpbrk(oldt, "=");
            if (v)
            {
                *(v++) = 0;
            }
            else
            {
                ok = false;
            }
            if (uri_decode(oldt) != GLOBUS_SUCCESS)
            {
                ok = false;
            }
            if (uri_decode(v) != GLOBUS_SUCCESS)
            {
                ok = false;
            }
            if (strcmp(test_cases[j].key, oldt) != 0 || strcmp(test_cases[j].value, v) != 0)
            {
                ok = false;
            }
            j++;
        }
skip_verify:
        printf("%s %zu - %s%s\n", ok?"ok":"not ok", i+1, test_names[i], cp);
        free(cp);
        free(form_data);
        if (!ok)
        {
            rc++;
        }
    }

    return rc;
}
/* main() */
