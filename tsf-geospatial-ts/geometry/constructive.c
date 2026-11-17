/** @file
 * @brief geometry group
 *
 * Constructive operations against areas known in closed form: overlay
 * of axis-aligned squares, buffers against pi r^2 (the polygonal
 * approximation converges as the segments per quadrant grow - charted),
 * and simplification, which removes vertices but keeps a valid shape
 * when asked to preserve topology.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "geometry/constructive"

#include "te_config.h"
#include "tapi_test.h"

#include <math.h>

#include "tapi_geo.h"
#include "tapi_geo_assert.h"
#include "tsapi_geo.h"

#define A_WKT "POLYGON ((0 0, 4 0, 4 4, 0 4, 0 0))"
#define B_WKT "POLYGON ((2 2, 6 2, 6 6, 2 6, 2 2))"

static double
area(tsapi_geo_session *s, const tapi_geo_geometry *g)
{
    tapi_geo_measure m;

    CHECK_RC(tapi_geo_area(s->pco, g, TAPI_GEO_PLANAR, TAPI_GEO_AXIS_GIS,
                           &m));
    return m.value;
}

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *op;
    tapi_geo_geometry *a = NULL;
    tapi_geo_geometry *b = NULL;
    tapi_geo_geometry *r = NULL;
    te_mi_logger *mi = NULL;

    TEST_START;
    TEST_GET_STRING_PARAM(op);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_geom_cons");
    mi = tsapi_mi_open();
    TSAPI_GEO_WKT(&sess, A_WKT, "EPSG:3857", a);
    TSAPI_GEO_WKT(&sess, B_WKT, "EPSG:3857", b);

    if (strcmp(op, "buffer") == 0)
    {
        static const unsigned int quads[] = { 1, 2, 4, 8, 16, 32, 64, 128 };
        tapi_geo_geometry *p = NULL;
        double prev = INFINITY;
        unsigned int i;

        TEST_STEP("Buffer a point by 10 m: the area approaches 100 pi");
        TSAPI_GEO_WKT(&sess, "POINT (100 100)", "EPSG:3857", p);
        for (i = 0; i < TE_ARRAY_LEN(quads); i++)
        {
            double v, err;

            CHECK_RC(tapi_geo_buffer(sess.pco, p, 10, quads[i], &r));
            v = area(&sess, r);
            err = fabs(v - 100 * M_PI) / (100 * M_PI);
            /* An inscribed polygon is always smaller, and finer is
             * closer. */
            if (!(v < 100 * M_PI) || !(err < prev))
                TEST_VERDICT("Buffer area %.6f with %u segments per "
                             "quadrant does not converge on 100 pi", v,
                             quads[i]);
            prev = err;
            tsapi_mi_put(mi, "quad_segs", quads[i]);
            tsapi_mi_put_type(mi, TE_MI_MEAS_PERCENTAGE, "area_error",
                              err * 100, TE_MI_MEAS_MULTIPLIER_PLAIN);
            tapi_geo_geometry_free(sess.pco, r);
            r = NULL;
        }
        if (prev > 1e-4)
            TEST_VERDICT("Buffer area error stays above 0.01%%");
        tsapi_mi_graph_type(mi, "conv", "Buffer area error vs segments per "
                            "quadrant", "quad_segs", TE_MI_MEAS_PERCENTAGE,
                            "area_error");
        tapi_geo_geometry_free(sess.pco, p);
    }
    else if (strcmp(op, "simplify") == 0)
    {
        static const double tols[] = { 0, 0.001, 0.01, 0.1, 0.5, 1, 2 };
        tapi_geo_geometry *wave = NULL;
        te_string w = TE_STRING_INIT;
        long long prev_n = -1;
        unsigned int i;

        TEST_STEP("Simplify a 2000-vertex wavy polygon at growing "
                  "tolerances");
        te_string_append(&w, "POLYGON ((0 0");
        for (i = 1; i <= 1000; i++)
            te_string_append(&w, ", %.17g %.17g", i * 0.1,
                             0.5 * sin(i * 0.37));
        te_string_append(&w, ", 100 50");
        for (i = 1000; i > 0; i--)
            te_string_append(&w, ", %.17g %.17g", (i - 1) * 0.1,
                             50 + 0.5 * sin(i * 0.53));
        te_string_append(&w, ", 0 0))");
        TSAPI_GEO_WKT(&sess, w.ptr, "EPSG:3857", wave);
        te_string_free(&w);
        for (i = 0; i < TE_ARRAY_LEN(tols); i++)
        {
            tapi_geo_info info;

            CHECK_RC(tapi_geo_simplify(sess.pco, wave, tols[i], true, &r));
            CHECK_RC(tapi_geo_info_get(sess.pco, r, &info));
            if (prev_n >= 0 && info.npoints > prev_n)
                TEST_VERDICT("Simplification added vertices at tolerance "
                             "%g", tols[i]);
            prev_n = info.npoints;
            TAPI_GEO_ASSERT(tapi_geo_assert_valid(sess.pco, r,
                                                  "simplified polygon"));
            tsapi_mi_put(mi, "tolerance", tols[i]);
            tsapi_mi_put(mi, "vertices", (double)info.npoints);
            tapi_geo_geometry_free(sess.pco, r);
            r = NULL;
        }
        if (prev_n > 8)
            TEST_VERDICT("Simplifying at 2 m left %lld vertices", prev_n);
        tsapi_mi_graph(mi, "simplify", "Vertices left vs tolerance",
                       "tolerance", "vertices");
        tapi_geo_geometry_free(sess.pco, wave);
    }
    else
    {
        /* intersection 4, union 28, difference 12, sym_difference 24 */
        double want = strcmp(op, "intersection") == 0 ? 4 :
                      strcmp(op, "union") == 0 ? 28 :
                      strcmp(op, "difference") == 0 ? 12 : 24;
        double v;

        TEST_STEP("%s of two 4x4 squares overlapping by 2x2", op);
        CHECK_RC(tapi_geo_construct(sess.pco, op, a, b, NULL, &r));
        v = area(&sess, r);
        tsapi_mi_put_type(mi, TE_MI_MEAS_UNITLESS_VALUE, "area", v,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
        tsapi_mi_put_type(mi, TE_MI_MEAS_UNITLESS_VALUE, "area", want,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
        tsapi_mi_graph(mi, "area", "Area: result, then the closed form",
                       NULL, "area");
        if (!tsapi_geo_near(v, want, 1e-9))
            TEST_VERDICT("%s area %.17g, expected %g", op, v, want);
        TAPI_GEO_ASSERT(tapi_geo_assert_valid(sess.pco, r, op));
        if (strcmp(r->crs, "EPSG:3857") != 0)
            TEST_VERDICT("%s lost the CRS", op);
    }

    TEST_SUCCESS;

cleanup:
    tapi_geo_geometry_free(sess.pco, r);
    tapi_geo_geometry_free(sess.pco, a);
    tapi_geo_geometry_free(sess.pco, b);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
