/** @file
 * @brief crs group
 *
 * Axis order is always the caller's to say and never swapped behind its
 * back. The same position given in GIS order (longitude first) and in
 * the authority order of EPSG:4326 (latitude first) must land on the
 * same projected point, given in the target's own order (UPS North is
 * northing, easting). A call that does not say which order is refused,
 * and the wrong order is detectably wrong, not silently "fixed".
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "crs/axis_order"

#include "te_config.h"
#include "tapi_test.h"

#include <math.h>

#include "tapi_geo_crs.h"
#include "tapi_geo_assert.h"
#include "tsapi_geo.h"

/* GN7-2 polar stereographic example: 73 N 44 E on WGS 84. */
#define LAT 73.0
#define LON 44.0
#define E   3320416.75
#define N   632668.43

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *axis;
    bool gis;
    double in[2], out[2], wrong[2];
    tapi_geo_xform_info xi;
    tapi_geo_res r = TAPI_GEO_RES_INIT;
    te_mi_logger *mi = NULL;

    TEST_START;
    TEST_GET_STRING_PARAM(axis);
    gis = strcmp(axis, "gis") == 0;

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_crs_axis");
    mi = tsapi_mi_open();

    TEST_STEP("Transform the position in %s order", axis);
    in[0] = gis ? LON : LAT;
    in[1] = gis ? LAT : LON;
    CHECK_RC(tapi_geo_transform_coords(sess.pco, "EPSG:4326", "EPSG:32661",
                                       gis ? TAPI_GEO_AXIS_GIS :
                                             TAPI_GEO_AXIS_AUTHORITY,
                                       NULL, in, 1, 2, out, &xi));
    if (strcmp(xi.axis, axis) != 0)
        TEST_VERDICT("The result does not say which axis order it used");
    /* Output in the same convention: GIS = easting first; authority =
     * the target's own order, northing first for UPS North. */
    TAPI_GEO_ASSERT(tapi_geo_assert_coord_near(out[0], out[1],
                                               gis ? E : N, gis ? N : E,
                                               0.01, "UPS North"));
    tsapi_mi_put(mi, "x", out[0]);
    tsapi_mi_put(mi, "y", out[1]);

    TEST_STEP("The swapped input is a different place, not corrected");
    {
        double sw[2] = { in[1], in[0] };

        CHECK_RC(tapi_geo_transform_coords(sess.pco, "EPSG:4326",
                                           "EPSG:32661",
                                           gis ? TAPI_GEO_AXIS_GIS :
                                                 TAPI_GEO_AXIS_AUTHORITY,
                                           NULL, sw, 1, 2, wrong, &xi));
        if (hypot(wrong[0] - out[0], wrong[1] - out[1]) < 1000)
            TEST_VERDICT("Swapped coordinates gave the same point: the axis "
                         "order was guessed");
        /* 44 N 73 E is outside UPS North's area of use (60-90 N). */
        if (xi.out_of_domain != 1)
            TEST_VERDICT("The swapped position is not reported outside the "
                         "area of use");
        tsapi_mi_put(mi, "x", wrong[0]);
        tsapi_mi_put(mi, "y", wrong[1]);
    }

    TEST_STEP("A transformation that does not name the order is refused");
    rc = tapi_geo_call(sess.pco, TAPI_GEO_RPC_TRANSFORM_COORDS, 0, 0, "",
                       "44 73", "src=EPSG:4326\ndst=EPSG:32661", &r);
    if (rc == 0)
        TEST_VERDICT("A transformation ran without an axis order");
    if (TE_RC_GET_ERROR(rc) != TE_EINVAL)
        TEST_VERDICT("A missing axis order is refused with %r", rc);
    tsapi_mi_graph(mi, "xy", "Output coordinates: right order, then "
                   "swapped input", NULL, "x,y");

    TEST_SUCCESS;

cleanup:
    tapi_geo_rpc_res_free(&r);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
