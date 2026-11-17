/** @file
 * @brief crs group
 *
 * A definition PROJ does not accept is a result, not a crash: unknown
 * codes, garbage, a coordinate operation where a CRS was wanted. A
 * deprecated code still resolves and says it is deprecated.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "crs/validate"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo_crs.h"
#include "tsapi_geo.h"

static const struct {
    const char *def;
    bool valid;
    bool deprecated;
} cases[] = {
    { "EPSG:4326", true, false },
    { "4326", true, false },
    { "+proj=tmerc +lat_0=39.668 +lon_0=-8.133 +k=1 +x_0=0 +y_0=0 "
      "+ellps=GRS80 +units=m +type=crs", true, false },
    { "EPSG:3785", true, true },
    { "EPSG:999999", false, false },
    { "not a crs at all", false, false },
    { "+proj=pipeline +step +proj=unitconvert +xy_in=deg +xy_out=rad", false,
      false },
    { "GEOGCS[\"broken\"", false, false },
};

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    te_mi_logger *mi = NULL;
    unsigned int i;

    TEST_START;

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_crs_valid");
    mi = tsapi_mi_open();

    for (i = 0; i < TE_ARRAY_LEN(cases); i++)
    {
        te_string why = TE_STRING_INIT;
        bool valid = false;

        TEST_SUBSTEP("%s", cases[i].def);
        CHECK_RC(tapi_geo_crs_validate(sess.pco, cases[i].def, &valid, &why));
        if (valid != cases[i].valid)
            TEST_VERDICT("'%s' is %svalid", cases[i].def,
                         valid ? "" : "not ");
        if (!valid)
            RING("Refused: %s", why.ptr != NULL ? why.ptr : "");
        if (valid)
        {
            tapi_geo_crs_info info;

            CHECK_RC(tapi_geo_crs_lookup(sess.pco, cases[i].def, &info,
                                         NULL));
            if (info.deprecated != cases[i].deprecated)
                TEST_VERDICT("'%s' is %sreported deprecated", cases[i].def,
                             info.deprecated ? "" : "not ");
        }
        tsapi_mi_put(mi, "valid", valid);
        te_string_free(&why);
    }
    TEST_STEP("The engine still answers");
    CHECK_RC(tapi_geo_available(sess.pco, NULL));
    tsapi_mi_graph(mi, "valid", "Accepted (1) / refused (0) per definition",
                   NULL, "valid");

    TEST_SUCCESS;

cleanup:
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
