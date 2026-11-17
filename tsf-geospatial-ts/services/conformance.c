/** @file
 * @brief services group
 *
 * OGC API - Features: the landing page has links, the conformance page
 * declares the Core and GeoJSON classes as JSON, and the collections
 * list answers.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "services/conformance"

#include "te_config.h"
#include "tapi_test.h"

#include "svc.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *target;
    svc_target tg = { .url = TE_STRING_INIT };
    tapi_geo_ogc_result res;
    bool core = false, geojson = false;
    te_mi_logger *mi = NULL;

    TEST_START;
    TEST_GET_STRING_PARAM(target);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_svc_conf");
    SVC_TARGET(&sess, target, &tg);
    mi = tsapi_mi_open();

    TEST_STEP("Landing page, conformance, collections of %s", tg.server);
    CHECK_RC(tapi_geo_ogc_conformance(sess.pco, &tg.t, &core, &geojson,
                                      &res));
    RING("core %d, geojson %d, issues %lld\n%s", core, geojson, res.issues,
         res.records.ptr != NULL ? res.records.ptr : "");
    tsapi_mi_put(mi, "conforms", core);
    tsapi_mi_put(mi, "conforms", geojson);
    tsapi_mi_graph(mi, "conf", "Core, GeoJSON conformance declared", NULL,
                   "conforms");
    if (!res.ok || !core || !geojson)
        TEST_VERDICT("%s does not declare OGC API - Features conformance",
                     tg.server);
    tapi_geo_ogc_result_free(&res);

    TEST_SUCCESS;

cleanup:
    te_string_free(&tg.url);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
