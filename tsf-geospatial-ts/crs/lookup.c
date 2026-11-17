/** @file
 * @brief crs group
 *
 * What the engine says a CRS is: its type, its axis order, its unit,
 * its datum, its area of use - for definitions given every way PROJ
 * reads them (EPSG code, OGC name, URN, WKT2 of itself).
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "crs/lookup"

#include "te_config.h"
#include "tapi_test.h"

#include <math.h>

#include "tapi_geo_crs.h"
#include "tapi_geo_assert.h"
#include "tsapi_geo.h"

static const struct {
    const char *crs;
    const char *type;
    bool lat_first;
    const char *unit;
    unsigned int axes;
    const char *code;
} cases[] = {
    { "EPSG:4326", "geographic2d", true, "degree", 2, "4326" },
    { "OGC:CRS84", "geographic2d", false, "degree", 2, "CRS84" },
    { "EPSG:4979", "geographic3d", true, "degree", 3, "4979" },
    { "EPSG:3857", "projected", false, "metre", 2, "3857" },
    { "EPSG:32629", "projected", false, "metre", 2, "32629" },
    { "EPSG:27700", "projected", false, "metre", 2, "27700" },
    { "EPSG:2227", "projected", false, "US survey foot", 2, "2227" },
    { "EPSG:32661", "projected", true, "metre", 2, "32661" },
    { "urn:ogc:def:crs:EPSG::3763", "projected", false, "metre", 2, "3763" },
};

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *crs;
    tapi_geo_crs_info info;
    tapi_geo_crs_info again;
    te_string wkt2 = TE_STRING_INIT;
    te_mi_logger *mi = NULL;
    unsigned int i;
    int c = -1;

    TEST_START;
    TEST_GET_STRING_PARAM(crs);
    for (i = 0; i < TE_ARRAY_LEN(cases); i++)
        if (strcmp(cases[i].crs, crs) == 0)
            c = (int)i;
    if (c < 0)
        TEST_FAIL("No expectations for %s", crs);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_crs_lookup");
    mi = tsapi_mi_open();

    TEST_STEP("Look %s up", crs);
    CHECK_RC(tapi_geo_crs_lookup(sess.pco, crs, &info, &wkt2));
    RING("%s: %s (%s), %u axes, %s first: %s | %s | %s; datum %s; area of "
         "use %g %g %g %g (%s)", crs, info.name, info.type, info.axes,
         info.lat_first ? "north" : "east", info.axis[0], info.axis[1],
         info.axis[2], info.datum, info.area_of_use.minx,
         info.area_of_use.miny, info.area_of_use.maxx, info.area_of_use.maxy,
         info.area_name);
    if (strcmp(info.type, cases[c].type) != 0)
        TEST_VERDICT("%s is a %s, expected %s", crs, info.type,
                     cases[c].type);
    if (info.lat_first != cases[c].lat_first)
        TEST_VERDICT("%s axis order: %s first, expected %s first", crs,
                     info.lat_first ? "north" : "east",
                     cases[c].lat_first ? "north" : "east");
    if (strcmp(info.unit, cases[c].unit) != 0)
        TEST_VERDICT("%s unit '%s', expected %s", crs, info.unit,
                     cases[c].unit);
    if (info.axes != cases[c].axes)
        TEST_VERDICT("%s has %u axes, expected %u", crs, info.axes,
                     cases[c].axes);
    if (strcmp(info.code, cases[c].code) != 0)
        TEST_VERDICT("%s is identified as %s:%s", crs, info.auth, info.code);
    if (!(info.area_of_use.minx >= -180 && info.area_of_use.maxx <= 180 &&
          info.area_of_use.miny >= -90 && info.area_of_use.maxy <= 90))
        TEST_VERDICT("%s has no sensible area of use", crs);

    TEST_STEP("Its own WKT2 describes the same CRS");
    CHECK_RC(tapi_geo_crs_lookup(sess.pco, wkt2.ptr, &again, NULL));
    if (strcmp(again.name, info.name) != 0 || again.lat_first !=
        info.lat_first || strcmp(again.unit, info.unit) != 0)
        TEST_VERDICT("%s read back from its WKT2 is %s", crs, again.name);
    TAPI_GEO_ASSERT(tapi_geo_assert_crs_compatible(sess.pco, crs, wkt2.ptr,
                                                   "WKT2 round trip"));
    tsapi_mi_put(mi, "aou_west", info.area_of_use.minx);
    tsapi_mi_put(mi, "aou_south", info.area_of_use.miny);
    tsapi_mi_put(mi, "aou_east", info.area_of_use.maxx);
    tsapi_mi_put(mi, "aou_north", info.area_of_use.maxy);
    tsapi_mi_graph(mi, "aou", "Area of use, degrees", NULL,
                   "aou_west,aou_south,aou_east,aou_north");

    TEST_SUCCESS;

cleanup:
    te_string_free(&wkt2);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
