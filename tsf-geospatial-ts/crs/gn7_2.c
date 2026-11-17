/** @file
 * @brief crs group
 *
 * The worked examples of IOGP Guidance Note 7-2 (EPSG), the published
 * reference for each projection method: geographic coordinates to the
 * projected ones the note gives (to the centimetre it prints), and back
 * (to a billionth of a degree). Each is a conversion with no datum
 * change, so the engine must call it exact.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "crs/gn7_2"

#include "te_config.h"
#include "tapi_test.h"

#include <math.h>

#include "tapi_geo_crs.h"
#include "tapi_geo_assert.h"
#include "tsapi_geo.h"

static const struct {
    const char *method;
    const char *src;
    const char *dst;
    double lon, lat;
    double e, n;
} cases[] = {
    { "tm", "EPSG:4277", "EPSG:27700", 0.5, 50.5, 577274.99, 69740.50 },
    { "lcc2sp", "EPSG:4267", "EPSG:32040", -96, 28.5, 2963503.91,
      254759.80 },
    { "merc_a", "EPSG:4257", "EPSG:3002", 120, -3, 5009726.58, 569150.82 },
    { "ps_a", "EPSG:4326", "EPSG:32661", 44, 73, 3320416.75, 632668.43 },
    /* 100 deg 20' W, 24 deg 22' 54.433" N */
    { "pseudo_merc", "EPSG:4326", "EPSG:3857", -100.333333333333333,
      24.381786944444444, -11169055.58, 2800000.00 },
};

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *method;
    tapi_geo_xform_info xi;
    double in[2], out[2], back[2];
    te_mi_logger *mi = NULL;
    unsigned int i;
    int c = -1;

    TEST_START;
    TEST_GET_STRING_PARAM(method);
    for (i = 0; i < TE_ARRAY_LEN(cases); i++)
        if (strcmp(cases[i].method, method) == 0)
            c = (int)i;
    if (c < 0)
        TEST_FAIL("Unknown method %s", method);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_crs_gn72");
    mi = tsapi_mi_open();

    TEST_STEP("%s -> %s: the GN7-2 example", cases[c].src, cases[c].dst);
    in[0] = cases[c].lon;
    in[1] = cases[c].lat;
    CHECK_RC(tapi_geo_transform_coords(sess.pco, cases[c].src, cases[c].dst,
                                       TAPI_GEO_AXIS_GIS, NULL, in, 1, 2,
                                       out, &xi));
    tapi_geo_xform_info_log(&xi);
    RING("GN7-2 %s: E %.4f N %.4f, published E %.2f N %.2f", method, out[0],
         out[1], cases[c].e, cases[c].n);
    tsapi_mi_put(mi, "easting_residual_mm", (out[0] - cases[c].e) * 1000);
    tsapi_mi_put(mi, "northing_residual_mm", (out[1] - cases[c].n) * 1000);
    TAPI_GEO_ASSERT(tapi_geo_assert_coord_near(out[0], out[1], cases[c].e,
                                               cases[c].n, 0.01,
                                               "GN7-2 example"));
    if (!xi.exact || xi.ballpark)
        TEST_VERDICT("A %s conversion is not reported exact", method);
    if (xi.out_of_domain != 0)
        TEST_VERDICT("The GN7-2 point is reported outside the area of use");

    TEST_STEP("And back");
    CHECK_RC(tapi_geo_transform_coords(sess.pco, cases[c].dst, cases[c].src,
                                       TAPI_GEO_AXIS_GIS, NULL, out, 1, 2,
                                       back, &xi));
    tsapi_mi_put(mi, "lon_residual_ndeg", (back[0] - in[0]) * 1e9);
    tsapi_mi_put(mi, "lat_residual_ndeg", (back[1] - in[1]) * 1e9);
    TAPI_GEO_ASSERT(tapi_geo_assert_coord_near(back[0], back[1], in[0],
                                               in[1], 1e-9,
                                               "inverse of the GN7-2 "
                                               "example"));
    tsapi_mi_graph(mi, "residual", "Residuals: forward (mm), inverse "
                   "(1e-9 degree)", NULL,
                   "easting_residual_mm,northing_residual_mm,"
                   "lon_residual_ndeg,lat_residual_ndeg");

    TEST_SUCCESS;

cleanup:
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
