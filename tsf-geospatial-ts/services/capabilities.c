/** @file
 * @brief services group
 *
 * GetCapabilities checked for geospatial sense: the document parses, is
 * the version asked for, names its layers with CRS that PROJ knows and
 * geographic boxes that are boxes on the Earth, and (WMS 1.3.0) states
 * per-CRS boxes that agree with the geographic one. MapServer must pass;
 * each control mode must be caught.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "services/capabilities"

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
    te_mi_logger *mi = NULL;
    bool control_bad;

    TEST_START;
    TEST_GET_STRING_PARAM(target);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_svc_caps");
    SVC_TARGET(&sess, target, &tg);
    control_bad = strcmp(tg.mode, "good") != 0;
    mi = tsapi_mi_open();

    TEST_STEP("GetCapabilities of %s %s (%s)", tg.server, tg.service,
              tg.mode);
    CHECK_RC(tapi_geo_ogc_capabilities(sess.pco, &tg.t,
                                       strcmp(tg.server, "mapserver") == 0 &&
                                       strcmp(tg.service, "wms") == 0 ?
                                       "cells" : NULL, NULL, &res));
    RING("%s: HTTP %ld %s, %lld issue(s)\n%s", tg.url.ptr, res.http_status,
         res.content_type, res.issues,
         res.records.ptr != NULL ? res.records.ptr : "");
    tsapi_mi_put(mi, "layers", (double)tapi_geo_res_int(&res.raw, "layers",
                                                        0));
    tsapi_mi_put(mi, "issues", (double)res.issues);
    tsapi_mi_graph(mi, "caps", "Layers advertised, issues found", NULL,
                   "layers,issues");
    if (!control_bad && !res.ok)
        TEST_VERDICT("%s %s capabilities have issues", tg.server,
                     tg.service);
    if (control_bad)
    {
        const char *cat = strcmp(tg.mode, "bad_crs") == 0 ? "crs_unknown" :
                          "bbox";

        if (res.ok || !tapi_geo_ogc_has_issue(&res, cat))
            TEST_VERDICT("The control defect %s was not caught", tg.mode);
    }
    tapi_geo_ogc_result_free(&res);

    TEST_SUCCESS;

cleanup:
    te_string_free(&tg.url);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
