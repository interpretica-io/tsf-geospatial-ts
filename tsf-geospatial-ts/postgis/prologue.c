/** @file
 * @brief postgis group prologue: start PostGIS in a container
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */
#define TE_TEST_NAME    "postgis/prologue"
#include "te_config.h"
#include "tapi_test.h"

#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };

    TEST_START;
    TEST_STEP("Start PostGIS on 127.0.0.1:%d", TSAPI_GEO_PG_PORT);
    TSAPI_GEO_SESSION(&sess, "pco_pg_prologue");
    if (tsapi_geo_postgis_up(&sess) != 0)
        WARN("PostGIS did not start (no docker on the agent?): the postgis "
             "tests will skip");
    TEST_SUCCESS;
cleanup:
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
