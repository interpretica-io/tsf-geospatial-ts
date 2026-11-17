/** @file
 * @brief geometry group
 *
 * Measurements against published values. Planar ones against closed
 * forms (a 3-4-5 triangle, squares). Geodesic ones on WGS 84 against
 * GeographicLib (C. F. F. Karney): the GeodSolve example JFK - LHR,
 * the 1 x 1 degree cell at the equator (Planimeter), and ten degrees
 * along the equator (10 a pi / 180: a geodesic there). The same
 * geodesic quantities measured from Web Mercator coordinates must agree:
 * the engine measures on the ellipsoid, not on the projection.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "geometry/measure"

#include "te_config.h"
#include "tapi_test.h"

#include <math.h>

#include "tapi_geo.h"
#include "tapi_geo_crs.h"
#include "tsapi_geo.h"

typedef struct ref {
    const char *what;
    const char *a;
    const char *b;          /* distance only */
    const char *crs;
    double value;
    double tol;
    const char *unit;
} ref;

static const ref planar[] = {
    { "distance", "POINT (0 0)", "LINESTRING (3 -1, 3 4)", "EPSG:3857", 3,
      1e-12, "metre" },
    { "length", "LINESTRING (0 0, 3 4)", NULL, "EPSG:3857", 5, 1e-12,
      "metre" },
    { "area", "POLYGON ((0 0, 4 0, 0 3, 0 0))", NULL, "EPSG:3857", 6, 1e-12,
      "m2" },
    { "perimeter", "POLYGON ((0 0, 4 0, 0 3, 0 0))", NULL, "EPSG:3857", 12,
      1e-12, "metre" },
    /* EPSG:2227 is in US survey feet: the unit is said, not converted. */
    { "length", "LINESTRING (0 0, 0 100)", NULL, "EPSG:2227", 100, 1e-12,
      "US survey foot" },
};

