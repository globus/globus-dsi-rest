#include "globus_i_dsi_rest.h"
#include <stdbool.h>

int
main()
{
    char *encoded = NULL;
    int rc = 0;
    char *names[] = {
        "no-params",
        "all-safe-chars",
        "ascii-uri-chars",
        "control-chars",
        "non-ascii-uri-chars"
    };

    globus_dsi_rest_key_array_t         test_cases_array =
    {
        .count = 4,
        .key_value = (globus_dsi_rest_key_value_t[]) {
            {
                .key = "safekey",
                .value = "safeval",
            },
            {
                .key = "un safe&key",
                .value = "unsafe/value",
            },
            {
                .key = "tab-crlf",
                .value = "\t\r\n"
            },
            {
                .key = "ünsåfé ké¥",
                .value = "¨˜ßåƒ´√å¬¨´"
            },
        }
    };

    printf("1..%zu\n", test_cases_array.count+1);
    globus_module_activate(GLOBUS_DSI_REST_MODULE);

    for (test_cases_array.count = 0; test_cases_array.count <= 4; test_cases_array.count++)
    {
        bool ok = true;
        bool key_ok = true;
        bool val_ok = true;

        globus_dsi_rest_uri_add_query(
            "https://rest.example.org/resource",
            &test_cases_array,
            &encoded);

        if (test_cases_array.count == 0)
        {
            if (strchr(encoded, '?') != NULL)
            {
                ok = false;
                rc++;
            }
        }
        else
        {
            char *p = strchr(encoded, '?');
            if (p == NULL)
            {
                ok = false;
                rc++;
            }
            else
            {
                p++;
                for (size_t i = 0; p != NULL && i < test_cases_array.count; i++)
                {
                    char *k = p;
                    char *v = strchr(k, '=');
                    char *n = strchr(v, '&');
                    const char *origk = test_cases_array.key_value[i].key;
                    const char *origv = test_cases_array.key_value[i].value;
                    size_t o, e;

                    *(v++) = 0;
                    if (n)
                    {
                        *(n++) = 0;
                    }

                    o = 0;
                    e = 0;

                    while (origk[o] != 0 && k[e] != 0)
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
                        int kchar = (unsigned char) k[e++];
                        if (kchar == '%')
                        {
                            if (!isxdigit(k[e]))
                            {
                                ok = key_ok = false;
                                break;
                            }
                            kchar = unhex[(unsigned) k[e++]] << 4;
                            if (!isxdigit(k[e]))
                            {
                                ok = key_ok = false;
                                break;
                            }
                            kchar += unhex[(unsigned) k[e++]];
                        }
                        else if (kchar == '+')
                        {
                            kchar = ' ';
                        }
                        if (kchar != (unsigned char) origk[o])
                        {
                            ok = key_ok = false;
                        }
                        o++;
                    }
                    o = 0; 
                    e = 0;
                    while (origv[o] != 0 && v[e] != 0)
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
                        int vchar = (unsigned char) v[e++];
                        if (vchar == '%')
                        {
                            if (!isxdigit(v[e]))
                            {
                                ok = val_ok = false;
                                break;
                            }
                            vchar = unhex[(unsigned) v[e++]] << 4;
                            if (!isxdigit(v[e]))
                            {
                                ok = val_ok = false;
                                break;
                            }
                            vchar += unhex[(unsigned) v[e++]];
                        }
                        else if (vchar == '+')
                        {
                            vchar = ' ';
                        }
                        if (vchar != (unsigned char) origv[o])
                        {
                            ok = val_ok = false;
                        }
                        o++;
                    }

                    p = n;
                }
            }
            if (!ok)
            {
                rc++;
            }
        }

        printf("%s - %s%s%s\n", ok ? "ok" : "not ok",
                names[test_cases_array.count],
                key_ok ? "" : " key encoding error",
                val_ok ? "" : " value encoding error");
                
    }


    return rc;
}
/* main() */
