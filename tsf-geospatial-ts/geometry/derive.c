/** @file
 * @brief geometry group
 *
 * Bounds, centroids and envelopes of geometries whose answers are known:
 * an L-shaped polygon (centroid by the composite-area formula), a line,
 * a multipoint, and an empty geometry (no bounds, an empty centroid).
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "geometry/derive"

#include "te_config.h"
#include "tapi_test.h"

#include <math.h>

#include "tapi_geo.h"
#include "tapi_geo_assert.h"
#include "tsapi_geo.h"

static const struct {
    const char *wkt;
    tapi_geo_bbox bbox;
    double cx, cy;
    const char *envelope_type;
} cases[] = {
    /* An L: a 4x2 bar (centroid 2 1, area 8) and a 2x2 block on top of
     * its left end (centroid 1 3, area 4): (8*2 + 4*1)/12, (8*1+4*3)/12. */
    { "POLYGON ((0 0, 4 0, 4 2, 2 2, 2 4, 0 4, 0 0))", { 0, 0, 4, 4 },
      20.0 / 12, 20.0 / 12, "Polygon" },
    { "LINESTRING (0 0, 10 0, 10 10)", { 0, 0, 10, 10 }, 7.5, 2.5,
      "Polygon" },
    { "MULTIPOINT ((1 1), (3 1), (2 4))", { 1, 1, 3, 4 }, 2, 2, "Polygon" },
    { "POINT (5 6)", { 5, 6, 5, 6 }, 5, 6, "Point" },
};

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    te_mi_logger *mi = NULL;
    unsigned int i;

    TEST_START;

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_geom_derive");
    mi = tsapi_mi_open();

    for (i = 0; i < TE_ARRAY_LEN(cases); i++)
    {
        tapi_geo_geometry *g = NULL, *c = NULL, *e = NULL;
        tapi_geo_bbox bb;
        tapi_geo_info ci, ei;

        TEST_SUBSTEP("%s", cases[i].wkt);
        TSAPI_GEO_WKT(&sess, cases[i].wkt, "EPSG:3857", g);
        CHECK_RC(tapi_geo_bounds(sess.pco, g, &bb));
        TAPI_GEO_ASSERT(tapi_geo_assert_bbox_contains(&cases[i].bbox, &bb,
                                                      "bounds"));
        TAPI_GEO_ASSERT(tapi_geo_assert_bbox_contains(&bb, &cases[i].bbox,
                                                      "bounds"));
        CHECK_RC(tapi_geo_centroid(sess.pco, g, &c));
        CHECK_RC(tapi_geo_info_get(sess.pco, c, &ci));
        TAPI_GEO_ASSERT(tapi_geo_assert_coord_near(ci.bbox.minx,
                                                   ci.bbox.miny,
                                                   cases[i].cx, cases[i].cy,
                                                   1e-12, "centroid"));
        CHECK_RC(tapi_geo_envelope(sess.pco, g, &e));
        CHECK_RC(tapi_geo_info_get(sess.pco, e, &ei));
        if (strcmp(ei.type, cases[i].envelope_type) != 0)
            TEST_VERDICT("The envelope of %s is a %s", cases[i].wkt,
                         ei.type);
        if (strcmp(c->crs, "EPSG:3857") != 0 ||
            strcmp(e->crs, "EPSG:3857") != 0)
            TEST_VERDICT("A derived geometry lost the CRS");
        tsapi_mi_put(mi, "centroid_x", ci.bbox.minx);
        tsapi_mi_put(mi, "centroid_y", ci.bbox.miny);
        tapi_geo_geometry_free(sess.pco, g);
        tapi_geo_geometry_free(sess.pco, c);
        tapi_geo_geometry_free(sess.pco, e);
    }

    TEST_STEP("An empty geometry has no bounds and an empty centroid");
    {
        tapi_geo_geometry *g = NULL, *c = NULL;
        tapi_geo_info info;

        TSAPI_GEO_WKT(&sess, "POLYGON EMPTY", "EPSG:3857", g);
        CHECK_RC(tapi_geo_info_get(sess.pco, g, &info));
        if (!info.empty || !isnan(info.bbox.minx))
            TEST_VERDICT("An empty polygon reports bounds");
        CHECK_RC(tapi_geo_centroid(sess.pco, g, &c));
        CHECK_RC(tapi_geo_info_get(sess.pco, c, &info));
        if (!info.empty)
            TEST_VERDICT("The centroid of an empty polygon is not empty");
        tapi_geo_geometry_free(sess.pco, g);
        tapi_geo_geometry_free(sess.pco, c);
    }
    tsapi_mi_graph(mi, "centroid", "Centroid coordinates per fixture", NULL,
                   "centroid_x,centroid_y");

    TEST_SUCCESS;

cleanup:
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
