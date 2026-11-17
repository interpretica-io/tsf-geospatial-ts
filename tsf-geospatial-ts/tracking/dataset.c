/** @file
 * @brief tracking group
 *
 * Observations recorded in a dataset (a GeoPackage of points with a time
 * field, here in PT-TM06 metres) load into the same track as the same
 * observations given as text in WGS 84: equal sample count, duration
 * and length to the millimetre.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "tracking/dataset"

#include "te_config.h"
#include "tapi_test.h"

#include <math.h>

#include "tapi_geo_track.h"
#include "tsapi_geo.h"

#define GEN "count=300\nspeed=8\ninterval=2\nheading=200\nturn=5\nseed=4\n" \
            "start=-8.6 41.15"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    te_string path = TE_STRING_INIT;
    te_string tm = TE_STRING_INIT;
    tapi_geo_dataset *ds = NULL;
    tapi_geo_track *a = NULL, *b = NULL;
    tapi_geo_track_sanity sa, sb;
    te_mi_logger *mi = NULL;

    TEST_START;

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_trk_ds");
    mi = tsapi_mi_open();

    TEST_STEP("The trajectory as text, and in a PT-TM06 GeoPackage");
    CHECK_RC(tapi_geo_track_generate(sess.pco, GEN, &a));
    te_string_append(&path, "%s/track.gpkg", sess.work);
    te_string_append(&tm, "%s/track_tm06.gpkg", sess.work);
    CHECK_RC(tapi_geo_dataset_generate(sess.pco, path.ptr, "GPKG",
                                       "EPSG:4326", "trajectory", GEN, NULL,
                                       NULL, NULL, NULL));
    CHECK_RC(tapi_geo_dataset_open(sess.pco, path.ptr, NULL, &ds));
    CHECK_RC(tapi_geo_dataset_copy(sess.pco, ds, tm.ptr, "GPKG",
                                   "EPSG:3763"));
    tapi_geo_dataset_close(sess.pco, ds);
    ds = NULL;
    CHECK_RC(tapi_geo_dataset_open(sess.pco, tm.ptr, NULL, &ds));

    TEST_STEP("Load the GeoPackage as a track");
    CHECK_RC(tapi_geo_track_load_dataset(sess.pco, ds, NULL, "t",
                                         "EPSG:3763", TAPI_GEO_AXIS_GIS, &b));
    CHECK_RC(tapi_geo_track_check(sess.pco, a, NULL, &sa));
    CHECK_RC(tapi_geo_track_check(sess.pco, b, NULL, &sb));
    RING("text: %lld samples, %.3f m, %.0f s; dataset: %lld samples, %.3f m, "
         "%.0f s", sa.samples, sa.length_m, sa.duration_s, sb.samples,
         sb.length_m, sb.duration_s);
    tsapi_mi_put(mi, "length_m", sa.length_m);
    tsapi_mi_put(mi, "length_m", sb.length_m);
    tsapi_mi_graph(mi, "len", "Length: from text, from the dataset", NULL,
                   "length_m");
    if (sa.samples != sb.samples || sa.duration_s != sb.duration_s)
        TEST_VERDICT("The dataset track has %lld samples over %g s, the text "
                     "one %lld over %g s", sb.samples, sb.duration_s,
                     sa.samples, sa.duration_s);
    if (!tsapi_geo_near(sa.length_m, sb.length_m, 1e-3))
        TEST_VERDICT("Lengths differ: %.6f vs %.6f m", sa.length_m,
                     sb.length_m);

    TEST_SUCCESS;

cleanup:
    tapi_geo_track_free(sess.pco, a);
    tapi_geo_track_free(sess.pco, b);
    tapi_geo_dataset_close(sess.pco, ds);
    te_string_free(&path);
    te_string_free(&tm);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
