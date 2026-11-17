/** @file
 * @brief raster group
 *
 * A large raster is processed in tiles: statistics of a square GeoTIFF
 * growing to 8192 x 8192 doubles (512 MiB uncompressed) keep the RPC
 * server's peak memory near a tile's worth, whatever the image size.
 * Throughput and memory are charted against the size.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "raster/large"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo_raster.h"
#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    int size;
    te_string path = TE_STRING_INIT;
    te_string p = TE_STRING_INIT;
    tapi_geo_raster *r = NULL;
    tapi_geo_raster_stats st;
    tapi_geo_stats s0, s1;
    double mean;
    te_mi_logger *mi = NULL;

    TEST_START;
    TEST_GET_INT_PARAM(size);
    mean = 1 + (size - 1) / 2.0 + (size - 1) / 2.0;

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_ras_large");
    mi = tsapi_mi_open();

    TEST_STEP("Write a %d x %d Float64 gradient, deflate-compressed", size,
              size);
    te_string_append(&path, "%s/big.tif", sess.work);
    te_string_append(&p, "width=%d\nheight=%d\ntype=Float64\n"
                     "pattern=gradient\na=1\nbx=1\nby=1\nblock=512\n"
                     "co.COMPRESS=DEFLATE", size, size);
    CHECK_RC(tapi_geo_raster_write(sess.pco, path.ptr, p.ptr));

    TEST_STEP("Restart the RPC server for a clean memory baseline");
    CHECK_RC(rcf_rpc_server_restart(sess.pco));
    CHECK_RC(tapi_geo_config(sess.pco, "max_pixels=268435456"));
    CHECK_RC(tapi_geo_stats_get(sess.pco, &s0));

    TEST_STEP("Statistics in 512-pixel tiles");
    CHECK_RC(tapi_geo_raster_open(sess.pco, path.ptr, NULL, &r));
    CHECK_RC(tapi_geo_raster_statistics(sess.pco, r, 1, 512, &st));
    CHECK_RC(tapi_geo_stats_get(sess.pco, &s1));
    RING("%dx%d: mean %.17g (expected %.17g), %.1f Mpix/s, peak RSS %ld -> "
         "%ld KiB", size, size, st.mean, mean, st.mpix_per_s, s0.maxrss_kb,
         s1.maxrss_kb);
    if (!tsapi_geo_near(st.mean, mean, 1e-6 * mean))
        TEST_VERDICT("The mean of a %d-pixel gradient is wrong", size);
    tsapi_mi_put(mi, "size", size);
    tsapi_mi_put(mi, "mpix_per_s", st.mpix_per_s);
    tsapi_mi_put_type(mi, TE_MI_MEAS_MEMORY, "peak_rss_growth",
                      (double)(s1.maxrss_kb - s0.maxrss_kb),
                      TE_MI_MEAS_MULTIPLIER_KIBI);
    tsapi_mi_put_type(mi, TE_MI_MEAS_MEMORY, "image_size",
                      (double)size * size * 8 / 1024,
                      TE_MI_MEAS_MULTIPLIER_KIBI);
    tsapi_mi_graph_type(mi, "mem", "Peak RSS growth vs the image's "
                        "uncompressed size", "size", TE_MI_MEAS_MEMORY,
                        "peak_rss_growth,image_size");
    tsapi_mi_graph(mi, "rate", "Statistics throughput (Mpix/s)", "size",
                   "mpix_per_s");
    /* A tile is 2 MiB, GDAL's block cache is bounded (5% of RAM by
     * default, capped): growth must stay far below the image. */
    if ((double)(s1.maxrss_kb - s0.maxrss_kb) >
        64 * 1024 + (double)size * size * 8 / 1024 / 4)
        TEST_VERDICT("Memory grows with the image: tiles are not "
                     "streamed");

    TEST_SUCCESS;

cleanup:
    tapi_geo_raster_close(sess.pco, r);
    te_string_free(&path);
    te_string_free(&p);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
