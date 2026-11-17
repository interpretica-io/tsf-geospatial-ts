/** @file
 * @brief services group prologue
 *
 * Start the OGC servers on the agent: the suite's control server, and
 * MapServer and pygeoapi where installed, serving a grid written by the
 * module under test.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */
#define TE_TEST_NAME    "services/prologue"
#include "te_config.h"
#include "tapi_test.h"

#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    te_mi_logger *mi = NULL;

    TEST_START;
    TEST_STEP("Start the OGC servers");
    TSAPI_GEO_SESSION(&sess, "pco_svc_prologue");
    if ((sess.engine.features & TAPI_GEO_FEAT_CURL) == 0)
        RING("The engine has no libcurl: service checks will skip");
    CHECK_RC(tsapi_geo_ogc_servers_up(&sess));
    mi = tsapi_mi_open();
    tsapi_mi_put(mi, "up", tsapi_geo_ogc_server_up(&sess, "control"));
    tsapi_mi_put(mi, "up", tsapi_geo_ogc_server_up(&sess, "mapserver"));
    tsapi_mi_put(mi, "up", tsapi_geo_ogc_server_up(&sess, "pygeoapi"));
    tsapi_mi_graph(mi, "up", "Servers up: control, MapServer, pygeoapi",
                   NULL, "up");
    TEST_SUCCESS;
cleanup:
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
