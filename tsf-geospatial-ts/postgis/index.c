/** @file
 * @brief postgis group
 *
 * Spatial indexes: without one the planner has nothing but a scan;
 * with a GiST index a bounding-box query uses it. Both cases on the same
 * 5000 polygons; the plan is logged.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "postgis/index"

#include "te_config.h"
#include "tapi_test.h"

#include "pg.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *index;
    tapi_sql_conn *conn = NULL;
    bool has = false, used = false;
    te_string plan = TE_STRING_INIT;
    te_mi_logger *mi = NULL;

    TEST_START;
    TEST_GET_STRING_PARAM(index);

    TEST_STEP("Connect to PostGIS through tsf-sql");
    TSAPI_GEO_SESSION(&sess, "pco_pg_index");
    PG_CONNECT(&sess, conn);
    mi = tsapi_mi_open();

    CHECK_RC(tapi_sql_execute(conn, "DROP TABLE IF EXISTS cells", NULL));
    CHECK_RC(tapi_sql_execute(conn, "CREATE TABLE cells (id int PRIMARY KEY, "
                              "geom geometry(Polygon, 3857))", NULL));
    CHECK_RC(tapi_sql_execute(conn, "INSERT INTO cells SELECT i, "
                              "ST_MakeEnvelope((i % 100) * 10, (i / 100) * "
                              "10, (i % 100) * 10 + 9, (i / 100) * 10 + 9, "
                              "3857) FROM generate_series(0, 4999) i", NULL));
    if (strcmp(index, "gist") == 0)
        CHECK_RC(tapi_sql_execute(conn, "CREATE INDEX cells_gix ON cells "
                                  "USING GIST (geom)", NULL));
    CHECK_RC(tapi_sql_execute(conn, "ANALYZE cells", NULL));

    TEST_STEP("Is there a spatial index, and is it used?");
    CHECK_RC(tapi_geo_postgis_index_check(conn, "cells", "geom", &has, &used,
                                          &plan));
    RING("index %d, used %d:\n%s", has, used, plan.ptr);
    tsapi_mi_put(mi, "has_index", has);
    tsapi_mi_put(mi, "index_used", used);
    tsapi_mi_graph(mi, "idx", "Index present, index used", NULL,
                   "has_index,index_used");
    if (strcmp(index, "gist") == 0 && (!has || !used))
        TEST_VERDICT("The GiST index is %s", has ? "not used" : "not found");
    if (strcmp(index, "none") == 0 && (has || used))
        TEST_VERDICT("An index is reported where there is none");

    TEST_SUCCESS;

cleanup:
    te_string_free(&plan);
    if (conn != NULL)
    {
        tapi_sql_execute(conn, "DROP TABLE IF EXISTS cells", NULL);
        tapi_sql_disconnect(conn);
    }
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
