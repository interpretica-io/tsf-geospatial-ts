/** @file
 * @brief crs group
 *
 * A bounding box transformed by its corners only misses the bulge of
 * its edges: in LAEA Europe the parallel at 35 N sags south between the
 * corners. Densified, the transformed box must contain the image of
 * every point of the original edges; corners-only is the known-bad
 * control that must not.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "crs/bounds"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo_crs.h"
#include "tsapi_geo.h"

#define NPTS 160

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    int densify;
    tapi_geo_bbox in = { -10, 35, 30, 70 };
    tapi_geo_bbox out;
    tapi_geo_xform_info xi;
    tapi_geo_res r = TAPI_GEO_RES_INIT;
    te_string p = TE_STRING_INIT;
    double pts[2 * NPTS], img[2 * NPTS];
    unsigned int i, outside = 0;
    te_mi_logger *mi = NULL;

    TEST_START;
    TEST_GET_INT_PARAM(densify);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_crs_bounds");
    mi = tsapi_mi_open();

    TEST_STEP("Transform the box with densify=%d", densify);
    te_string_append(&p, "src=EPSG:4326\ndst=EPSG:3035\naxis=gis\n"
                     "densify=%d", densify);
    CHECK_RC(tapi_geo_call(sess.pco, TAPI_GEO_RPC_TRANSFORM_BOUNDS, 0, 0, "",
                           "-10 35 30 70", p.ptr, &r));
    out.minx = tapi_geo_res_dbl(&r, "minx");
    out.miny = tapi_geo_res_dbl(&r, "miny");
    out.maxx = tapi_geo_res_dbl(&r, "maxx");
    out.maxy = tapi_geo_res_dbl(&r, "maxy");
    RING("Box in LAEA Europe: %.3f %.3f %.3f %.3f", out.minx, out.miny,
         out.maxx, out.maxy);

    TEST_STEP("Transform %u points along the original edges", NPTS);
    for (i = 0; i < NPTS; i++)
    {
        double f = (double)(i % (NPTS / 4)) / (NPTS / 4);

        switch (i / (NPTS / 4))
        {
            case 0: pts[2 * i] = in.minx + f * 40; pts[2 * i + 1] = 35; break;
            case 1: pts[2 * i] = 30; pts[2 * i + 1] = 35 + f * 35; break;
            case 2: pts[2 * i] = 30 - f * 40; pts[2 * i + 1] = 70; break;
            default: pts[2 * i] = -10; pts[2 * i + 1] = 70 - f * 35; break;
        }
    }
    CHECK_RC(tapi_geo_transform_coords(sess.pco, "EPSG:4326", "EPSG:3035",
                                       TAPI_GEO_AXIS_GIS, NULL, pts, NPTS, 2,
                                       img, &xi));
    for (i = 0; i < NPTS; i++)
    {
        double gap = 0;

        if (img[2 * i] < out.minx - 1e-6 || img[2 * i] > out.maxx + 1e-6 ||
            img[2 * i + 1] < out.miny - 1e-6 ||
            img[2 * i + 1] > out.maxy + 1e-6)
        {
            outside++;
            gap = out.miny - img[2 * i + 1];
        }
        tsapi_mi_put_type(mi, TE_MI_MEAS_UNITLESS_VALUE, "miss_m",
                          gap > 0 ? gap : 0, TE_MI_MEAS_MULTIPLIER_PLAIN);
    }
    tsapi_mi_graph(mi, "miss", "How far each edge point falls below the "
                   "transformed box (m)", NULL, "miss_m");
    RING("%u of %u edge points outside the transformed box", outside, NPTS);
    if (densify > 0 && outside > 0)
        TEST_VERDICT("The densified box misses points of its own edges");
    if (densify == 0 && outside == 0)
        TEST_VERDICT("Corners alone covered the curved edges: the control "
                     "shows nothing");

    TEST_SUCCESS;

cleanup:
    tapi_geo_rpc_res_free(&r);
    te_string_free(&p);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
