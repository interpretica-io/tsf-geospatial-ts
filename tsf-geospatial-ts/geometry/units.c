/** @file
 * @brief geometry group
 *
 * Planar measures of geographic coordinates are degrees, and the engine
 * says so: the unit is "degree", the result is flagged geographic, and a
 * caller that needs metres (require_linear=1) is refused rather than
 * handed degrees. A planar measure never claims metres for a CRS whose
 * unit is something else.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "geometry/units"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo.h"
#include "tsapi_geo.h"

static const struct {
    const char *crs;
    const char *unit;
    bool geographic;
} crs[] = {
    { "EPSG:4326", "degree", true },
    { "EPSG:4258", "degree", true },
    { "EPSG:3857", "metre", false },
    { "EPSG:32629", "metre", false },
    { "EPSG:2227", "US survey foot", false },
    { "EPSG:27700", "metre", false },
};

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    te_mi_logger *mi = NULL;
    unsigned int i;

    TEST_START;

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_geom_units");
    mi = tsapi_mi_open();

    for (i = 0; i < TE_ARRAY_LEN(crs); i++)
    {
        tapi_geo_geometry *a = NULL, *b = NULL;
        tapi_geo_measure m;

        TEST_SUBSTEP("Planar distance in %s", crs[i].crs);
        TSAPI_GEO_WKT(&sess, "POINT (0 0)", crs[i].crs, a);
        TSAPI_GEO_WKT(&sess, "POINT (3 4)", crs[i].crs, b);
        CHECK_RC(tapi_geo_distance(sess.pco, a, b, TAPI_GEO_PLANAR,
                                   TAPI_GEO_AXIS_GIS, &m));
        if (strcmp(m.unit, crs[i].unit) != 0)
            TEST_VERDICT("A planar distance in %s is in '%s', expected %s",
                         crs[i].crs, m.unit, crs[i].unit);
        if (m.geographic != crs[i].geographic)
            TEST_VERDICT("A planar distance in %s is %sflagged geographic",
                         crs[i].crs, m.geographic ? "" : "not ");
        tsapi_mi_put(mi, "planar_distance", m.value);

        rc = tapi_geo_measure_any(sess.pco, "distance", a, b,
                                  TAPI_GEO_PLANAR, TAPI_GEO_AXIS_GIS,
                                  "require_linear=1", &m);
        if (crs[i].geographic && rc == 0)
            TEST_VERDICT("A planar distance of geographic coordinates was "
                         "given where metres were required");
        if (!crs[i].geographic && rc != 0)
            TEST_VERDICT("A planar distance in %s was refused: %s",
                         crs[i].crs, tapi_geo_last_error());

        TEST_SUBSTEP("Geodesic distance from %s", crs[i].crs);
        CHECK_RC(tapi_geo_distance(sess.pco, a, b, TAPI_GEO_GEODESIC,
                                   TAPI_GEO_AXIS_GIS, &m));
        if (strcmp(m.unit, "m") != 0)
            TEST_VERDICT("A geodesic distance is in '%s'", m.unit);
        tsapi_mi_put(mi, "geodesic_distance_m", m.value);
        tapi_geo_geometry_free(sess.pco, a);
        tapi_geo_geometry_free(sess.pco, b);
    }

    TEST_STEP("Without a CRS there is no unit and no geodesic");
    {
        tapi_geo_geometry *a = NULL, *b = NULL;
        tapi_geo_measure m;

        TSAPI_GEO_WKT(&sess, "POINT (0 0)", NULL, a);
        TSAPI_GEO_WKT(&sess, "POINT (3 4)", NULL, b);
        CHECK_RC(tapi_geo_distance(sess.pco, a, b, TAPI_GEO_PLANAR,
                                   TAPI_GEO_AXIS_GIS, &m));
        if (strcmp(m.unit, "crs_unit") != 0)
            TEST_VERDICT("A distance with no CRS claims the unit '%s'",
                         m.unit);
        if (tapi_geo_distance(sess.pco, a, b, TAPI_GEO_GEODESIC,
                              TAPI_GEO_AXIS_GIS, &m) == 0)
            TEST_VERDICT("A geodesic distance was computed with no CRS");
        tapi_geo_geometry_free(sess.pco, a);
        tapi_geo_geometry_free(sess.pco, b);
    }
    tsapi_mi_graph(mi, "dist", "Planar (CRS units) and geodesic (m) "
                   "distance of (0 0)-(3 4) per CRS", NULL,
                   "planar_distance,geodesic_distance_m");

    TEST_SUCCESS;

cleanup:
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
