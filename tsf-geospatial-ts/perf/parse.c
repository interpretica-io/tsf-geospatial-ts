/** @file
 * @brief perf group
 *
 * Geometry parsing throughput by input complexity: a polygon of N
 * vertices parsed in a loop on the agent (no RPC per parse), cold first
 * call and warm mean, for WKT, WKB and GeoJSON. Charted as vertices
 * per second against N.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "perf/parse"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo.h"
#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    static const int sizes[] = { 10, 100, 1000, 10000, 100000 };
    tsapi_geo_session sess = { 0 };
    const char *format;
    tapi_geo_format fmt;
    te_mi_logger *mi = NULL;
    unsigned int i;

    TEST_START;
    TEST_GET_STRING_PARAM(format);
    fmt = strcmp(format, "wkb") == 0 ? TAPI_GEO_FMT_WKB :
          strcmp(format, "geojson") == 0 ? TAPI_GEO_FMT_GEOJSON :
                                           TAPI_GEO_FMT_WKT;

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_perf_parse");
    mi = tsapi_mi_open();

    for (i = 0; i < TE_ARRAY_LEN(sizes); i++)
    {
        tapi_geo_gen_item *items = NULL;
        unsigned int n = 0;
        tapi_geo_geometry *g = NULL;
        te_string text = TE_STRING_INIT;
        te_string p = TE_STRING_INIT;
        tapi_geo_perf perf;
        int iters = sizes[i] >= 10000 ? 5 : 200;

        TEST_SUBSTEP("%d vertices", sizes[i]);
        te_string_append(&p, "count=1\nvertices=%d\nseed=1", sizes[i]);
        CHECK_RC(tapi_geo_generate(sess.pco, "polygons", p.ptr, &items, &n,
                                   NULL));
        CHECK_RC(tapi_geo_parse(sess.pco, "wkt", items[0].text, NULL, NULL,
                                &g, NULL));
        CHECK_RC(tapi_geo_serialize(sess.pco, g, fmt, -1, &text));
        te_string_reset(&p);
        te_string_append(&p, "iterations=%d", iters);
        CHECK_RC(tapi_geo_perf_run(sess.pco, "parse", NULL, NULL, text.ptr,
                                   p.ptr, &perf));
        RING("%s, %lld vertices: cold %.1f us, warm %.1f us, %.3g vertices/s",
             format, perf.work_items, perf.cold_us, perf.warm_us,
             perf.items_per_s);
        tsapi_mi_put(mi, "vertices", (double)perf.work_items);
        tsapi_mi_put_type(mi, TE_MI_MEAS_LATENCY, "cold", perf.cold_us,
                          TE_MI_MEAS_MULTIPLIER_MICRO);
        tsapi_mi_put_type(mi, TE_MI_MEAS_LATENCY, "warm", perf.warm_us,
                          TE_MI_MEAS_MULTIPLIER_MICRO);
        tsapi_mi_put(mi, "vertices_per_s", perf.items_per_s);
        if (perf.work_items != sizes[i] + 1)
            TEST_VERDICT("The parsed polygon has %lld vertices, %d + 1 "
                         "expected", perf.work_items, sizes[i]);
        tapi_geo_geometry_free(sess.pco, g);
        tapi_geo_gen_items_free(items, n);
        te_string_free(&text);
        te_string_free(&p);
    }
    tsapi_mi_graph_type(mi, "lat", "Parse latency vs vertices", "vertices",
                        TE_MI_MEAS_LATENCY, "cold,warm");
    tsapi_mi_graph(mi, "rate", "Parse throughput (vertices/s) vs vertices",
                   "vertices", "vertices_per_s");

    TEST_SUCCESS;

cleanup:
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
