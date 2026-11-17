/** @file
 * @brief vector group
 *
 * A GeoJSON file written by hand - not by the module - with one defect
 * of each kind at a known feature, validated: each defect is found at
 * its feature, the clean features are left alone. The generator is not
 * involved, so this does not share a blind spot with it.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "vector/handmade"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo_dataset.h"
#include "tsapi_geo.h"

#define F(_id, _name, _pop, _geom) \
    "{\"type\":\"Feature\",\"properties\":{\"ident\":" #_id ",\"name\":"     \
    _name ",\"pop\":" #_pop "},\"geometry\":" _geom "}"
#define SQ(_x, _x2) \
    "{\"type\":\"Polygon\",\"coordinates\":[[[" #_x ",41],[" #_x2 ",41],["   \
    #_x2 ",41.1],[" #_x ",41.1],[" #_x ",41]]]}"

static const char *const doc =
    "{\"type\":\"FeatureCollection\",\"features\":["
    F(1, "\"clean\"", 100, SQ(-8.6, -8.5)) ","
    /* 2: bow-tie */
    F(2, "\"bowtie\"", 100, "{\"type\":\"Polygon\",\"coordinates\":[[[-8.5,"
      "41],[-8.4,41.1],[-8.4,41],[-8.5,41.1],[-8.5,41]]]}") ","
    /* 3: no geometry */
    F(3, "\"nogeom\"", 100, "null") ","
    /* 4: duplicate identifier of 1 */
    F(1, "\"dup\"", 100, SQ(-8.3, -8.2)) ","
    /* 5: a point where polygons belong */
    F(5, "\"point\"", 100, "{\"type\":\"Point\",\"coordinates\":[-8.2,41]}")
    ","
    /* 6: off the map */
    F(6, "\"far\"", 100, SQ(-80, -79.9)) ","
    /* 7: negative population */
    F(7, "\"negative\"", -5, SQ(-8.1, -8.0)) ","
    /* 8: no name */
    F(8, "null", 100, SQ(-8.0, -7.9)) ","
    F(9, "\"clean2\"", 7, SQ(-7.9, -7.8))
    "]}";

static const struct {
    const char *category;
    const char *fid;     /* the identifier in the report */
} expect[] = {
    { "invalid_geometry", "2" },
    { "missing_geometry", "3" },
    { "duplicate_id", "1" },
    { "unexpected_geometry_type", "5" },
    { "out_of_range_coordinate", "6" },
    { "invalid_attribute", "7" },
    { "invalid_attribute", "8" },
};

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    te_string path = TE_STRING_INIT;
    tapi_geo_dataset *ds = NULL;
    tapi_geo_ds_rules rules = { 0 };
    tapi_geo_ds_report rep;
    te_mi_logger *mi = NULL;
    unsigned int i;

    TEST_START;

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_vec_hand");
    mi = tsapi_mi_open();

    TEST_STEP("Write the hand-made GeoJSON and validate it");
    CHECK_RC(tsapi_geo_put(&sess, "hand.geojson", doc));
    te_string_append(&path, "%s/hand.geojson", sess.work);
    CHECK_RC(tapi_geo_dataset_open(sess.pco, path.ptr, NULL, &ds));
    rules.id_field = "ident";
    rules.expect_types = "Polygon,MultiPolygon";
    /* GDAL reads RFC 7946 GeoJSON as EPSG:4326 with its data in
     * longitude, latitude order. */
    rules.expect_crs = "EPSG:4326";
    rules.bounds = "-10 40 -7 42";
    rules.required = "name";
    rules.extra = "range.pop=0:1000000000";
    CHECK_RC(tapi_geo_dataset_validate(sess.pco, ds, &rules, &rep));
    RING("%lld features, %lld issues:\n%s", rep.features, rep.issues,
         rep.records.ptr != NULL ? rep.records.ptr : "");
    tsapi_mi_put(mi, "invalid_geometry", (double)rep.invalid_geometry);
    tsapi_mi_put(mi, "missing_geometry", (double)rep.missing_geometry);
    tsapi_mi_put(mi, "duplicate_id", (double)rep.duplicate_id);
    tsapi_mi_put(mi, "unexpected_type", (double)rep.unexpected_geometry_type);
    tsapi_mi_put(mi, "out_of_range", (double)rep.out_of_range_coordinate);
    tsapi_mi_put(mi, "invalid_attribute", (double)rep.invalid_attribute);
    tsapi_mi_graph(mi, "cat", "Issues per category", NULL,
                   "invalid_geometry,missing_geometry,duplicate_id,"
                   "unexpected_type,out_of_range,invalid_attribute");
    if (rep.features != 9)
        TEST_VERDICT("%lld features read, 9 written", rep.features);
    if (rep.crs_mismatch != 0)
        TEST_VERDICT("GeoJSON is not read as EPSG:4326 (%s)", rep.crs);
    if (rep.issues != (long long)TE_ARRAY_LEN(expect))
        TEST_VERDICT("%lld issues for %u planted defects", rep.issues,
                     (unsigned)TE_ARRAY_LEN(expect));
    for (i = 0; i < TE_ARRAY_LEN(expect); i++)
    {
        char needle[96];

        snprintf(needle, sizeof(needle), "\t%s\t%s\t", expect[i].fid,
                 expect[i].category);
        if (strstr(rep.records.ptr, needle) == NULL)
            TEST_VERDICT("No %s at feature %s", expect[i].category,
                         expect[i].fid);
    }
    tapi_geo_ds_report_free(&rep);

    TEST_SUCCESS;

cleanup:
    tapi_geo_dataset_close(sess.pco, ds);
    te_string_free(&path);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
