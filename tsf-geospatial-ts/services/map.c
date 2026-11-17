/** @file
 * @brief services group
 *
 * WMS GetMap: the answer is an image (not an XML exception sent with
 * 200), of the format and size asked for, and not blank. MapServer
 * renders the grid; the control's exception and wrong-size modes must
 * be caught.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "services/map"

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

    TEST_START;
    TEST_GET_STRING_PARAM(target);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_svc_map");
    SVC_TARGET(&sess, target, &tg);
    mi = tsapi_mi_open();

    TEST_STEP("GetMap 320 x 160 of the grid, EPSG:4326 (lat, lon box)");
    CHECK_RC(tapi_geo_ogc_map(sess.pco, &tg.t, "cells", "EPSG:4326",
                              "41.1,-8.7,41.6,-7.7", 320, 160, &res));
    RING("HTTP %ld, %s, %lld bytes, blank %lld, issues %lld\n%s",
         res.http_status, res.content_type,
         tapi_geo_res_int(&res.raw, "bytes", 0),
         tapi_geo_res_int(&res.raw, "blank", -1), res.issues,
         res.records.ptr != NULL ? res.records.ptr : "");
    tsapi_mi_put_type(mi, TE_MI_MEAS_MEMORY, "bytes",
                      (double)tapi_geo_res_int(&res.raw, "bytes", 0),
                      TE_MI_MEAS_MULTIPLIER_PLAIN);
    tsapi_mi_graph_type(mi, "size", "Size of the map image", NULL,
                        TE_MI_MEAS_MEMORY, "bytes");
    if (strcmp(tg.mode, "good") == 0)
    {
        if (!res.ok)
            TEST_VERDICT("%s GetMap has issues", tg.server);
        if (tapi_geo_res_int(&res.raw, "blank", 1) != 0)
            TEST_VERDICT("%s rendered a blank map", tg.server);
    }
    else if (res.ok || !tapi_geo_ogc_has_issue(&res,
                                               strcmp(tg.mode,
                                                      "xml_exception") == 0 ?
                                               "exception" : "image"))
        TEST_VERDICT("The control defect %s was not caught", tg.mode);
    tapi_geo_ogc_result_free(&res);

    TEST_SUCCESS;

cleanup:
    te_string_free(&tg.url);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
