/** @file
 * @brief perf group
 *
 * Spatial predicate latency: does a polygon of N vertices contain a
 * point, asked repeatedly, plain and prepared (an index built once).
 * Preparing must pay for complex polygons; both are charted against N.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "perf/predicate"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo.h"
#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    static const int sizes[] = { 16, 256, 4096, 65536 };
    tsapi_geo_session sess = { 0 };
    te_mi_logger *mi = NULL;
    tapi_geo_geometry *pt = NULL;
    double last_plain = 0, last_prep = 0;
    unsigned int i;

    TEST_START;

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_perf_pred");
    mi = tsapi_mi_open();
    CHECK_RC(tapi_geo_parse_wkt(sess.pco, "POINT (0.3 0.2)", 0, &pt));

    for (i = 0; i < TE_ARRAY_LEN(sizes); i++)
    {
        tapi_geo_gen_item *items = NULL;
        unsigned int n = 0;
        tapi_geo_geometry *poly = NULL;
        te_string p = TE_STRING_INIT;
        tapi_geo_perf plain, prep;

        te_string_append(&p, "count=1\nvertices=%d\nseed=2\nbounds=-1 -1 1 1"
                         "\nradius=1", sizes[i]);
        CHECK_RC(tapi_geo_generate(sess.pco, "polygons", p.ptr, &items, &n,
                                   NULL));
        CHECK_RC(tapi_geo_parse_wkt(sess.pco, items[0].text, 0, &poly));
        /* The plain predicate rebuilds the polygon's topology on every
         * call (quadratic here): fewer rounds for the big ones. */
        CHECK_RC(tapi_geo_perf_run(sess.pco, "predicate", poly, pt, NULL,
                                   sizes[i] > 4096 ?
                                   "predicate=contains\niterations=4" :
                                   sizes[i] > 256 ?
                                   "predicate=contains\niterations=30" :
                                   "predicate=contains\niterations=200",
                                   &plain));
        CHECK_RC(tapi_geo_perf_run(sess.pco, "prepared_predicate", poly, pt,
                                   NULL, "predicate=contains\n"
                                   "iterations=200", &prep));
        RING("%d vertices: plain %.2f us, prepared %.2f us (cold %.1f us)",
             sizes[i], plain.warm_us, prep.warm_us, prep.cold_us);
        tsapi_mi_put(mi, "vertices", sizes[i]);
        tsapi_mi_put_type(mi, TE_MI_MEAS_LATENCY, "plain", plain.warm_us,
                          TE_MI_MEAS_MULTIPLIER_MICRO);
        tsapi_mi_put_type(mi, TE_MI_MEAS_LATENCY, "prepared", prep.warm_us,
                          TE_MI_MEAS_MULTIPLIER_MICRO);
        last_plain = plain.warm_us;
        last_prep = prep.warm_us;
        tapi_geo_geometry_free(sess.pco, poly);
        tapi_geo_gen_items_free(items, n);
        te_string_free(&p);
    }
    tsapi_mi_graph_type(mi, "lat", "contains() latency vs polygon vertices",
                        "vertices", TE_MI_MEAS_LATENCY, "plain,prepared");
    if (!(last_prep < last_plain))
        TEST_VERDICT("A prepared polygon of 65536 vertices is not faster "
                     "than a plain one");

    TEST_SUCCESS;

cleanup:
    tapi_geo_geometry_free(sess.pco, pt);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
