/** @file
 * @brief tracking group
 *
 * Position error against a reference: a receiver whose fixes carry
 * independent east and north errors of standard deviation sigma has
 * RMSE sigma * sqrt(2) and median error sigma * sqrt(2 ln 2) (the
 * Rayleigh distribution). 2000 fixes of a simulated receiver are
 * checked against the true trajectory; the per-fix error is charted.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "tracking/error"

#include "te_config.h"
#include "tapi_test.h"

#include <math.h>

#include "tapi_geo_track.h"
#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    double noise;
    te_string p = TE_STRING_INIT;
    tapi_geo_track *truth = NULL, *fixes = NULL;
    tapi_geo_track_error e;
    te_mi_logger *mi = NULL;
    unsigned int i;
    double rmse, p50;

    TEST_START;
    TEST_GET_DOUBLE_PARAM(noise);
    rmse = noise * sqrt(2);
    p50 = noise * sqrt(2 * log(2));

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_trk_error");
    mi = tsapi_mi_open();

    TEST_STEP("A true trajectory and a receiver with sigma = %g m", noise);
    CHECK_RC(tapi_geo_track_generate(sess.pco,
                                     "count=2000\nspeed=15\ninterval=1\n"
                                     "heading=10\nturn=2\nseed=9\n"
                                     "start=-8.6 41.1", &truth));
    te_string_append(&p, "count=2000\nspeed=15\ninterval=1\nheading=10\n"
                     "turn=2\nseed=9\nstart=-8.6 41.1\nnoise=%.17g", noise);
    /* Same seed, same path: the noise draws come after the heading's. */
    CHECK_RC(tapi_geo_track_generate(sess.pco, p.ptr, &fixes));

    TEST_STEP("Error of the fixes against the truth");
    CHECK_RC(tapi_geo_track_error_vs(sess.pco, fixes, truth, 4 * rmse, &e));
    RING("n %lld, mean %.3f, p50 %.3f (Rayleigh %.3f), p95 %.3f, rmse %.3f "
         "(sigma sqrt 2 = %.3f), max %.3f, over 4 rmse %lld", e.n, e.mean,
         e.p50, p50, e.p95, e.rmse, rmse, e.max, e.over);
    for (i = 0; i < e.n_errors && i < 600; i++)
        tsapi_mi_put_type(mi, TE_MI_MEAS_UNITLESS_VALUE, "error_m",
                          e.errors[i], TE_MI_MEAS_MULTIPLIER_PLAIN);
    tsapi_mi_graph(mi, "err", "Position error of the first 600 fixes (m)",
                   NULL, "error_m");
    if (e.n != 2000 || e.unmatched != 0)
        TEST_VERDICT("%lld fixes matched, %lld not", e.n, e.unmatched);
    /* With 2000 samples the estimates are within a few per cent; the
     * walk itself drifts heading, which a wrong pairing would expose. */
    if (fabs(e.rmse - rmse) > 0.08 * rmse)
        TEST_VERDICT("RMSE %.3f m for sigma %g m: not sigma sqrt 2", e.rmse,
                     noise);
    if (fabs(e.p50 - p50) > 0.08 * p50)
        TEST_VERDICT("Median error %.3f m: not Rayleigh", e.p50);
    if (!(e.p50 <= e.p90 && e.p90 <= e.p95 && e.p95 <= e.p99 &&
          e.p99 <= e.max))
        TEST_VERDICT("Percentiles out of order");
    tapi_geo_track_error_free(&e);

    TEST_SUCCESS;

cleanup:
    tapi_geo_track_free(sess.pco, truth);
    tapi_geo_track_free(sess.pco, fixes);
    te_string_free(&p);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
