/** @file
 * @brief raster group
 *
 * NoData is honoured: every k-th pixel (row-major, the first included)
 * is NoData, so ceil(W*H / k) are, and statistics skip exactly them.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "raster/nodata"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo_raster.h"
#include "tsapi_geo.h"

#define W 500
#define H 300

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    int every;
    te_string path = TE_STRING_INIT;
    te_string p = TE_STRING_INIT;
    tapi_geo_raster *r = NULL;
    tapi_geo_raster_stats st;
    long long want;
    te_mi_logger *mi = NULL;

    TEST_START;
    TEST_GET_INT_PARAM(every);
    want = every > 0 ? ((long long)W * H + every - 1) / every : 0;

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_ras_nodata");
    mi = tsapi_mi_open();

    te_string_append(&path, "%s/n.tif", sess.work);
    te_string_append(&p, "width=%d\nheight=%d\ntype=Int16\npattern=constant"
                     "\na=11\nnodata=-32768\nnodata_every=%d", W, H, every);
    CHECK_RC(tapi_geo_raster_write(sess.pco, path.ptr, p.ptr));
    CHECK_RC(tapi_geo_raster_open(sess.pco, path.ptr, NULL, &r));
    CHECK_RC(tapi_geo_raster_statistics(sess.pco, r, 1, 0, &st));
    tsapi_mi_put(mi, "nodata", (double)st.nodata);
    tsapi_mi_put(mi, "expected", (double)want);
    tsapi_mi_graph(mi, "nd", "NoData pixels found vs expected", NULL,
                   "nodata,expected");
    if (st.nodata != want || st.valid != (long long)W * H - want)
        TEST_VERDICT("%lld NoData and %lld valid pixels, expected %lld and "
                     "%lld", st.nodata, st.valid, want, W * H - want);
    if (st.valid > 0 && (st.min != 11 || st.max != 11))
        TEST_VERDICT("NoData leaked into the statistics: min %g max %g",
                     st.min, st.max);

    TEST_SUCCESS;

cleanup:
    tapi_geo_raster_close(sess.pco, r);
    te_string_free(&path);
    te_string_free(&p);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
