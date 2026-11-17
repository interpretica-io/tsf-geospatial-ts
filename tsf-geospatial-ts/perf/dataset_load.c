/** @file
 * @brief perf group
 *
 * Dataset loading: open time (cold, then warm with the file in the page
 * cache) and a full feature count of GeoPackage and GeoJSON files of
 * growing size; features per second against the feature count.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "perf/dataset_load"

#include "te_config.h"
#include "tapi_test.h"

#include <sys/time.h>

#include "tapi_geo_dataset.h"
#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    static const int sizes[] = { 1000, 10000, 100000 };
    tsapi_geo_session sess = { 0 };
    const char *driver;
    te_mi_logger *mi = NULL;
    unsigned int i;

    TEST_START;
    TEST_GET_STRING_PARAM(driver);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_perf_ds");
    mi = tsapi_mi_open();

    for (i = 0; i < TE_ARRAY_LEN(sizes); i++)
    {
        te_string path = TE_STRING_INIT;
        te_string g = TE_STRING_INIT;
        tapi_geo_dataset *ds = NULL;
        double cold, warm, secs;
        long long count = 0;
        struct timeval t0, t1;

        te_string_append(&path, "%s/l%d.%s", sess.work, sizes[i],
                         strcmp(driver, "GPKG") == 0 ? "gpkg" : "geojson");
        te_string_append(&g, "count=%d\nseed=1\nvertices=8\nbounds=-9 37 -6 "
                         "42", sizes[i]);
        CHECK_RC(tapi_geo_dataset_generate(sess.pco, path.ptr, driver,
                                           "EPSG:4326", "polygons", g.ptr,
                                           NULL, NULL, NULL, NULL));
        CHECK_RC(tsapi_geo_sh(&sess, "sync; cat %s > /dev/null", path.ptr));
        CHECK_RC(tapi_geo_dataset_open(sess.pco, path.ptr, NULL, &ds));
        cold = ds->open_us;
        tapi_geo_dataset_close(sess.pco, ds);
        CHECK_RC(tapi_geo_dataset_open(sess.pco, path.ptr, NULL, &ds));
        warm = ds->open_us;
        gettimeofday(&t0, NULL);
        CHECK_RC(tapi_geo_dataset_count(sess.pco, ds, NULL, true, &count));
        gettimeofday(&t1, NULL);
        secs = (double)(t1.tv_sec - t0.tv_sec) +
               (double)(t1.tv_usec - t0.tv_usec) / 1e6;
        if (count != sizes[i])
            TEST_VERDICT("%lld features counted of %d", count, sizes[i]);
        RING("%s, %d features: open %.0f / %.0f us, full scan %.3f s "
             "(%.0f features/s)", driver, sizes[i], cold, warm, secs,
             count / secs);
        tsapi_mi_put(mi, "features", sizes[i]);
        tsapi_mi_put_type(mi, TE_MI_MEAS_LATENCY, "open_first", cold,
                          TE_MI_MEAS_MULTIPLIER_MICRO);
        tsapi_mi_put_type(mi, TE_MI_MEAS_LATENCY, "open_again", warm,
                          TE_MI_MEAS_MULTIPLIER_MICRO);
        tsapi_mi_put(mi, "features_per_s", count / secs);
        tapi_geo_dataset_close(sess.pco, ds);
        te_string_free(&path);
        te_string_free(&g);
    }
    tsapi_mi_graph_type(mi, "open", "Open time vs features", "features",
                        TE_MI_MEAS_LATENCY, "open_first,open_again");
    tsapi_mi_graph(mi, "scan", "Full scan rate (features/s, including the "
                   "RPC)", "features", "features_per_s");

    TEST_SUCCESS;

cleanup:
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