static const ref geodesic[] = {
    /* GeodSolve -i 40.6 -73.8 51.6 -0.5 -> 5551759.400319 m */
    { "distance", "POINT (-73.8 40.6)", "POINT (-0.5 51.6)", "EPSG:4326",
      5551759.400319, 1e-4, "m" },
    { "length", "LINESTRING (0 0, 10 0)", NULL, "EPSG:4326",
      10 * 6378137.0 * M_PI / 180.0, 1e-6, "m" },
    /* Planimeter: the 1 x 1 degree cell at the equator. */
    { "area", "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))", NULL, "EPSG:4326",
      12308778361.469, 1, "m2" },
    /* Perimeter: the sum of the four geodesic edges (NAN: computed). */
    { "perimeter", "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))", NULL, "EPSG:4326",
      NAN, 1e-6, "m" },
    /* A polygon with a hole: the cell minus a quarter cell. */
    { "area", "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0), (0 0, 0 0.5, 0.5 0.5, "
      "0.5 0, 0 0))", NULL, "EPSG:4326", NAN, 1, "m2" },
};

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *what;
    const char *mode;
    bool geo;
    const ref *tab;
    unsigned int n, i;
    te_mi_logger *mi = NULL;

    TEST_START;
    TEST_GET_STRING_PARAM(what);
    TEST_GET_STRING_PARAM(mode);
    geo = strcmp(mode, "geodesic") == 0;
    tab = geo ? geodesic : planar;
    n = geo ? TE_ARRAY_LEN(geodesic) : TE_ARRAY_LEN(planar);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_geom_measure");
    mi = tsapi_mi_open();

    for (i = 0; i < n; i++)
    {
        tapi_geo_geometry *a = NULL;
        tapi_geo_geometry *b = NULL;
        tapi_geo_measure m;
        double want = tab[i].value;

        if (strcmp(tab[i].what, what) != 0)
            continue;
        TEST_SUBSTEP("%s %s of %s", mode, what, tab[i].a);
        TSAPI_GEO_WKT(&sess, tab[i].a, tab[i].crs, a);
        if (tab[i].b != NULL)
            TSAPI_GEO_WKT(&sess, tab[i].b, tab[i].crs, b);
        CHECK_RC(tapi_geo_measure_any(sess.pco, what, a, b,
                                      geo ? TAPI_GEO_GEODESIC :
                                            TAPI_GEO_PLANAR,
                                      TAPI_GEO_AXIS_GIS, NULL, &m));
        RING("%s = %.17g %s (%s)", what, m.value, m.unit, m.method);
        if (isnan(want) && strcmp(what, "perimeter") == 0)
        {
            static const char *const v[] = {
                "POINT (0 0)", "POINT (1 0)", "POINT (1 1)", "POINT (0 1)",
                "POINT (0 0)"
            };
            unsigned int k;

            want = 0;
            for (k = 0; k < 4; k++)
            {
                tapi_geo_geometry *p = NULL, *q = NULL;
                tapi_geo_measure d;

                TSAPI_GEO_WKT(&sess, v[k], tab[i].crs, p);
                TSAPI_GEO_WKT(&sess, v[k + 1], tab[i].crs, q);
                CHECK_RC(tapi_geo_distance(sess.pco, p, q, TAPI_GEO_GEODESIC,
                                           TAPI_GEO_AXIS_GIS, &d));
                want += d.value;
                tapi_geo_geometry_free(sess.pco, p);
                tapi_geo_geometry_free(sess.pco, q);
            }
        }
        else if (isnan(want))
        {
            /* The holed cell: the cell's area minus the quarter's. */
            tapi_geo_geometry *c = NULL, *q = NULL;
            tapi_geo_measure mc, mq;

            TSAPI_GEO_WKT(&sess, "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))",
                          tab[i].crs, c);
            TSAPI_GEO_WKT(&sess, "POLYGON ((0 0, 0.5 0, 0.5 0.5, 0 0.5, "
                          "0 0))", tab[i].crs, q);
            CHECK_RC(tapi_geo_area(sess.pco, c, TAPI_GEO_GEODESIC,
                                   TAPI_GEO_AXIS_GIS, &mc));
            CHECK_RC(tapi_geo_area(sess.pco, q, TAPI_GEO_GEODESIC,
                                   TAPI_GEO_AXIS_GIS, &mq));
            want = mc.value - mq.value;
            tapi_geo_geometry_free(sess.pco, c);
            tapi_geo_geometry_free(sess.pco, q);
        }
        if (strcmp(m.unit, tab[i].unit) != 0)
            TEST_VERDICT("%s %s is in '%s', expected %s", mode, what, m.unit,
                         tab[i].unit);
        if (!tsapi_geo_near(m.value, want, tab[i].tol))
            TEST_VERDICT("%s %s of fixture %u is off the reference", mode,
                         what, i);
        tsapi_mi_put(mi, "residual", m.value - want);

        if (geo)
        {
            /* The same thing from Web Mercator coordinates. */
            tapi_geo_geometry *pa = NULL, *pb = NULL;
            tapi_geo_xform_info xi;
            tapi_geo_measure pm;

            CHECK_RC(tapi_geo_transform(sess.pco, a, NULL, "EPSG:3857",
                                        TAPI_GEO_AXIS_GIS, NULL, &pa, &xi));
            if (b != NULL)
                CHECK_RC(tapi_geo_transform(sess.pco, b, NULL, "EPSG:3857",
                                            TAPI_GEO_AXIS_GIS, NULL, &pb,
                                            &xi));
            CHECK_RC(tapi_geo_measure_any(sess.pco, what, pa, pb,
                                          TAPI_GEO_GEODESIC,
                                          TAPI_GEO_AXIS_GIS, NULL, &pm));
            /* Vertices round-trip through the projection to ~1e-9 deg;
             * segments between them are geodesics either way. */
            if (!tsapi_geo_near(pm.value, m.value,
                                fmax(tab[i].tol, fabs(m.value) * 1e-9)))
                TEST_VERDICT("Geodesic %s from Web Mercator differs from "
                             "the one from lon/lat", what);
            tsapi_mi_put(mi, "residual_mercator", pm.value - want);
            tapi_geo_geometry_free(sess.pco, pa);
            tapi_geo_geometry_free(sess.pco, pb);
        }
        if (m.ignored[0] != '\0')
            RING("Ignored dimensions: %s", m.ignored);
        tapi_geo_geometry_free(sess.pco, a);
        tapi_geo_geometry_free(sess.pco, b);
    }
    tsapi_mi_graph(mi, "residual", "Measured minus reference", NULL,
                   geo ? "residual,residual_mercator" : "residual");

    TEST_SUCCESS;

cleanup:
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
