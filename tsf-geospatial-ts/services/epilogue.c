/** @file
 * @brief services group epilogue: stop the OGC servers
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */
#define TE_TEST_NAME    "services/epilogue"
#include "te_config.h"
#include "tapi_test.h"

#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };

    TEST_START;
    TEST_STEP("Stop the OGC servers");
    TSAPI_GEO_SESSION(&sess, "pco_svc_epilogue");
    CHECK_RC(tsapi_geo_ogc_servers_down(&sess));
    TEST_SUCCESS;
cleanup:
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
