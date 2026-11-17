/** @file
 * @brief crs group
 *
 * Coordinates outside the area where a CRS is defined are counted, and
 * refused when the caller asks for strictness; coordinates a
 * transformation cannot represent (the far side of an orthographic
 * view) are reported non-finite and a geometry made of them is refused.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "crs/domain"

#include "te_config.h"
#include "tapi_test.h"

#include <math.h>

#include "tapi_geo_crs.h"
#include "tsapi_geo.h"

static const struct {
    const char *point;
    const char *dst;
    double lon, lat;
    unsigned int outside;
    bool nonfinite;
} cases[] = {
    /* Porto in Portugal's ETRS89 / PT-TM06. */
    { "inside", "EPSG:3763", -8.61, 41.15, 0, false },
    /* Madrid is beyond PT-TM06's area of use. */
    { "outside", "EPSG:3763", -3.70, 40.42, 1, false },
    /* UTM 1N straddles the antimeridian side: 179.5 E is in zone 60. */
    { "wrong_zone", "EPSG:32601", 179.5, 10, 1, false },
    /* Web Mercator stops at 85.06 degrees; the pole is out of its area
     * of use (it still gets a huge finite northing). */
    { "pole", "EPSG:3857", 0, 90, 1, false },
    /* The far side of the Earth has no orthographic image at all. */
    { "far_side", "+proj=ortho +lat_0=0 +lon_0=0 +type=crs", 120, 0, 0,
      true },
};

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *point;
    tapi_geo_xform_info xi;
    tapi_geo_xform_opts strict = TAPI_GEO_XFORM_OPTS_DEFAULT;
    tapi_geo_geometry *g = NULL;
    tapi_geo_geometry *t = NULL;
    double in[2], out[2];
    te_string wkt = TE_STRING_INIT;
    te_mi_logger *mi = NULL;
    unsigned int i;
    int c = -1;

    TEST_START;
    TEST_GET_STRING_PARAM(point);
    for (i = 0; i < TE_ARRAY_LEN(cases); i++)
        if (strcmp(cases[i].point, point) == 0)
            c = (int)i;
    if (c < 0)
        TEST_FAIL("Unknown point %s", point);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_crs_domain");
    mi = tsapi_mi_open();
    in[0] = cases[c].lon;
    in[1] = cases[c].lat;

    TEST_STEP("Transform (%g %g) into %s", in[0], in[1], cases[c].dst);
    CHECK_RC(tapi_geo_transform_coords(sess.pco, "EPSG:4326", cases[c].dst,
                                       TAPI_GEO_AXIS_GIS, NULL, in, 1, 2,
                                       out, &xi));
    tsapi_mi_put(mi, "out_of_domain", xi.out_of_domain);
    tsapi_mi_put(mi, "nonfinite", xi.nonfinite);
    tsapi_mi_graph(mi, "domain", "Outside the area of use / not finite",
                   NULL, "out_of_domain,nonfinite");
    if (xi.out_of_domain != cases[c].outside)
        TEST_VERDICT("%s: %u coordinate(s) reported outside the area of use",
                     point, xi.out_of_domain);
    if ((xi.nonfinite > 0) != cases[c].nonfinite)
        TEST_VERDICT("%s: %u non-finite result(s)", point, xi.nonfinite);

    TEST_STEP("Strictly, an outside coordinate is refused");
    strict.strict_domain = true;
    rc = tapi_geo_transform_coords(sess.pco, "EPSG:4326", cases[c].dst,
                                   TAPI_GEO_AXIS_GIS, &strict, in, 1, 2, out,
                                   &xi);
    if (cases[c].outside > 0 && TE_RC_GET_ERROR(rc) != TE_EDOM)
        TEST_VERDICT("%s: a strict transformation gave %r, not EDOM", point,
                     rc);
    if (cases[c].outside == 0 && rc != 0)
        TEST_VERDICT("%s: a strict transformation failed", point);

    TEST_STEP("A geometry that cannot be transformed is refused");
    te_string_append(&wkt, "POINT (%.17g %.17g)", in[0], in[1]);
    TSAPI_GEO_WKT(&sess, wkt.ptr, "EPSG:4326", g);
    rc = tapi_geo_transform(sess.pco, g, NULL, cases[c].dst,
                            TAPI_GEO_AXIS_GIS, NULL, &t, &xi);
    if (cases[c].nonfinite && rc == 0)
        TEST_VERDICT("A geometry with no image in %s was transformed",
                     cases[c].dst);
    if (!cases[c].nonfinite && rc != 0)
        TEST_VERDICT("The geometry did not transform: %s",
                     tapi_geo_last_error());

    TEST_SUCCESS;

cleanup:
    tapi_geo_geometry_free(sess.pco, g);
    tapi_geo_geometry_free(sess.pco, t);
    te_string_free(&wkt);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
