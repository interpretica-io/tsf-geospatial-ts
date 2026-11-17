/** @file
 * @brief postgis group
 *
 * What is stored in a spatial column: a table with a declared
 * geometry(Polygon, 4326) column - unconstrained by a check, as columns
 * often are in the field (typmod relaxed to geometry) - holds known bad
 * rows: invalid shapes, a NULL, an empty polygon, a row in another SRID,
 * a line. Each is counted, the invalid rows are named with PostGIS's
 * own reason, and the spatial columns are listed. A typed column next to
 * it holds nothing but its declared type and SRID.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "postgis/validate"

#include "te_config.h"
#include "tapi_test.h"

#include "pg.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    tapi_sql_conn *conn = NULL;
    tapi_geo_pg_validity v;
    tapi_geo_pg_column *cols = NULL;
    unsigned int ncols = 0, i;
    te_string ver = TE_STRING_INIT;
    te_mi_logger *mi = NULL;
    bool listed = false;

    TEST_START;

    TEST_STEP("Connect to PostGIS through tsf-sql");
    TSAPI_GEO_SESSION(&sess, "pco_pg_validate");
    PG_CONNECT(&sess, conn);
    mi = tsapi_mi_open();
    CHECK_RC(tapi_geo_postgis_version(conn, &ver));
    RING("PostGIS: %s", ver.ptr);

    TEST_STEP("A parcels table with known bad rows");
    CHECK_RC(tapi_sql_execute(conn, "DROP TABLE IF EXISTS parcels", NULL));
    CHECK_RC(tapi_sql_execute(conn, "CREATE TABLE parcels (id int PRIMARY "
                              "KEY, geom geometry(Polygon, 4326))", NULL));
    CHECK_RC(tapi_sql_execute(conn, "INSERT INTO parcels SELECT g, "
                              "ST_SetSRID(ST_MakeEnvelope(g, 0, g + 0.5, "
                              "0.5), 4326) FROM generate_series(1, 20) g",
                              NULL));
    CHECK_RC(tapi_sql_execute(conn, "INSERT INTO parcels VALUES (101, "
                              "'SRID=4326;POLYGON((0 0, 2 2, 2 0, 0 2, 0 0))'"
                              "), (102, 'SRID=4326;POLYGON((0 0, 4 0, 4 4, "
                              "0 4, 0 0), (5 5, 5 6, 6 6, 5 5))'), (103, "
                              "NULL), (104, 'SRID=4326;POLYGON EMPTY')",
                              NULL));
    /* Relax the column, as a migration that drops the typmod would, and
     * store what the typmod used to forbid. */
    CHECK_RC(tapi_sql_execute(conn, "ALTER TABLE parcels ALTER COLUMN geom "
                              "TYPE geometry", NULL));
    CHECK_RC(tapi_sql_execute(conn, "INSERT INTO parcels VALUES (105, "
                              "'SRID=3857;POLYGON((0 0, 1 0, 1 1, 0 1, 0 0))'"
                              "), (106, 'SRID=4326;LINESTRING(0 0, 1 1)')",
                              NULL));

    TEST_STEP("The column is listed");
    CHECK_RC(tapi_geo_postgis_columns(conn, &cols, &ncols));
    for (i = 0; i < ncols; i++)
        if (strcmp(cols[i].table, "parcels") == 0 &&
            strcmp(cols[i].column, "geom") == 0)
            listed = true;
    if (!listed)
        TEST_VERDICT("parcels.geom is not a listed geometry column");

    TEST_STEP("Validate the column");
    CHECK_RC(tapi_geo_postgis_validate(conn, "parcels", "geom", "id", &v));
    RING("rows %lld, null %lld, empty %lld, invalid %lld, other SRID %lld, "
         "other type %lld (declared %s, %d), SRIDs %lld\n%s", v.rows, v.null,
         v.empty, v.invalid, v.srid_mismatch, v.type_mismatch,
         v.declared_type, v.declared_srid, v.distinct_srids,
         v.invalid_samples.ptr != NULL ? v.invalid_samples.ptr : "");
    tsapi_mi_put(mi, "invalid", (double)v.invalid);
    tsapi_mi_put(mi, "null", (double)v.null);
    tsapi_mi_put(mi, "empty", (double)v.empty);
    tsapi_mi_put(mi, "distinct_srids", (double)v.distinct_srids);
    if (v.rows != 26 || v.null != 1 || v.empty != 1 || v.invalid != 2)
        TEST_VERDICT("Counted %lld rows, %lld NULL, %lld empty, %lld invalid",
                     v.rows, v.null, v.empty, v.invalid);
    if (v.distinct_srids != 2)
        TEST_VERDICT("%lld SRIDs in the column, 2 stored", v.distinct_srids);
    if (v.invalid_samples.ptr == NULL ||
        strstr(v.invalid_samples.ptr, "101\t") == NULL ||
        strstr(v.invalid_samples.ptr, "102\t") == NULL)
        TEST_VERDICT("The invalid rows are not named");
    tapi_geo_pg_validity_free(&v);

    TEST_STEP("A typed column holds only its type and SRID");
    CHECK_RC(tapi_sql_execute(conn, "DROP TABLE IF EXISTS parcels_typed",
                              NULL));
    CHECK_RC(tapi_sql_execute(conn, "CREATE TABLE parcels_typed (id int "
                              "PRIMARY KEY, geom geometry(Polygon, 4326))",
                              NULL));
    CHECK_RC(tapi_sql_execute(conn, "INSERT INTO parcels_typed SELECT id, "
                              "geom FROM parcels WHERE id <= 20", NULL));
    CHECK_RC(tapi_geo_postgis_validate(conn, "parcels_typed", "geom", "id",
                                       &v));
    if (strcasecmp(v.declared_type, "POLYGON") != 0 ||
        v.declared_srid != 4326 || v.srid_mismatch != 0 ||
        v.type_mismatch != 0 || v.invalid != 0 || v.rows != 20)
        TEST_VERDICT("The typed column: %s %d, %lld other SRID, %lld other "
                     "type, %lld invalid", v.declared_type, v.declared_srid,
                     v.srid_mismatch, v.type_mismatch, v.invalid);
    tsapi_mi_put(mi, "invalid", (double)v.invalid);
    tsapi_mi_put(mi, "null", (double)v.null);
    tsapi_mi_put(mi, "empty", (double)v.empty);
    tsapi_mi_put(mi, "distinct_srids", (double)v.distinct_srids);
    tapi_geo_pg_validity_free(&v);
    tsapi_mi_graph(mi, "pg", "Stored defects: relaxed column, typed column",
                   NULL, "invalid,null,empty,distinct_srids");

    TEST_SUCCESS;

cleanup:
    free(cols);
    te_string_free(&ver);
    if (conn != NULL)
    {
        tapi_sql_execute(conn, "DROP TABLE IF EXISTS parcels", NULL);
        tapi_sql_execute(conn, "DROP TABLE IF EXISTS parcels_typed", NULL);
        tapi_sql_disconnect(conn);
    }
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
