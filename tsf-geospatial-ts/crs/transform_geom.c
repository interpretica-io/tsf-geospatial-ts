/** @file
 * @brief crs group
 *
 * Geometries, not only points: a polygon with a hole, in 3D with M,
 * transformed WGS 84 -> PT-TM06 -> WGS 84 comes back to within a
 * millimetre and keeps its type, its rings, its Z and its M. The
 * transformed geometry carries the destination CRS.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "crs/transform_geom"

#include "te_config.h"
#include "tapi_test.h"

#include <math.h>

#include "tapi_geo_crs.h"
#include "tapi_geo_assert.h"
#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *dims;
    tapi_geo_geometry *g = NULL, *t = NULL, *back = NULL;
    tapi_geo_geometry *gb = NULL, *bb = NULL;
    tapi_geo_xform_info xi;
    tapi_geo_info i0, i1, i2;
    tapi_geo_measure hd;
    te_string wkt = TE_STRING_INIT;
    te_mi_logger *mi = NULL;
    bool z, m;

    TEST_START;
    TEST_GET_STRING_PARAM(dims);
    z = strchr(dims + 2, 'z') != NULL;
    m = strchr(dims + 2, 'm') != NULL;

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_crs_tgeom");
    if (m && (sess.engine.features & TAPI_GEO_FEAT_M) == 0)
        TEST_SKIP("The agent's GEOS has no M coordinates");
    mi = tsapi_mi_open();

#define V(_x, _y, _k) \
    te_string_append(&wkt, "%s%.10g %.10g%s%s", (_k) == 0 ? "" : ", ",     \
                     (_x), (_y), z ? " 120.5" : "", m ? " 7" : "")
    te_string_append(&wkt, "POLYGON%s ((", z ? (m ? " ZM" : " Z") :
                     (m ? " M" : ""));
    V(-8.70, 41.10, 0); V(-8.50, 41.10, 1); V(-8.50, 41.25, 2);
    V(-8.70, 41.25, 3); V(-8.70, 41.10, 4);
    te_string_append(&wkt, "), (");
    V(-8.65, 41.15, 0); V(-8.65, 41.20, 1); V(-8.55, 41.20, 2);
    V(-8.65, 41.15, 3);
    te_string_append(&wkt, "))");
#undef V
    TSAPI_GEO_WKT(&sess, wkt.ptr, "EPSG:4326", g);

    TEST_STEP("WGS 84 -> PT-TM06 (ETRS89) and back");
    CHECK_RC(tapi_geo_transform(sess.pco, g, NULL, "EPSG:3763",
                                TAPI_GEO_AXIS_GIS, NULL, &t, &xi));
    tapi_geo_xform_info_log(&xi);
    CHECK_RC(tapi_geo_transform(sess.pco, t, NULL, "EPSG:4326",
                                TAPI_GEO_AXIS_GIS, NULL, &back, &xi));
    CHECK_RC(tapi_geo_info_get(sess.pco, g, &i0));
    CHECK_RC(tapi_geo_info_get(sess.pco, t, &i1));
    CHECK_RC(tapi_geo_info_get(sess.pco, back, &i2));
    if (strcmp(i1.crs, "EPSG:3763") != 0)
        TEST_VERDICT("The transformed geometry is in '%s'", i1.crs);
    if (strcmp(i1.type, i0.type) != 0 || i1.npoints != i0.npoints ||
        strcmp(i1.dims, dims) != 0 || strcmp(i2.dims, dims) != 0)
        TEST_VERDICT("Transformation changed the geometry's shape: %s %s "
                     "%lld points", i1.type, i1.dims, i1.npoints);
    if (m && (i2.minm != 7 || i2.maxm != 7))
        TEST_VERDICT("M changed in a transformation");
    if (z)
        tsapi_mi_put(mi, "z_change_m", i2.maxz - i0.maxz);

    TEST_STEP("The round trip comes back to within a millimetre");
    /* Compare in metres: both back in PT-TM06, Hausdorff distance. */
    CHECK_RC(tapi_geo_transform(sess.pco, back, NULL, "EPSG:3763",
                                TAPI_GEO_AXIS_GIS, NULL, &bb, &xi));
    CHECK_RC(tapi_geo_transform(sess.pco, g, NULL, "EPSG:3763",
                                TAPI_GEO_AXIS_GIS, NULL, &gb, &xi));
    CHECK_RC(tapi_geo_measure_any(sess.pco, "hausdorff", gb, bb,
                                  TAPI_GEO_PLANAR, TAPI_GEO_AXIS_GIS, NULL,
                                  &hd));
    RING("Round-trip Hausdorff distance %.3g m", hd.value);
    tsapi_mi_put(mi, "roundtrip_mm", hd.value * 1000);
    tsapi_mi_graph(mi, "rt", "Round-trip error (mm)", NULL,
                   z ? "roundtrip_mm,z_change_m" : "roundtrip_mm");
    if (!(hd.value < 1e-3))
        TEST_VERDICT("The round trip moved the polygon by more than 1 mm");

    TEST_SUCCESS;

cleanup:
    tapi_geo_geometry_free(sess.pco, g);
    tapi_geo_geometry_free(sess.pco, t);
    tapi_geo_geometry_free(sess.pco, back);
    tapi_geo_geometry_free(sess.pco, gb);
    tapi_geo_geometry_free(sess.pco, bb);
    te_string_free(&wkt);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
