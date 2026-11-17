/** @file
 * @brief postgis group
 *
 * The database and the agent's engine answer the same spatial question
 * the same way: each DE-9IM predicate over generated polygon pairs is
 * evaluated by PostGIS (ST_*) and by GEOS in the engine, and the
 * answers must agree pair by pair. A disagreement is a finding about
 * one of the two.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "postgis/predicates"

#include "te_config.h"
#include "tapi_test.h"

#include "pg.h"

int
main(int argc, char **argv)
{
    static const char *const preds[] = {
        "intersects", "contains", "within", "touches", "crosses", "overlaps",
        "disjoint", "equals", "covers", "covered_by"
    };
    static const char *const st[] = {
        "ST_Intersects", "ST_Contains", "ST_Within", "ST_Touches",
        "ST_Crosses", "ST_Overlaps", "ST_Disjoint", "ST_Equals", "ST_Covers",
        "ST_CoveredBy"
    };
    tsapi_geo_session sess = { 0 };
    tapi_sql_conn *conn = NULL;
    tapi_geo_gen_item *items = NULL;
    unsigned int n = 0, i, k, agree = 0;
    te_mi_logger *mi = NULL;

    TEST_START;

    TEST_STEP("Connect to PostGIS through tsf-sql");
    TSAPI_GEO_SESSION(&sess, "pco_pg_pred");
    PG_CONNECT(&sess, conn);
    mi = tsapi_mi_open();

    TEST_STEP("40 polygons in a small box: many pairs touch and overlap");
    CHECK_RC(tapi_geo_generate(sess.pco, "polygons", "count=40\nseed=13\n"
                               "bounds=0 0 30 30\nradius=5\nvertices=6",
                               &items, &n, NULL));
    for (k = 0; k < TE_ARRAY_LEN(preds); k++)
    {
        unsigned int truths = 0;

        for (i = 0; i + 1 < n; i++)
        {
            tapi_geo_geometry *a = NULL, *b = NULL;
            bool geos = false, pg = false;

            CHECK_RC(tapi_geo_parse_wkt(sess.pco, items[i].text, 3857, &a));
            CHECK_RC(tapi_geo_parse_wkt(sess.pco, items[i + 1].text, 3857,
                                        &b));
            CHECK_RC(tapi_geo_predicate(sess.pco, preds[k], a, b, NULL,
                                        &geos, NULL));
            CHECK_RC(tapi_geo_postgis_predicate(conn, sess.pco, st[k], a, b,
                                                &pg));
            if (geos != pg)
                TEST_VERDICT("%s and %s disagree on pair %u", preds[k], st[k],
                             i);
            agree++;
            truths += geos;
            tapi_geo_geometry_free(sess.pco, a);
            tapi_geo_geometry_free(sess.pco, b);
        }
        tsapi_mi_put(mi, "true_pairs", truths);
    }
    RING("%u answers agree", agree);
    tsapi_mi_graph(mi, "truth", "Pairs for which each predicate holds "
                   "(intersects ... covered_by)", NULL, "true_pairs");

    TEST_SUCCESS;

cleanup:
    tapi_geo_gen_items_free(items, n);
    if (conn != NULL)
        tapi_sql_disconnect(conn);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
