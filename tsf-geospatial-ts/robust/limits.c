/** @file
 * @brief robust group
 *
 * Every configured limit holds: a geometry over max_vertices, nesting
 * over max_depth, input over max_input, more objects than max_handles,
 * a dataset pass over max_features, an operation past timeout_ms - each
 * refused with its own error, the engine usable afterwards.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "robust/limits"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo.h"
#include "tapi_geo_dataset.h"
#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *limit;
    te_string t = TE_STRING_INIT;
    tapi_geo_geometry *g = NULL;
    tapi_geo_geometry *keep[16];
    tapi_geo_dataset *ds = NULL;
    te_mi_logger *mi = NULL;
    te_errno want = TE_E2BIG;
    unsigned int i, nkeep = 0;

    TEST_START;
    TEST_GET_STRING_PARAM(limit);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_rob_limits");
    mi = tsapi_mi_open();
    memset(keep, 0, sizeof(keep));

    if (strcmp(limit, "max_vertices") == 0)
    {
        CHECK_RC(tapi_geo_config(sess.pco, "max_vertices=1000"));
        te_string_append(&t, "LINESTRING (0 0");
        for (i = 1; i < 1500; i++)
            te_string_append(&t, ", %u %u", i, i % 7);
        te_string_append(&t, ")");
        rc = tapi_geo_parse(sess.pco, "wkt", t.ptr, NULL, NULL, &g, NULL);
    }
    else if (strcmp(limit, "max_depth") == 0)
    {
        CHECK_RC(tapi_geo_config(sess.pco, "max_depth=4"));
        for (i = 0; i < 8; i++)
            te_string_append(&t, "GEOMETRYCOLLECTION (");
        te_string_append(&t, "POINT (0 0)");
        for (i = 0; i < 8; i++)
            te_string_append(&t, ")");
        rc = tapi_geo_parse(sess.pco, "wkt", t.ptr, NULL, NULL, &g, NULL);
    }
    else if (strcmp(limit, "max_input") == 0)
    {
        CHECK_RC(tapi_geo_config(sess.pco, "max_input=4096"));
        te_string_append(&t, "MULTIPOINT (");
        for (i = 0; i < 1000; i++)
            te_string_append(&t, "%s(%u 1)", i == 0 ? "" : ", ", i);
        te_string_append(&t, ")");
        rc = tapi_geo_parse(sess.pco, "wkt", t.ptr, NULL, NULL, &g, NULL);
    }
    else if (strcmp(limit, "max_handles") == 0)
    {
        tapi_geo_stats st;

        CHECK_RC(tapi_geo_stats_get(sess.pco, &st));
        te_string_append(&t, "max_handles=%u", st.live + 10);
        CHECK_RC(tapi_geo_config(sess.pco, t.ptr));
        for (nkeep = 0; nkeep < 10; nkeep++)
            CHECK_RC(tapi_geo_parse_wkt(sess.pco, "POINT (1 1)", 0,
                                        &keep[nkeep]));
        rc = tapi_geo_parse_wkt(sess.pco, "POINT (1 1)", 0, &g);
    }
    else if (strcmp(limit, "max_features") == 0)
    {
        te_string_append(&t, "%s/f.gpkg", sess.work);
        CHECK_RC(tapi_geo_dataset_generate(sess.pco, t.ptr, "GPKG",
                                           "EPSG:4326", "points",
                                           "count=500", NULL, NULL, NULL,
                                           NULL));
        CHECK_RC(tapi_geo_config(sess.pco, "max_features=100"));
        CHECK_RC(tapi_geo_dataset_open(sess.pco, t.ptr, NULL, &ds));
        {
            tapi_geo_ds_rules ru = { 0 };
            tapi_geo_ds_report rep;

            rc = tapi_geo_dataset_validate(sess.pco, ds, &ru, &rep);
            tapi_geo_ds_report_free(&rep);
        }
    }
    else if (strcmp(limit, "timeout") == 0)
    {
        tapi_geo_gen_item *items = NULL;
        unsigned int n = 0;

        /* A buffer of a 100000-vertex line with a fine arc: seconds of
         * work, stopped after 50 ms. */
        want = TE_ETIMEDOUT;
        te_string_append(&t, "LINESTRING (0 0");
        for (i = 1; i < 100000; i++)
            te_string_append(&t, ", %u %g", i, (i * 7919 % 1000) / 10.0);
        te_string_append(&t, ")");
        CHECK_RC(tapi_geo_parse(sess.pco, "wkt", t.ptr, NULL, NULL, &g,
                                NULL));
        {
            tapi_geo_geometry *b = NULL;

            rc = tapi_geo_construct(sess.pco, "buffer", g, NULL,
                                    "distance=3\nquad_segs=64\n"
                                    "timeout_ms=50", &b);
            tapi_geo_geometry_free(sess.pco, b);
        }
        tapi_geo_gen_items_free(items, n);
    }
    else
        TEST_FAIL("Unknown limit %s", limit);

    TEST_STEP("The %s limit refused the operation", limit);
    tsapi_mi_put(mi, "refused", rc != 0);
    tsapi_mi_graph(mi, "refused", "Refused (1) by the limit", NULL,
                   "refused");
    if (rc == 0)
        TEST_VERDICT("The %s limit did not hold", limit);
    if (TE_RC_GET_ERROR(rc) != (unsigned int)want)
        TEST_VERDICT("The %s limit refused with %r, expected %s", limit, rc,
                     te_rc_err2str(want));
    RING("Refused: %s", tapi_geo_last_error());

    TEST_STEP("The engine still works");
    CHECK_RC(tapi_geo_config(sess.pco, "max_vertices=10000000\nmax_depth=64"
                             "\nmax_input=268435456\nmax_features="
                             "100000000\nmax_handles=4095\ntimeout_ms=0"));
    {
        tapi_geo_geometry *ok = NULL;

        CHECK_RC(tapi_geo_parse_wkt(sess.pco, "POINT (2 2)", 0, &ok));
        tapi_geo_geometry_free(sess.pco, ok);
    }

    TEST_SUCCESS;

cleanup:
    for (i = 0; i < nkeep; i++)
        tapi_geo_geometry_free(sess.pco, keep[i]);
    tapi_geo_geometry_free(sess.pco, g);
    tapi_geo_dataset_close(sess.pco, ds);
    te_string_free(&t);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
