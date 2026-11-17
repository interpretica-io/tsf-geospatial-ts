/** @file
 * @brief robust group
 *
 * Resources are released after success and after failure: 50 rounds of
 * making geometries, datasets and tracks - half of the rounds ending in
 * a failed operation - and freeing them return the engine to the same
 * number of live objects and keep the RPC server's memory flat
 * (charted per round).
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "robust/cleanup"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo.h"
#include "tapi_geo_dataset.h"
#include "tapi_geo_track.h"
#include "tsapi_geo.h"

#define ROUNDS 50

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    tapi_geo_stats s0, s;
    te_string path = TE_STRING_INIT;
    te_mi_logger *mi = NULL;
    long rss_first = 0;
    unsigned int r;

    TEST_START;

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_rob_cleanup");
    mi = tsapi_mi_open();
    te_string_append(&path, "%s/c.gpkg", sess.work);
    CHECK_RC(tapi_geo_dataset_generate(sess.pco, path.ptr, "GPKG",
                                       "EPSG:4326", "polygons", "count=200",
                                       NULL, NULL, NULL, NULL));
    CHECK_RC(tapi_geo_stats_get(sess.pco, &s0));

    for (r = 0; r < ROUNDS; r++)
    {
        tapi_geo_geometry *a = NULL, *b = NULL, *c = NULL, *bad = NULL;
        tapi_geo_dataset *ds = NULL;
        tapi_geo_track *t = NULL;
        tapi_geo_ds_rules ru = { 0 };
        tapi_geo_ds_report rep;

        CHECK_RC(tapi_geo_parse_wkt(sess.pco, "POLYGON ((0 0, 9 0, 9 9, 0 9, "
                                    "0 0))", 3857, &a));
        CHECK_RC(tapi_geo_buffer(sess.pco, a, 2, 16, &b));
        CHECK_RC(tapi_geo_intersection(sess.pco, a, b, &c));
        CHECK_RC(tapi_geo_dataset_open(sess.pco, path.ptr, NULL, &ds));
        CHECK_RC(tapi_geo_dataset_validate(sess.pco, ds, &ru, &rep));
        tapi_geo_ds_report_free(&rep);
        CHECK_RC(tapi_geo_track_generate(sess.pco, "count=100", &t));
        if (r % 2 == 1)
        {
            /* Operations that fail after their inputs exist. */
            if (tapi_geo_parse_wkt(sess.pco, "POLYGON ((0 0, 1 1", 0,
                                   &bad) == 0)
                TEST_VERDICT("A broken WKT parsed");
            if (tapi_geo_construct(sess.pco, "buffer", a, NULL,
                                   "distance=1\nno_such_option=1", &bad) == 0)
                TEST_VERDICT("An unknown option was accepted");
            tapi_geo_dataset *none = NULL;

            if (tapi_geo_dataset_open(sess.pco, "/nonexistent/x.gpkg", NULL,
                                      &none) == 0)
                TEST_VERDICT("A missing file opened");
        }
        CHECK_RC(tapi_geo_geometry_free(sess.pco, a));
        CHECK_RC(tapi_geo_geometry_free(sess.pco, b));
        CHECK_RC(tapi_geo_geometry_free(sess.pco, c));
        CHECK_RC(tapi_geo_dataset_close(sess.pco, ds));
        CHECK_RC(tapi_geo_track_free(sess.pco, t));
        CHECK_RC(tapi_geo_stats_get(sess.pco, &s));
        if (s.live != s0.live)
            TEST_VERDICT("Round %u left %d objects behind", r,
                         (int)s.live - (int)s0.live);
        if (r == 4)
            rss_first = s.rss_kb;
        tsapi_mi_put_type(mi, TE_MI_MEAS_MEMORY, "rss", (double)s.rss_kb,
                          TE_MI_MEAS_MULTIPLIER_KIBI);
    }
    tsapi_mi_graph_type(mi, "rss", "RPC server RSS per round", NULL,
                        TE_MI_MEAS_MEMORY, "rss");
    RING("RSS after round 5: %ld KiB, after round %d: %ld KiB; %llu objects "
         "made, %llu released", rss_first, ROUNDS, s.rss_kb,
         (unsigned long long)(s.created - s0.created),
         (unsigned long long)(s.released - s0.released));
    if (s.created - s0.created != s.released - s0.released)
        TEST_VERDICT("Objects made and released do not balance");
    /* Allocator caches settle in the first rounds; after that a leak of
     * even a few KiB per round shows over 45 rounds. */
    if (s.rss_kb - rss_first > 4096)
        TEST_VERDICT("RSS grew by %ld KiB over the rounds",
                     s.rss_kb - rss_first);

    TEST_SUCCESS;

cleanup:
    te_string_free(&path);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
