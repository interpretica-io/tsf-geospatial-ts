/** @file
 * @brief services group
 *
 * Features as a GDAL client sees them (WFS 2.0 of MapServer, OGC API -
 * Features of pygeoapi and of the control server): all 50 cells come,
 * with valid geometries, no duplicates, inside the extent the service
 * advertises. The control's invalid geometries and misplaced extent
 * must be caught.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "services/features"

#include "te_config.h"
#include "tapi_test.h"

#include "svc.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *target;
    svc_target tg = { .url = TE_STRING_INIT };
    tapi_geo_ogc_features f;
    te_mi_logger *mi = NULL;
    long long want;

    TEST_START;
    TEST_GET_STRING_PARAM(target);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_svc_feat");
    SVC_TARGET(&sess, target, &tg);
    mi = tsapi_mi_open();
    /* The real servers serve the 10 x 5 grid; the control server its 10
     * cells. */
    want = strcmp(tg.server, "control") == 0 ? 10 : 50;

    TEST_STEP("Read every feature of %s %s", tg.server, tg.service);
    CHECK_RC(tapi_geo_ogc_features_get(sess.pco, &tg.t, NULL, 0, 0, &f));
    RING("%lld features (%s), invalid %lld, duplicates %lld, outside the "
         "advertised extent %lld, %.0f us", f.features, f.crs, f.invalid,
         f.duplicates, f.outside_extent, f.elapsed_us);
    tsapi_mi_put(mi, "features", (double)f.features);
    tsapi_mi_put(mi, "invalid", (double)f.invalid);
    tsapi_mi_put(mi, "outside_extent", (double)f.outside_extent);
    tsapi_mi_put_type(mi, TE_MI_MEAS_LATENCY, "elapsed", f.elapsed_us,
                      TE_MI_MEAS_MULTIPLIER_MICRO);
    tsapi_mi_graph(mi, "feat", "Features, invalid, outside the extent", NULL,
                   "features,invalid,outside_extent");
    if (f.features != want)
        TEST_VERDICT("%lld features read, %lld served", f.features, want);
    if (strcmp(tg.mode, "good") == 0 && !f.ok)
        TEST_VERDICT("%s features have issues", tg.server);
    if (strcmp(tg.mode, "invalid_geom") == 0 && f.invalid != 3)
        TEST_VERDICT("%lld invalid geometries found of 3 served", f.invalid);
    if (strcmp(tg.mode, "bad_bbox") == 0 && f.outside_extent == 0)
        TEST_VERDICT("Features outside the advertised extent not caught");

    TEST_SUCCESS;

cleanup:
    te_string_free(&tg.url);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
