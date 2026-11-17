/** @file
 * @brief crs group
 *
 * Two definitions are the same CRS, the same but for the axis order, or
 * different. EPSG:4326 and OGC:CRS84 differ only in axis order - the
 * classic source of swapped coordinates - and must be told apart from
 * truly equivalent spellings and from different datums.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "crs/compare"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo_crs.h"
#include "tapi_geo_assert.h"
#include "tsapi_geo.h"

static const struct {
    const char *a;
    const char *b;
    bool equivalent;
    bool axis_swapped;
    bool same_datum;
} cases[] = {
    { "EPSG:4326", "urn:ogc:def:crs:EPSG::4326", true, false, true },
    { "EPSG:4326", "OGC:CRS84", false, true, true },
    { "EPSG:4326", "EPSG:4258", false, false, false },
    { "EPSG:3857", "EPSG:3395", false, false, true },
    { "EPSG:32629", "+proj=utm +zone=29 +datum=WGS84 +units=m +no_defs "
      "+type=crs", true, false, true },
};

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    te_mi_logger *mi = NULL;
    unsigned int i;

    TEST_START;

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_crs_cmp");
    mi = tsapi_mi_open();

    for (i = 0; i < TE_ARRAY_LEN(cases); i++)
    {
        tapi_geo_crs_cmp c;

        TEST_SUBSTEP("%s vs %s", cases[i].a, cases[i].b);
        CHECK_RC(tapi_geo_crs_compare(sess.pco, cases[i].a, cases[i].b, &c));
        RING("%s vs %s: strict %d, equivalent %d, except axis %d, swapped %d, "
             "same datum %d", cases[i].a, cases[i].b, c.strict, c.equivalent,
             c.equivalent_except_axis, c.axis_swapped, c.same_geodetic);
        if (c.equivalent != cases[i].equivalent)
            TEST_VERDICT("%s and %s are %sequivalent", cases[i].a,
                         cases[i].b, c.equivalent ? "" : "not ");
        if (c.axis_swapped != cases[i].axis_swapped)
            TEST_VERDICT("%s vs %s: axis swap %sdetected", cases[i].a,
                         cases[i].b, c.axis_swapped ? "" : "not ");
        if (c.same_geodetic != cases[i].same_datum)
            TEST_VERDICT("%s vs %s: datum %sthe same", cases[i].a,
                         cases[i].b, c.same_geodetic ? "" : "not ");
        tsapi_mi_put(mi, "equivalent", c.equivalent);
        tsapi_mi_put(mi, "axis_swapped", c.axis_swapped);
    }

    TEST_STEP("The compatibility assertion holds for the same CRS");
    TAPI_GEO_ASSERT(tapi_geo_assert_crs_compatible(sess.pco, "EPSG:4326",
                                                   "urn:ogc:def:crs:EPSG::4326",
                                                   "same CRS"));
    tsapi_mi_graph(mi, "cmp", "Equivalent / axis-swapped per pair", NULL,
                   "equivalent,axis_swapped");

    TEST_SUCCESS;

cleanup:
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
