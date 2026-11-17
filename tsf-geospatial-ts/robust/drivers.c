/** @file
 * @brief robust group
 *
 * Untrusted files do not choose their driver. A VRT can point at any
 * file the agent can read (here a GeoPackage beside it - it could be
 * /etc/shadow), so the default driver allowlist refuses it; a caller
 * that wants it says drivers=any, or names its drivers. A file whose
 * extension lies is refused by a narrow allowlist.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "robust/drivers"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo_dataset.h"
#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    te_string gpkg = TE_STRING_INIT;
    te_string vrt = TE_STRING_INIT;
    te_string fake = TE_STRING_INIT;
    te_string doc = TE_STRING_INIT;
    tapi_geo_dataset *ds = NULL;
    long long count = 0;
    te_mi_logger *mi = NULL;

    TEST_START;

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_rob_drivers");
    mi = tsapi_mi_open();
    te_string_append(&gpkg, "%s/inner.gpkg", sess.work);
    te_string_append(&vrt, "%s/outer.vrt", sess.work);
    te_string_append(&fake, "%s/looks.geojson", sess.work);
    CHECK_RC(tapi_geo_dataset_generate(sess.pco, gpkg.ptr, "GPKG",
                                       "EPSG:4326", "points", "count=20",
                                       NULL, NULL, NULL, NULL));
    te_string_append(&doc, "<OGRVRTDataSource><OGRVRTLayer name=\"x\">"
                     "<SrcDataSource>%s</SrcDataSource><SrcLayer>features"
                     "</SrcLayer></OGRVRTLayer></OGRVRTDataSource>\n",
                     gpkg.ptr);
    CHECK_RC(tsapi_geo_put(&sess, "outer.vrt", doc.ptr));

    TEST_STEP("A VRT is refused by default");
    rc = tapi_geo_dataset_open(sess.pco, vrt.ptr, NULL, &ds);
    tsapi_mi_put(mi, "opened", rc == 0);
    if (rc == 0)
        TEST_VERDICT("A VRT opened with the default driver allowlist");
    if (TE_RC_GET_ERROR(rc) != TE_ECORRUPTED)
        TEST_VERDICT("The VRT was refused with %r", rc);

    TEST_STEP("It opens when the caller allows any driver");
    CHECK_RC(tapi_geo_dataset_open(sess.pco, vrt.ptr, "drivers=any", &ds));
    tsapi_mi_put(mi, "opened", 1);
    if (strcmp(ds->driver, "OGR_VRT") != 0)
        TEST_VERDICT("The VRT opened with driver %s", ds->driver);
    CHECK_RC(tapi_geo_dataset_count(sess.pco, ds, NULL, true, &count));
    if (count != 20)
        TEST_VERDICT("The VRT shows %lld features of 20", count);
    tapi_geo_dataset_close(sess.pco, ds);
    ds = NULL;

    TEST_STEP("A GeoPackage named .geojson is refused by a GeoJSON-only "
              "allowlist");
    CHECK_RC(tsapi_geo_sh(&sess, "cp %s %s", gpkg.ptr, fake.ptr));
    rc = tapi_geo_dataset_open(sess.pco, fake.ptr, "drivers=GeoJSON", &ds);
    tsapi_mi_put(mi, "opened", rc == 0);
    if (rc == 0)
        TEST_VERDICT("A GeoPackage opened through a GeoJSON-only list");
    CHECK_RC(tapi_geo_dataset_open(sess.pco, fake.ptr, NULL, &ds));
    if (strcmp(ds->driver, "GPKG") != 0)
        TEST_VERDICT("The file was taken for %s", ds->driver);
    tsapi_mi_graph(mi, "open", "Opened (1) / refused (0) per attempt", NULL,
                   "opened");

    TEST_SUCCESS;

cleanup:
    tapi_geo_dataset_close(sess.pco, ds);
    te_string_free(&gpkg);
    te_string_free(&vrt);
    te_string_free(&fake);
    te_string_free(&doc);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
