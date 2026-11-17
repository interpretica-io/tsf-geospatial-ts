/** @file
 * @brief geometry group
 *
 * Repair is explicit: a repaired copy is valid, the original stays as it
 * was (still invalid), and the copy covers the same ground - MakeValid
 * keeps every part of a bow-tie, so its area is the sum of both
 * triangles, not zero.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "geometry/repair"

#include "te_config.h"
#include "tapi_test.h"

#include <math.h>

#include "tapi_geo.h"
#include "tsapi_geo.h"

static const struct {
    const char *name;
    const char *wkt;
    double area;            /* of the repaired geometry, NAN: unchecked */
    tapi_geo_repair_method method;
} cases[] = {
    /* Two triangles of area 1 each, crossing at (1 1). */
    { "bowtie", "POLYGON ((0 0, 2 2, 2 0, 0 2, 0 0))", 2,
      TAPI_GEO_REPAIR_MAKE_VALID },
    { "bowtie_structure", "POLYGON ((0 0, 2 2, 2 0, 0 2, 0 0))", 2,
      TAPI_GEO_REPAIR_MAKE_VALID_STRUCTURE },
    { "hole_outside", "POLYGON ((0 0, 4 0, 4 4, 0 4, 0 0), (5 5, 5 6, 6 6, "
      "5 5))", NAN, TAPI_GEO_REPAIR_MAKE_VALID },
    { "duplicate_vertex", "LINESTRING (0 0, 1 0, 1 0, 1 0, 2 2)", NAN,
      TAPI_GEO_REPAIR_REMOVE_DUPLICATES },
    { "clockwise", "POLYGON ((0 0, 0 1, 1 1, 1 0, 0 0))", 1,
      TAPI_GEO_REPAIR_ORIENT_CCW },
};

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *defect;
    tapi_geo_geometry *g = NULL;
    tapi_geo_geometry *fixed = NULL;
    tapi_geo_validity v;
    tapi_geo_measure m;
    te_mi_logger *mi = NULL;
    unsigned int i;
    int c = -1;

    TEST_START;
    TEST_GET_STRING_PARAM(defect);
    for (i = 0; i < TE_ARRAY_LEN(cases); i++)
        if (strcmp(cases[i].name, defect) == 0)
            c = (int)i;
    if (c < 0)
        TEST_FAIL("Unknown case %s", defect);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_geom_repair");
    mi = tsapi_mi_open();
    TSAPI_GEO_WKT(&sess, cases[c].wkt, "EPSG:3857", g);

    TEST_STEP("Repair a copy");
    CHECK_RC(tapi_geo_repair(sess.pco, g, cases[c].method, &fixed));

    TEST_STEP("The copy is clean, the original untouched");
    CHECK_RC(tapi_geo_validate(sess.pco, fixed, "all", strcmp(defect, "clockwise") == 0 ? "orientation=ccw" : NULL,
                               &v));
    if (!v.ok)
        TEST_VERDICT("The repaired %s still has issues: %s", defect,
                     v.category);
    tapi_geo_validity_free(&v);
    CHECK_RC(tapi_geo_validate(sess.pco, g, "all", strcmp(defect, "clockwise") == 0 ? "orientation=ccw" : NULL, &v));
    if (v.ok)
        TEST_VERDICT("The original %s became clean: repair was not on a "
                     "copy", defect);
    tsapi_mi_put(mi, "issues", v.issues);
    tapi_geo_validity_free(&v);
    CHECK_RC(tapi_geo_validate(sess.pco, fixed, "all", strcmp(defect, "clockwise") == 0 ? "orientation=ccw" : NULL,
                               &v));
    tsapi_mi_put(mi, "issues", v.issues);
    tapi_geo_validity_free(&v);
    tsapi_mi_graph(mi, "issues", "Issues before and after repair", NULL,
                   "issues");

    if (!isnan(cases[c].area))
    {
        TEST_STEP("The copy covers the same ground");
        CHECK_RC(tapi_geo_area(sess.pco, fixed, TAPI_GEO_PLANAR,
                               TAPI_GEO_AXIS_GIS, &m));
        if (!tsapi_geo_near(m.value, cases[c].area, 1e-9))
            TEST_VERDICT("The repaired %s has area %.17g, expected %g",
                         defect, m.value, cases[c].area);
    }

    TEST_SUCCESS;

cleanup:
    tapi_geo_geometry_free(sess.pco, g);
    tapi_geo_geometry_free(sess.pco, fixed);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
