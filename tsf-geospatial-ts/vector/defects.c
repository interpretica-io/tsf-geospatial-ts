/** @file
 * @brief vector group
 *
 * Dataset validation finds exactly the defects that were planted: a
 * GeoPackage of 400 polygons with N defects of one kind at known
 * positions must report N in that category, and nothing else wrong.
 * Each planted feature is named in the report.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "vector/defects"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo_dataset.h"
#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *defect;
    int count;
    te_string path = TE_STRING_INIT;
    te_string plan = TE_STRING_INIT;
    tapi_geo_dataset *ds = NULL;
    tapi_geo_planted *planted = NULL;
    unsigned int n_planted = 0;
    tapi_geo_ds_rules rules = { 0 };
    tapi_geo_ds_report rep;
    long long got = -1, extra;
    te_mi_logger *mi = NULL;
    unsigned int i;

    TEST_START;
    TEST_GET_STRING_PARAM(defect);
    TEST_GET_INT_PARAM(count);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_vec_defects");
    mi = tsapi_mi_open();

    TEST_STEP("Write 400 polygons with %d planted %s", count, defect);
    te_string_append(&path, "%s/d.gpkg", sess.work);
    te_string_append(&plan, "%s:%d", defect, count);
    CHECK_RC(tapi_geo_dataset_generate(sess.pco, path.ptr, "GPKG",
                                       "EPSG:3763", "polygons",
                                       "count=400\nseed=4\nbounds=-100000 "
                                       "-200000 100000 200000",
                                       plan.ptr, NULL, &planted, &n_planted));
    if (n_planted != (unsigned int)count)
        TEST_VERDICT("%u defects planted, %d asked for", n_planted, count);

    TEST_STEP("Validate it");
    CHECK_RC(tapi_geo_dataset_open(sess.pco, path.ptr, NULL, &ds));
    rules.id_field = "id";
    rules.expect_types = "Polygon,MultiPolygon";
    rules.expect_crs = "EPSG:3763";
    rules.bounds = "-100000 -200000 100000 200000";
    rules.schema = "id:Integer64;name:String;value:Real;expect:String;t:Real";
    rules.strict_schema = true;
    rules.required = "name";
    rules.extra = "range.value=0:999";
    CHECK_RC(tapi_geo_dataset_validate(sess.pco, ds, &rules, &rep));
    RING("features %lld, issues %lld:\n%s", rep.features, rep.issues,
         rep.records.ptr != NULL ? rep.records.ptr : "");
    if (rep.features != 400)
        TEST_VERDICT("Validation saw %lld features", rep.features);

    got = strcmp(defect, "invalid") == 0 ? rep.invalid_geometry :
          strcmp(defect, "dup_id") == 0 ? rep.duplicate_id :
          strcmp(defect, "missing_geom") == 0 ? rep.missing_geometry :
          strcmp(defect, "empty_geom") == 0 ? rep.empty_geometry :
          strcmp(defect, "out_of_range") == 0 ? rep.out_of_range_coordinate :
          strcmp(defect, "wrong_type") == 0 ? rep.unexpected_geometry_type :
          strcmp(defect, "bad_attr") == 0 ? rep.invalid_attribute :
          strcmp(defect, "null_attr") == 0 ? rep.invalid_attribute : -1;
    tsapi_mi_put(mi, "planted", count);
    tsapi_mi_put(mi, "found", (double)got);
    tsapi_mi_put(mi, "issues", (double)rep.issues);
    tsapi_mi_graph(mi, "found", "Planted vs found vs all issues", NULL,
                   "planted,found,issues");
    if (got != count)
        TEST_VERDICT("%d planted %s, %lld found", count, defect, got);
    /* A point thrown off the map is both out of range and not a
     * polygon; every other defect is one issue. */
    extra = rep.issues - got -
            (strcmp(defect, "out_of_range") == 0 ? count : 0);
    if (extra != 0)
        TEST_VERDICT("Validation reported %lld issues beyond the planted "
                     "%s", extra, defect);
    if (rep.bad_features != count)
        TEST_VERDICT("%lld features flagged, %d planted", rep.bad_features,
                     count);

    TEST_STEP("Every planted feature is named in the report");
    for (i = 0; i < n_planted; i++)
    {
        char needle[64];

        snprintf(needle, sizeof(needle), "\t%lld\t", planted[i].id);
        if (rep.records.ptr == NULL || strstr(rep.records.ptr, needle) ==
                                       NULL)
            TEST_VERDICT("The planted %s at id %lld is not in the report",
                         defect, planted[i].id);
    }
    tapi_geo_ds_report_free(&rep);

    TEST_SUCCESS;

cleanup:
    free(planted);
    tapi_geo_dataset_close(sess.pco, ds);
    te_string_free(&path);
    te_string_free(&plan);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
