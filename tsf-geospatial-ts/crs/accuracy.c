/** @file
 * @brief crs group
 *
 * A transformation says what it is. A conversion within one datum
 * (projecting) is exact; a datum change is not, and carries the
 * published accuracy of the operation PROJ chose; where PROJ knows no
 * operation at all it uses a ballpark (no datum shift), says so, and is
 * refused when ballparks are not allowed. When a better operation needs
 * a grid that is not installed, the result says it fell back.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "crs/accuracy"

#include "te_config.h"
#include "tapi_test.h"

#include <math.h>

#include "tapi_geo_crs.h"
#include "tapi_geo_assert.h"
#include "tsapi_geo.h"

static const struct {
    const char *pair;
    const char *src;
    const char *dst;
    double lon, lat;
    bool exact;
    bool ballpark;
    double max_accuracy;    /* m; < 0: unknown expected */
} cases[] = {
    { "conversion_bng", "EPSG:4277", "EPSG:27700", -1.5, 52.5, true, false,
      0 },
    { "conversion_webmerc", "EPSG:4326", "EPSG:3857", 10, 50, true, false,
      0 },
    { "helmert_etrs89", "EPSG:4258", "EPSG:4326", 10, 50, false, false, 1 },
    { "helmert_osgb36", "EPSG:4326", "EPSG:27700", -1.5, 52.5, false, false,
      2 },
    { "datum_nad27", "EPSG:4267", "EPSG:4269", -96, 38, false, false, 20 },
    { "ballpark", "EPSG:4326", "+proj=longlat +ellps=intl +type=crs", 10, 50,
      false, true, -1 },
};

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *pair;
    tapi_geo_xform_info xi;
    tapi_geo_xform_opts no_bp = TAPI_GEO_XFORM_OPTS_DEFAULT;
    double in[2], out[2];
    te_mi_logger *mi = NULL;
    unsigned int i;
    int c = -1;

    TEST_START;
    TEST_GET_STRING_PARAM(pair);
    for (i = 0; i < TE_ARRAY_LEN(cases); i++)
        if (strcmp(cases[i].pair, pair) == 0)
            c = (int)i;
    if (c < 0)
        TEST_FAIL("Unknown pair %s", pair);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_crs_acc");
    mi = tsapi_mi_open();

    TEST_STEP("Transform %s -> %s", cases[c].src, cases[c].dst);
    in[0] = cases[c].lon;
    in[1] = cases[c].lat;
    CHECK_RC(tapi_geo_transform_coords(sess.pco, cases[c].src, cases[c].dst,
                                       TAPI_GEO_AXIS_GIS, NULL, in, 1, 2,
                                       out, &xi));
    tapi_geo_xform_info_log(&xi);
    tsapi_mi_put(mi, "accuracy_m", xi.accuracy);
    tsapi_mi_put(mi, "best_candidate_m", xi.best_accuracy);
    tsapi_mi_put(mi, "candidates", xi.candidates);
    tsapi_mi_graph(mi, "acc", "Accuracy of the operation used and of the "
                   "best candidate (m), candidates", NULL,
                   "accuracy_m,best_candidate_m,candidates");
    if (xi.exact != cases[c].exact)
        TEST_VERDICT("%s is reported %sexact", pair, xi.exact ? "" : "not ");
    if (xi.ballpark != cases[c].ballpark)
        TEST_VERDICT("%s is %sreported a ballpark", pair,
                     xi.ballpark ? "" : "not ");
    if (cases[c].max_accuracy >= 0)
        TAPI_GEO_ASSERT(tapi_geo_assert_accuracy(&xi, cases[c].max_accuracy,
                                                 pair));
    else if (xi.accuracy >= 0)
        TEST_VERDICT("A ballpark claims an accuracy of %g m", xi.accuracy);
    if (xi.grids_missing > 0 && !xi.fallback)
        TEST_VERDICT("An operation with a missing grid is not a fallback");
    if (xi.fallback)
        RING("Fallback: a better operation needs a grid not installed "
             "(best candidate %.3f m, used %.3f m)", xi.best_accuracy,
             xi.accuracy);
    if (xi.operation[0] == '\0')
        TEST_VERDICT("The operation used is not named");

    TEST_STEP("Without ballparks the transformation is %s",
              cases[c].ballpark ? "refused" : "unchanged");
    no_bp.allow_ballpark = false;
    rc = tapi_geo_transform_coords(sess.pco, cases[c].src, cases[c].dst,
                                   TAPI_GEO_AXIS_GIS, &no_bp, in, 1, 2, out,
                                   &xi);
    if (cases[c].ballpark && rc == 0)
        TEST_VERDICT("A ballpark transformation ran with ballparks "
                     "disallowed");
    if (!cases[c].ballpark && rc != 0)
        TEST_VERDICT("%s failed with ballparks disallowed: %s", pair,
                     tapi_geo_last_error());

    TEST_SUCCESS;

cleanup:
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
