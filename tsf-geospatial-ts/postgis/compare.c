/** @file
 * @brief postgis group
 *
 * Stored geometries against expected ones: polygons written to the
 * database come back (as EWKB) equal to the engine's originals; a row
 * nudged by 1 m in the database, and one deleted, are found - the
 * nudge within a 0.5 m tolerance as a difference, within 2 m not.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "postgis/compare"

#include "te_config.h"
#include "tapi_test.h"

#include "pg.h"

#define N 30

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    tapi_sql_conn *conn = NULL;
    tapi_geo_gen_item *items = NULL;
    tapi_geo_geometry *exp[N];
    long long ids[N];
    unsigned int n = 0, i;
    tapi_geo_pg_diff d;
    te_mi_logger *mi = NULL;

    TEST_START;
    memset(exp, 0, sizeof(exp));

    TEST_STEP("Connect to PostGIS through tsf-sql");
    TSAPI_GEO_SESSION(&sess, "pco_pg_cmp");
    PG_CONNECT(&sess, conn);
    mi = tsapi_mi_open();

    TEST_STEP("Store %d generated polygons", N);
    CHECK_RC(tapi_geo_generate(sess.pco, "polygons", "count=30\nseed=5\n"
                               "bounds=-50000 100000 50000 200000\n"
                               "vertices=9", &items, &n, NULL));
    CHECK_RC(tapi_sql_execute(conn, "DROP TABLE IF EXISTS shapes", NULL));
    CHECK_RC(tapi_sql_execute(conn, "CREATE TABLE shapes (id bigint PRIMARY "
                              "KEY, geom geometry(Polygon, 3763))", NULL));
    for (i = 0; i < n; i++)
    {
        te_string ew = TE_STRING_INIT;
        tapi_sql_value p[2];

        CHECK_RC(tapi_geo_parse_wkt(sess.pco, items[i].text, 3763, &exp[i]));
        CHECK_RC(tapi_geo_serialize(sess.pco, exp[i], TAPI_GEO_FMT_EWKB, -1,
                                    &ew));
        ids[i] = items[i].id;
        p[0] = tapi_sql_int(ids[i]);
        p[1] = tapi_sql_text(ew.ptr);
        CHECK_RC(tapi_sql_query_params(conn, "INSERT INTO shapes VALUES (?, "
                                       "ST_GeomFromEWKB(decode(?, 'hex')))",
                                       p, 2, NULL, NULL));
        te_string_free(&ew);
    }

    TEST_STEP("They come back unchanged");
    CHECK_RC(tapi_geo_postgis_compare(conn, sess.pco, "shapes", "id", "geom",
                                      ids, exp, n, 0, &d));
    tsapi_mi_put(mi, "different", (double)d.different);
    tsapi_mi_put(mi, "missing", (double)d.missing);
    if (d.compared != n || d.different != 0 || d.missing != 0)
        TEST_VERDICT("Round trip through PostGIS: %lld compared, %lld "
                     "different, %lld missing", d.compared, d.different,
                     d.missing);
    tapi_geo_pg_diff_free(&d);

    TEST_STEP("Nudge one row by 1 m, delete another");
    CHECK_RC(tapi_sql_execute_fmt(conn, NULL, "UPDATE shapes SET geom = "
                                  "ST_Translate(geom, 1, 0) WHERE id = %lld",
                                  ids[3]));
    CHECK_RC(tapi_sql_execute_fmt(conn, NULL, "DELETE FROM shapes WHERE id = "
                                  "%lld", ids[7]));
    CHECK_RC(tapi_geo_postgis_compare(conn, sess.pco, "shapes", "id", "geom",
                                      ids, exp, n, 0.5, &d));
    RING("tolerance 0.5 m: %lld different, %lld missing, max Hausdorff "
         "%.3f\n%s", d.different, d.missing, d.max_hausdorff,
         d.records.ptr != NULL ? d.records.ptr : "");
    tsapi_mi_put(mi, "different", (double)d.different);
    tsapi_mi_put(mi, "missing", (double)d.missing);
    if (d.different != 1 || d.missing != 1 ||
        !tsapi_geo_near(d.max_hausdorff, 1, 1e-6))
        TEST_VERDICT("After the change: %lld different, %lld missing",
                     d.different, d.missing);
    tapi_geo_pg_diff_free(&d);
    CHECK_RC(tapi_geo_postgis_compare(conn, sess.pco, "shapes", "id", "geom",
                                      ids, exp, n, 2, &d));
    tsapi_mi_put(mi, "different", (double)d.different);
    tsapi_mi_put(mi, "missing", (double)d.missing);
    tsapi_mi_graph(mi, "diff", "Different / missing: exact, 0.5 m, 2 m",
                   NULL, "different,missing");
    if (d.different != 0)
        TEST_VERDICT("A 1 m nudge is a difference within 2 m");
    tapi_geo_pg_diff_free(&d);

    TEST_SUCCESS;

cleanup:
    for (i = 0; i < N; i++)
        tapi_geo_geometry_free(sess.pco, exp[i]);
    tapi_geo_gen_items_free(items, n);
    if (conn != NULL)
    {
        tapi_sql_execute(conn, "DROP TABLE IF EXISTS shapes", NULL);
        tapi_sql_disconnect(conn);
    }
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
