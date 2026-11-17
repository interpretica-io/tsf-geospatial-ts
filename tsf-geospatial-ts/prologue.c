/** @file
 * @brief Test Suite
 *
 * Test Suite prologue: record what the agent's geospatial engine is -
 * GEOS, PROJ and GDAL versions, features, and the GDAL drivers that can
 * read and write vector and raster data.
 *
 * Copyright (C) 2025-2026 Interpretica Unipessoal Lda
 *
 * @author Maxim Menshikov <maxim.menshikov@interpretica.io>
 */

#define TE_TEST_NAME    "prologue"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo.h"
#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    tapi_geo_res r = TAPI_GEO_RES_INIT;
    te_mi_logger *mi = NULL;
    char *cur;
    char *f[8];
    unsigned int vr = 0, vw = 0, rr = 0, rw = 0;

    TEST_START;

    TEST_STEP("What is the agent's geospatial engine");
    TSAPI_GEO_SESSION(&sess, "pco_prologue");
    CHECK_RC(tapi_geo_call(sess.pco, TAPI_GEO_RPC_AVAILABLE, 0, 0, "", "", "",
                           &r));
    RING("Engine:\n%s", r.result.ptr);
    cur = r.report.ptr;
    while (tapi_geo_report_next(&cur, f, 8) == 6)
    {
        bool vec = strcmp(f[2], "1") == 0, ras = strcmp(f[3], "1") == 0;
        bool wr = strcmp(f[4], "1") == 0 || strcmp(f[5], "1") == 0;

        vr += vec;
        rr += ras;
        vw += vec && wr;
        rw += ras && wr;
    }
    mi = tsapi_mi_open();
    tsapi_mi_put(mi, "drivers", vr);
    tsapi_mi_put(mi, "drivers", vw);
    tsapi_mi_put(mi, "drivers", rr);
    tsapi_mi_put(mi, "drivers", rw);
    tsapi_mi_graph(mi, "drivers", "GDAL drivers: vector read, vector write, "
                   "raster read, raster write", NULL, "drivers");
    if ((sess.engine.features & (TAPI_GEO_FEAT_GEOS | TAPI_GEO_FEAT_PROJ |
                                 TAPI_GEO_FEAT_GDAL)) !=
        (TAPI_GEO_FEAT_GEOS | TAPI_GEO_FEAT_PROJ | TAPI_GEO_FEAT_GDAL))
        TEST_VERDICT("The engine lacks GEOS, PROJ or GDAL");

    TEST_SUCCESS;

cleanup:
    tapi_geo_rpc_res_free(&r);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
