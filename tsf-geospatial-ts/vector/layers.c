/** @file
 * @brief vector group
 *
 * CRS consistency across a dataset: a GeoPackage whose two layers are
 * in different reference systems (the second added by GDAL's own
 * ogr2ogr, reprojected) is reported inconsistent, and a layer checked
 * against the wrong expected CRS is a mismatch.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "vector/layers"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo_dataset.h"
#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    te_string path = TE_STRING_INIT;
    tapi_geo_dataset *ds = NULL;
    tapi_geo_layer *layers = NULL;
    unsigned int n = 0;
    bool consistent = true;
    tapi_geo_ds_rules rules = { 0 };
    tapi_geo_ds_report rep;
    te_mi_logger *mi = NULL;

    TEST_START;

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_vec_layers");
    mi = tsapi_mi_open();

    TEST_STEP("A GeoPackage with a WGS 84 layer and a PT-TM06 copy of it");
    te_string_append(&path, "%s/two.gpkg", sess.work);
    CHECK_RC(tapi_geo_dataset_generate(sess.pco, path.ptr, "GPKG",
                                       "EPSG:4326", "points",
                                       "count=50\nseed=2\nbounds=-9 38 -7 41"
                                       "\nlayer_name=wgs84", NULL, NULL, NULL,
                                       NULL));
    rc = tsapi_geo_sh(&sess, "ogr2ogr -update -t_srs EPSG:3763 -nln tm06 "
                      "%s %s wgs84", path.ptr, path.ptr);
    if (rc != 0)
        TEST_SKIP("The agent has no ogr2ogr to add the second layer");

    TEST_STEP("The layers are listed and inconsistent");
    CHECK_RC(tapi_geo_dataset_open(sess.pco, path.ptr, NULL, &ds));
    CHECK_RC(tapi_geo_dataset_layers(sess.pco, ds, &layers, &n,
                                     &consistent));
    if (n != 2)
        TEST_VERDICT("%u layers listed, 2 expected", n);
    RING("Layers: %s (%s, %lld), %s (%s, %lld)", layers[0].name,
         layers[0].crs, layers[0].count, layers[1].name, layers[1].crs,
         layers[1].count);
    if (consistent)
        TEST_VERDICT("Layers in EPSG:4326 and EPSG:3763 are reported "
                     "consistent");

    TEST_STEP("Validation of each layer names the inconsistency");
    rules.layer = "tm06";
    rules.expect_crs = "EPSG:3763";
    CHECK_RC(tapi_geo_dataset_validate(sess.pco, ds, &rules, &rep));
    tsapi_mi_put(mi, "crs_mismatch", (double)rep.crs_mismatch);
    if (rep.crs_mismatch != 1)
        TEST_VERDICT("The layer in its expected CRS reported %lld CRS "
                     "issues, expected 1 (the other layer)",
                     rep.crs_mismatch);
    tapi_geo_ds_report_free(&rep);
    rules.expect_crs = "EPSG:4326";
    CHECK_RC(tapi_geo_dataset_validate(sess.pco, ds, &rules, &rep));
    tsapi_mi_put(mi, "crs_mismatch", (double)rep.crs_mismatch);
    if (rep.crs_mismatch != 2)
        TEST_VERDICT("The layer against the wrong CRS reported %lld CRS "
                     "issues, expected 2", rep.crs_mismatch);
    tapi_geo_ds_report_free(&rep);
    tsapi_mi_graph(mi, "crs", "CRS issues: right expectation, wrong one",
                   NULL, "crs_mismatch");

    TEST_SUCCESS;

cleanup:
    free(layers);
    tapi_geo_dataset_close(sess.pco, ds);
    te_string_free(&path);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
