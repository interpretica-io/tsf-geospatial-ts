/** @file
 * @brief vector group
 *
 * Large datasets in bounded memory: validation and comparison stream
 * the features and keep a few bytes of hash per feature, so the RPC
 * server's peak memory grows far slower than the data. Charted against
 * the feature count, with the throughput.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "vector/large"

#include "te_config.h"
#include "tapi_test.h"

#include <stdlib.h>
#include <sys/time.h>

#include "tapi_geo_dataset.h"
#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    int features;
    te_string path = TE_STRING_INIT;
    te_string gen = TE_STRING_INIT;
    tapi_geo_dataset *ds = NULL;
    tapi_geo_ds_rules rules = { 0 };
    tapi_geo_ds_report rep;
    tapi_geo_stats s0, s1;
    long long written = 0;
    struct timeval t0, t1;
    double secs;
    long file_kb = 0;
    te_string du = TE_STRING_INIT;
    te_mi_logger *mi = NULL;

    TEST_START;
    TEST_GET_INT_PARAM(features);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_vec_large");
    mi = tsapi_mi_open();

    TEST_STEP("Write %d polygons", features);
    te_string_append(&path, "%s/large.gpkg", sess.work);
    te_string_append(&gen, "count=%d\nseed=3\nbounds=-180 -80 180 80\n"
                     "vertices=8\nradius=0.01", features);
    CHECK_RC(tapi_geo_config(sess.pco, "max_features=10000000"));
    CHECK_RC(tapi_geo_dataset_generate(sess.pco, path.ptr, "GPKG",
                                       "EPSG:4326", "polygons", gen.ptr,
                                       "dup_id:3", &written, NULL, NULL));
    if (written != features)
        TEST_VERDICT("%lld features written", written);
    if (tsapi_geo_sh_out(&sess, &du, "du -k %s | cut -f1", path.ptr) == 0)
        file_kb = atol(du.ptr);

    TEST_STEP("Restart the RPC server for a clean memory baseline");
    CHECK_RC(rcf_rpc_server_restart(sess.pco));
    CHECK_RC(tapi_geo_stats_get(sess.pco, &s0));

    TEST_STEP("Validate all %d features", features);
    CHECK_RC(tapi_geo_dataset_open(sess.pco, path.ptr, NULL, &ds));
    rules.id_field = "id";
    rules.expect_types = "Polygon";
    gettimeofday(&t0, NULL);
    CHECK_RC(tapi_geo_dataset_validate(sess.pco, ds, &rules, &rep));
    gettimeofday(&t1, NULL);
    secs = (double)(t1.tv_sec - t0.tv_sec) +
           (double)(t1.tv_usec - t0.tv_usec) / 1e6;
    CHECK_RC(tapi_geo_stats_get(sess.pco, &s1));
    RING("%lld features validated in %.2f s (%.0f/s); peak RSS %ld -> %ld "
         "KiB for a %ld KiB file", rep.features, secs, rep.features / secs,
         s0.maxrss_kb, s1.maxrss_kb, file_kb);
    if (rep.features != features || rep.duplicate_id != 3 ||
        rep.issues != 3)
        TEST_VERDICT("Validation of the large dataset: %lld features, %lld "
                     "issues", rep.features, rep.issues);
    tsapi_mi_put(mi, "features", features);
    tsapi_mi_put_type(mi, TE_MI_MEAS_MEMORY, "peak_rss_growth",
                      (double)(s1.maxrss_kb - s0.maxrss_kb),
                      TE_MI_MEAS_MULTIPLIER_KIBI);
    tsapi_mi_put_type(mi, TE_MI_MEAS_MEMORY, "file_size", (double)file_kb,
                      TE_MI_MEAS_MULTIPLIER_KIBI);
    tsapi_mi_put_type(mi, TE_MI_MEAS_UNITLESS_VALUE, "features_per_s",
                      rep.features / secs, TE_MI_MEAS_MULTIPLIER_PLAIN);
    tsapi_mi_graph_type(mi, "mem", "Peak RSS growth vs file size",
                        "features", TE_MI_MEAS_MEMORY,
                        "peak_rss_growth,file_size");
    tsapi_mi_graph(mi, "rate", "Validation throughput (features/s)",
                   "features", "features_per_s");
    /* Bounded: under 64 bytes per feature (the hash table) plus 64 MiB
     * of fixed costs, and far less than the data itself for large
     * datasets. */
    if (s1.maxrss_kb - s0.maxrss_kb > (long)features * 64 / 1024 + 65536)
        TEST_VERDICT("Validation memory grows with the data");
    tapi_geo_ds_report_free(&rep);

    TEST_SUCCESS;

cleanup:
    tapi_geo_dataset_close(sess.pco, ds);
    te_string_free(&path);
    te_string_free(&gen);
    te_string_free(&du);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
