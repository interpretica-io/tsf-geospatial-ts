/** @file
 * @brief generation group
 *
 * Invalid geometries are only ever generated on request: without
 * allow_invalid=1 the invalid generator refuses, and no other generator
 * ever produces an invalid item, whatever its options (a spread of
 * seeds and shapes, every item validated).
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "generation/optin"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo.h"
#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    static const char *const shapes[] = {
        "vertices=3", "vertices=4\nholes=1", "vertices=64\nholes=1",
        "vertices=500", "vertices=5\nradius=0.0001",
    };
    tsapi_geo_session sess = { 0 };
    tapi_geo_gen_item *items = NULL;
    unsigned int n = 0;
    unsigned int seed, s, i, checked = 0;
    te_mi_logger *mi = NULL;

    TEST_START;

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_gen_optin");
    mi = tsapi_mi_open();

    TEST_STEP("The invalid generator refuses without the opt-in");
    rc = tapi_geo_generate(sess.pco, "invalid", "count=5", &items, &n, NULL);
    if (rc == 0)
        TEST_VERDICT("Invalid geometries were generated without "
                     "allow_invalid=1");
    if (TE_RC_GET_ERROR(rc) != TE_EINVAL)
        TEST_VERDICT("The refusal is %r", rc);

    TEST_STEP("Polygons of every shape and many seeds are all valid");
    for (s = 0; s < TE_ARRAY_LEN(shapes); s++)
    {
        unsigned int bad = 0;

        for (seed = 1; seed <= 8; seed++)
        {
            te_string p = TE_STRING_INIT;

            te_string_append(&p, "count=25\nseed=%u\n%s", seed, shapes[s]);
            CHECK_RC(tapi_geo_generate(sess.pco, "polygons", p.ptr, &items,
                                       &n, NULL));
            for (i = 0; i < n; i++)
            {
                tapi_geo_geometry *g = NULL;
                tapi_geo_validity v;

                CHECK_RC(tapi_geo_parse(sess.pco, "wkt", items[i].text, NULL,
                                        NULL, &g, NULL));
                CHECK_RC(tapi_geo_validate(sess.pco, g, "validity", NULL,
                                           &v));
                bad += !v.valid;
                checked++;
                tapi_geo_validity_free(&v);
                tapi_geo_geometry_free(sess.pco, g);
            }
            tapi_geo_gen_items_free(items, n);
            items = NULL;
            n = 0;
            te_string_free(&p);
        }
        tsapi_mi_put(mi, "invalid", bad);
        if (bad > 0)
            TEST_VERDICT("%u polygons of shape '%s' are invalid", bad,
                         shapes[s]);
    }
    RING("%u generated polygons checked, all valid", checked);
    tsapi_mi_graph(mi, "inv", "Invalid polygons per shape (must be 0)",
                   NULL, "invalid");

    TEST_SUCCESS;

cleanup:
    tapi_geo_gen_items_free(items, n);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
