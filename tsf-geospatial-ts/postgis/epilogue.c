/** @file
 * @brief postgis group epilogue: remove the PostGIS container
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */
#define TE_TEST_NAME    "postgis/epilogue"
#include "te_config.h"
#include "tapi_test.h"

#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };

    TEST_START;
    TEST_STEP("Remove the PostGIS container");
    TSAPI_GEO_SESSION(&sess, "pco_pg_epilogue");
    tsapi_geo_postgis_down(&sess);
    TEST_SUCCESS;
cleanup:
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
