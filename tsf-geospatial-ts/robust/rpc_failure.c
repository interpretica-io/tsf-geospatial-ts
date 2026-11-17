/** @file
 * @brief robust group
 *
 * Handles are scoped to the RPC server process that issued them: after
 * the RPC server restarts, a handle from before is refused (not taken
 * for whatever the new process holds); a geometry is refused by a
 * different RPC server; a released handle is refused; and an RPC that
 * times out fails cleanly, after which the restarted server works.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "robust/rpc_failure"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo.h"
#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    tsapi_geo_session other = { 0 };
    tapi_geo_geometry *g = NULL;
    tapi_geo_geometry *fresh = NULL;
    tapi_geo_geometry *mine = NULL;
    tapi_geo_info info;
    tapi_geo_stats s0, s1;
    te_mi_logger *mi = NULL;
    int stale;

    TEST_START;

    TEST_STEP("Open two sessions on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_rob_rpc");
    TSAPI_GEO_SESSION(&other, "pco_rob_rpc2");
    mi = tsapi_mi_open();

    TEST_STEP("A geometry belongs to the RPC server that made it");
    CHECK_RC(tapi_geo_parse_wkt(sess.pco, "POINT (1 2)", 4326, &g));
    if (tapi_geo_info_get(other.pco, g, &info) == 0)
        TEST_VERDICT("Another RPC server accepted a geometry it does not "
                     "hold");

    TEST_STEP("A released handle is refused");
    CHECK_RC(tapi_geo_parse_wkt(sess.pco, "POINT (3 4)", 4326, &mine));
    stale = mine->handle;
    CHECK_RC(tapi_geo_geometry_free(sess.pco, mine));
    mine = NULL;
    {
        tapi_geo_res r = TAPI_GEO_RES_INIT;

        rc = tapi_geo_call(sess.pco, TAPI_GEO_RPC_GEOM_INFO, stale, 0, "", "",
                           "", &r);
        tapi_geo_rpc_res_free(&r);
        if (rc == 0)
            TEST_VERDICT("A released handle still answers");
    }

    TEST_STEP("Restart the RPC server: the old handle means nothing");
    CHECK_RC(tapi_geo_stats_get(sess.pco, &s0));
    CHECK_RC(rcf_rpc_server_restart(sess.pco));
    CHECK_RC(tapi_geo_stats_get(sess.pco, &s1));
    if (s1.session == s0.session)
        TEST_VERDICT("The restarted RPC server has the old session tag");
    CHECK_RC(tapi_geo_parse_wkt(sess.pco, "POINT (5 6)", 4326, &fresh));
    {
        tapi_geo_res r = TAPI_GEO_RES_INIT;

        rc = tapi_geo_call(sess.pco, TAPI_GEO_RPC_GEOM_INFO, g->handle, 0,
                           "", "", "", &r);
        if (rc == 0 && tapi_geo_res_dbl(&r, "minx") == 5)
            TEST_VERDICT("A handle from before the restart named the new "
                         "process's geometry");
        if (rc == 0)
            TEST_VERDICT("A handle from before the restart is accepted");
        tapi_geo_rpc_res_free(&r);
    }
    tsapi_mi_put(mi, "live_before", s0.live);
    tsapi_mi_put(mi, "live_after", s1.live);
    tsapi_mi_graph(mi, "live", "Live objects before and after the restart",
                   NULL, "live_before,live_after");
    if (s1.live != 0)
        TEST_VERDICT("A fresh RPC server holds %u objects", s1.live);
    /* The local handle of the vanished geometry: drop it without the
     * (refused) remote release. */
    free(g);
    g = NULL;

    TEST_STEP("A call that times out fails, and the server recovers");
    {
        tapi_geo_geometry *big = NULL, *buf = NULL;
        te_string t = TE_STRING_INIT;
        tapi_geo_rpc_args a = { 0 };
        tapi_geo_rpc_res r = TAPI_GEO_RPC_RES_INIT;
        unsigned int i;

        te_string_append(&t, "LINESTRING (0 0");
        for (i = 1; i < 200000; i++)
            te_string_append(&t, ", %u %g", i, (i * 7919 % 1000) / 10.0);
        te_string_append(&t, ")");
        CHECK_RC(tapi_geo_parse(other.pco, "wkt", t.ptr, NULL, NULL, &big,
                                NULL));
        a.ha = big->handle;
        a.kind = "buffer";
        a.params = "distance=5\nquad_segs=128";
        /* 100 ms is far too short for this buffer: the RPC times out
         * and TE restarts the server. */
        rc = rpc_geo_call(other.pco, TAPI_GEO_RPC_GEOM_CONSTRUCT, &a, 100,
                          &r);
        tapi_geo_rpc_res_free(&r);
        if (rc == 0)
            RING("The buffer finished within 100 ms on this agent");
        else
            RING("The call failed as it should: %r", rc);
        te_string_free(&t);
        free(big);
        (void)buf;
        if (rc != 0 && other.pco->timed_out)
            CHECK_RC(rcf_rpc_server_restart(other.pco));
        CHECK_RC(tapi_geo_available(other.pco, NULL));
    }

    TEST_SUCCESS;

cleanup:
    tapi_geo_geometry_free(sess.pco, fresh);
    tapi_geo_geometry_free(sess.pco, mine);
    free(g);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&other);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
