/** @file
 * @brief perf group
 *
 * Coordinate transformation throughput: N points WGS 84 -> PT-TM06 (or
 * OSGB 1936 / BNG, a datum change) per call, cold (operation selection,
 * the PROJ database) and warm. Points per second against N.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "perf/transform"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo.h"
#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    static const int counts[] = { 1, 100, 10000, 100000, 1000000 };
    tsapi_geo_session sess = { 0 };
    const char *pair;
    const char *dst;
    const char *bounds;
    te_mi_logger *mi = NULL;
    unsigned int i;

    TEST_START;
    TEST_GET_STRING_PARAM(pair);
    dst = strcmp(pair, "bng") == 0 ? "EPSG:27700" : "EPSG:3763";
    bounds = strcmp(pair, "bng") == 0 ? "-5 50 1 57" : "-9 37 -6.5 42";

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_perf_xform");
    mi = tsapi_mi_open();

    for (i = 0; i < TE_ARRAY_LEN(counts); i++)
    {
        te_string p = TE_STRING_INIT;
        tapi_geo_perf perf;

        te_string_append(&p, "src=EPSG:4326\ndst=%s\naxis=gis\ncount=%d\n"
                         "bounds=%s\niterations=%d", dst, counts[i], bounds,
                         counts[i] >= 100000 ? 3 : 20);
        CHECK_RC(tapi_geo_perf_run(sess.pco, "transform", NULL, NULL, "",
                                   p.ptr, &perf));
        RING("%d points: cold %.0f us, warm %.0f us, %.3g points/s",
             counts[i], perf.cold_us, perf.warm_us, perf.items_per_s);
        tsapi_mi_put(mi, "points", counts[i]);
        tsapi_mi_put(mi, "points_per_s", perf.items_per_s);
        tsapi_mi_put_type(mi, TE_MI_MEAS_LATENCY, "cold", perf.cold_us,
                          TE_MI_MEAS_MULTIPLIER_MICRO);
        tsapi_mi_put_type(mi, TE_MI_MEAS_LATENCY, "warm", perf.warm_us,
                          TE_MI_MEAS_MULTIPLIER_MICRO);
        if (!(perf.warm_us > 0) || !(perf.cold_us > 0))
            TEST_VERDICT("No time measured for %d points", counts[i]);
        te_string_free(&p);
    }
    tsapi_mi_graph(mi, "rate", "Transformation throughput (points/s)",
                   "points", "points_per_s");
    tsapi_mi_graph_type(mi, "lat", "Call latency, cold and warm", "points",
                        TE_MI_MEAS_LATENCY, "cold,warm");

    TEST_SUCCESS;

cleanup:
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
