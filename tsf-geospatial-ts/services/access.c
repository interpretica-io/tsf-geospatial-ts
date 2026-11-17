/** @file
 * @brief services group
 *
 * Access restrictions are enforced: the protected area answers 401 or
 * 403 to no credentials and to a wrong password, and 200 to the right
 * one. The control mode that lets anyone in must be caught.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "services/access"

#include "te_config.h"
#include "tapi_test.h"

#include "svc.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *mode;
    te_string url = TE_STRING_INIT;
    tapi_geo_ogc_auth a;
    te_mi_logger *mi = NULL;

    TEST_START;
    TEST_GET_STRING_PARAM(mode);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_svc_access");
    if ((sess.engine.features & TAPI_GEO_FEAT_CURL) == 0)
        TEST_SKIP("The engine was built without libcurl");
    if (!tsapi_geo_ogc_server_up(&sess, "control"))
        TEST_SKIP("The control server is not running");
    mi = tsapi_mi_open();
    tsapi_geo_ogc_url("control", "wms", mode, &url);
    te_string_append(&url, "?SERVICE=WMS&REQUEST=GetCapabilities");

    TEST_STEP("Anonymous, wrong password, right password");
    CHECK_RC(tapi_geo_ogc_access(sess.pco, url.ptr, TSAPI_GEO_OGC_USER,
                                 TSAPI_GEO_OGC_PASSWORD, &a));
    RING("%s: %ld / %ld / %ld, enforced %d", url.ptr, a.status_anonymous,
         a.status_wrong_password, a.status_authenticated, a.enforced);
    tsapi_mi_put(mi, "http_status", a.status_anonymous);
    tsapi_mi_put(mi, "http_status", a.status_wrong_password);
    tsapi_mi_put(mi, "http_status", a.status_authenticated);
    tsapi_mi_graph(mi, "http", "HTTP status: anonymous, wrong, right", NULL,
                   "http_status");
    if (a.status_authenticated != 200)
        TEST_VERDICT("The right credentials were refused (%ld)",
                     a.status_authenticated);
    if (strcmp(mode, "auth") == 0 && !a.enforced)
        TEST_VERDICT("Access control is not enforced");
    if (strcmp(mode, "no_auth") == 0 && a.enforced)
        TEST_VERDICT("An open area passed for a protected one");

    TEST_SUCCESS;

cleanup:
    te_string_free(&url);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
