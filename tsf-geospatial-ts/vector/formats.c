/** @file
 * @brief vector group
 *
 * Every vector format the module supports reads back what was written
 * to it: a generated dataset (200 polygons with ids, names and values)
 * written through the driver under test, reopened, inspected (layers,
 * schema, count, extent, CRS) and compared feature by feature, by id,
 * with the same data written to GeoPackage. Formats the agent's GDAL
 * cannot write are skipped, not failed.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "vector/formats"

#include "te_config.h"
#include "tapi_test.h"

#include <math.h>

#include "tapi_geo_dataset.h"
#include "tsapi_geo.h"

#define GEN "count=200\nseed=8\nbounds=-9.5 37 -6.2 42\nvertices=7"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *driver;
    const char *ext;
    te_string path = TE_STRING_INIT;
    te_string master = TE_STRING_INIT;
    tapi_geo_dataset *ds = NULL;
    tapi_geo_dataset *ref = NULL;
    tapi_geo_layer *layers = NULL;
    tapi_geo_field *fields = NULL;
    unsigned int nl = 0, nf = 0;
    long long written = 0, count = 0;
    tapi_geo_bbox ext_box;
    tapi_geo_ds_cmp_opts opts = TAPI_GEO_DS_CMP_OPTS_DEFAULT;
    tapi_geo_ds_diff diff;
    char gtype[48];
    bool consistent;
    te_mi_logger *mi = NULL;
    double tol;

    TEST_START;
    TEST_GET_STRING_PARAM(driver);
    ext = strcmp(driver, "GPKG") == 0 ? "gpkg" :
          strcmp(driver, "ESRI Shapefile") == 0 ? "shp" :
          strcmp(driver, "FlatGeobuf") == 0 ? "fgb" :
          strcmp(driver, "KML") == 0 ? "kml" :
          strcmp(driver, "GML") == 0 ? "gml" :
          strcmp(driver, "GeoJSONSeq") == 0 ? "geojsons" : "geojson";
    /* Text formats print coordinates in decimal, and may round: they
     * get a nanodegree; binary formats must be exact. */
    tol = strcmp(driver, "KML") == 0 || strcmp(driver, "GML") == 0 ||
          strncmp(driver, "GeoJSON", 7) == 0 ? 1e-9 : 0;

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_vec_formats");
    if (!tapi_geo_driver_supported(sess.pco, driver, true))
        TEST_SKIP("The agent's GDAL cannot write %s", driver);
    mi = tsapi_mi_open();

    TEST_STEP("Write 200 polygons through %s, and to GeoPackage", driver);
    te_string_append(&path, "%s/f.%s", sess.work, ext);
    te_string_append(&master, "%s/master.gpkg", sess.work);
    CHECK_RC(tapi_geo_dataset_generate(sess.pco, path.ptr, driver,
                                       "EPSG:4326", "polygons", GEN, NULL,
                                       &written, NULL, NULL));
    CHECK_RC(tapi_geo_dataset_generate(sess.pco, master.ptr, "GPKG",
                                       "EPSG:4326", "polygons", GEN, NULL,
                                       NULL, NULL, NULL));
    if (written != 200)
        TEST_VERDICT("%s: %lld features written", driver, written);

    TEST_STEP("Reopen and inspect it");
    CHECK_RC(tapi_geo_dataset_open(sess.pco, path.ptr, NULL, &ds));
    RING("%s opened by driver %s in %.0f us", path.ptr, ds->driver,
         ds->open_us);
    tsapi_mi_put_type(mi, TE_MI_MEAS_LATENCY, "open_us", ds->open_us,
                      TE_MI_MEAS_MULTIPLIER_MICRO);
    CHECK_RC(tapi_geo_dataset_layers(sess.pco, ds, &layers, &nl,
                                     &consistent));
    if (nl < 1)
        TEST_VERDICT("%s has no layer", driver);
    CHECK_RC(tapi_geo_dataset_count(sess.pco, ds, NULL, true, &count));
    if (count != 200)
        TEST_VERDICT("%s read back %lld features, not 200", driver, count);
    CHECK_RC(tapi_geo_dataset_schema(sess.pco, ds, NULL, &fields, &nf, gtype,
                                     sizeof(gtype)));
    {
        unsigned int i;
        bool have_id = false, have_value = false;

        for (i = 0; i < nf; i++)
        {
            have_id = have_id || strcmp(fields[i].name, "id") == 0;
            have_value = have_value || strcmp(fields[i].name, "value") == 0;
        }
        if (!have_id || !have_value)
            TEST_VERDICT("%s lost the id or value field", driver);
    }
    CHECK_RC(tapi_geo_dataset_extent(sess.pco, ds, NULL, &ext_box));
    if (!(ext_box.minx >= -9.5 && ext_box.maxx <= -6.2 &&
          ext_box.miny >= 37 && ext_box.maxy <= 42))
        TEST_VERDICT("%s extent is outside the generated bounds", driver);
    if (strstr(layers[0].crs, "4326") == NULL)
        TEST_VERDICT("%s layer CRS is '%s'", driver, layers[0].crs);

    TEST_STEP("Compare with the GeoPackage copy, feature by id");
    CHECK_RC(tapi_geo_dataset_open(sess.pco, master.ptr, NULL, &ref));
    opts.id_field = "id";
    /* Topological equality: Shapefile rewinds rings clockwise. */
    opts.geometry = tol > 0 ? "hausdorff" : "topological";
    opts.tolerance = tol;
    opts.float_tolerance = 1e-9;
    /* KML has no typed attributes and GML may type them its own way:
     * compare the geometry and the identity there. */
    /* KML keeps no typed attributes of its own (name becomes the
     * placemark's): compare geometry and identity there. */
    opts.attributes = strcmp(driver, "KML") == 0 ? "none" :
                      strcmp(driver, "GML") == 0 ? "id,name,value" : "all";
    CHECK_RC(tapi_geo_dataset_compare(sess.pco, ref, ds, &opts, &diff));
    RING("matched %lld, only GPKG %lld, only %s %lld, geometry %lld, "
         "attributes %lld\n%s", diff.matched, diff.only_a, driver,
         diff.only_b, diff.geometry_diff, diff.attribute_diff,
         diff.records.ptr != NULL ? diff.records.ptr : "");
    tsapi_mi_put(mi, "matched", (double)diff.matched);
    tsapi_mi_put(mi, "geometry_diff", (double)diff.geometry_diff);
    tsapi_mi_put(mi, "attribute_diff", (double)diff.attribute_diff);
    tsapi_mi_graph(mi, "cmp", "Features matched / differing against the "
                   "GeoPackage copy", NULL,
                   "matched,geometry_diff,attribute_diff");
    tsapi_mi_graph_type(mi, "open", "Open time", NULL, TE_MI_MEAS_LATENCY,
                        "open_us");
    if (diff.matched != 200 || diff.only_a != 0 || diff.only_b != 0)
        TEST_VERDICT("%s: %lld of 200 features matched by id", driver,
                     diff.matched);
    if (diff.geometry_diff != 0)
        TEST_VERDICT("%s changed %lld geometries", driver,
                     diff.geometry_diff);
    if (diff.attribute_diff != 0 &&
        strcmp(driver, "ESRI Shapefile") != 0)
        TEST_VERDICT("%s changed %lld attribute values", driver,
                     diff.attribute_diff);
    tapi_geo_ds_diff_free(&diff);

    TEST_SUCCESS;

cleanup:
    free(layers);
    free(fields);
    tapi_geo_dataset_close(sess.pco, ds);
    tapi_geo_dataset_close(sess.pco, ref);
    te_string_free(&path);
    te_string_free(&master);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
