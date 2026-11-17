/** @file
 * @brief raster group
 *
 * Band statistics, computed tile by tile, against closed forms: the
 * gradient a + bx*x + by*y has its minimum, maximum, mean and standard
 * deviation in closed form; a checker board of 0 and hi has mean hi/2
 * when its cells tile the image; a constant has no spread. Every tile
 * size gives the same answer; the time each took is charted.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "raster/statistics"

#include "te_config.h"
#include "tapi_test.h"

#include <math.h>

#include "tapi_geo_raster.h"
#include "tsapi_geo.h"

#define W 640
#define H 480

int
main(int argc, char **argv)
{
    static const int tiles[] = { 16, 37, 64, 256, 1024 };
    tsapi_geo_session sess = { 0 };
    const char *pattern;
    te_string path = TE_STRING_INIT;
    te_string p = TE_STRING_INIT;
    tapi_geo_raster *r = NULL;
    double mn, mx, mean, sd;
    te_mi_logger *mi = NULL;
    unsigned int i;

    TEST_START;
    TEST_GET_STRING_PARAM(pattern);

    if (strcmp(pattern, "gradient") == 0)
    {
        /* v = 5 + 2x + 3y: x and y independent uniforms on 0..W-1,
         * 0..H-1; var(x) = (W^2 - 1) / 12. */
        mn = 5;
        mx = 5 + 2.0 * (W - 1) + 3.0 * (H - 1);
        mean = 5 + 2.0 * (W - 1) / 2 + 3.0 * (H - 1) / 2;
        sd = sqrt(4.0 * (W * W - 1) / 12 + 9.0 * (H * H - 1) / 12);
    }
    else if (strcmp(pattern, "checker") == 0)
    {
        /* 16-pixel cells, 40 x 30 of them: half are 0, half 200. */
        mn = 0;
        mx = 200;
        mean = 100;
        sd = 100;
    }
    else
    {
        mn = mx = mean = 42;
        sd = 0;
    }

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_ras_stats");
    mi = tsapi_mi_open();

    TEST_STEP("Write a %dx%d %s GeoTIFF", W, H, pattern);
    te_string_append(&path, "%s/s.tif", sess.work);
    te_string_append(&p, "width=%d\nheight=%d\ntype=Float64\npattern=%s\n"
                     "a=%s\nbx=2\nby=3\nhi=200\ncell=16\nblock=128", W, H,
                     pattern, strcmp(pattern, "constant") == 0 ? "42" : "5");
    CHECK_RC(tapi_geo_raster_write(sess.pco, path.ptr, p.ptr));
    CHECK_RC(tapi_geo_raster_open(sess.pco, path.ptr, NULL, &r));

    for (i = 0; i < TE_ARRAY_LEN(tiles); i++)
    {
        tapi_geo_raster_stats st;

        TEST_SUBSTEP("Statistics in %d-pixel tiles", tiles[i]);
        CHECK_RC(tapi_geo_raster_statistics(sess.pco, r, 1, tiles[i], &st));
        RING("tile %d: min %.17g max %.17g mean %.17g sd %.17g, %.1f Mpix/s",
             tiles[i], st.min, st.max, st.mean, st.stddev, st.mpix_per_s);
        if (st.valid != W * H || st.nodata != 0)
            TEST_VERDICT("%lld valid pixels of %d", st.valid, W * H);
        if (st.min != mn || st.max != mx)
            TEST_VERDICT("min/max %.17g/%.17g, expected %g/%g", st.min,
                         st.max, mn, mx);
        if (!tsapi_geo_near(st.mean, mean, 1e-9 * fmax(1, mean)) ||
            !tsapi_geo_near(st.stddev, sd, 1e-9 * fmax(1, sd)))
            TEST_VERDICT("mean/sd %.17g/%.17g, expected %.17g/%.17g",
                         st.mean, st.stddev, mean, sd);
        tsapi_mi_put(mi, "tile", tiles[i]);
        tsapi_mi_put_type(mi, TE_MI_MEAS_LATENCY, "elapsed", st.elapsed_us,
                          TE_MI_MEAS_MULTIPLIER_MICRO);
        tsapi_mi_put(mi, "mpix_per_s", st.mpix_per_s);
    }
    tsapi_mi_graph_type(mi, "time", "Statistics time vs tile size", "tile",
                        TE_MI_MEAS_LATENCY, "elapsed");
    tsapi_mi_graph(mi, "rate", "Throughput (Mpix/s) vs tile size", "tile",
                   "mpix_per_s");

    TEST_SUCCESS;

cleanup:
    tapi_geo_raster_close(sess.pco, r);
    te_string_free(&path);
    te_string_free(&p);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
