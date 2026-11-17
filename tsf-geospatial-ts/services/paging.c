/** @file
 * @brief services group
 *
 * Pagination returns the same features whatever the page size: read
 * with pages of 7 and of 1000, the two sets agree in count and content
 * (an order-independent digest of id and geometry). The control mode
 * that serves its first page again must be caught.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "services/paging"

#include "te_config.h"
#include "tapi_test.h"

#include "svc.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *target;
    svc_target tg = { .url = TE_STRING_INIT };
    tapi_geo_ogc_features a, b;
    bool consistent = false;
    te_mi_logger *mi = NULL;

    TEST_START;
    TEST_GET_STRING_PARAM(target);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_svc_page");
    SVC_TARGET(&sess, target, &tg);
    mi = tsapi_mi_open();

    TEST_STEP("Read %s %s with pages of 7 and of 1000", tg.server,
              tg.service);
    CHECK_RC(tapi_geo_ogc_paging(sess.pco, &tg.t, NULL, 7, 1000, &consistent,
                                 &a, &b));
    RING("pages of 7: %lld features, %lld duplicates, digest %s; pages of "
         "1000: %lld, %lld, %s", a.features, a.duplicates, a.digest,
         b.features, b.duplicates, b.digest);
    tsapi_mi_put(mi, "features", (double)a.features);
    tsapi_mi_put(mi, "features", (double)b.features);
    tsapi_mi_put(mi, "duplicates", (double)a.duplicates);
    tsapi_mi_put(mi, "duplicates", (double)b.duplicates);
    tsapi_mi_graph(mi, "pages", "Features and duplicates: small pages, "
                   "large pages", NULL, "features,duplicates");
    if (strcmp(tg.mode, "paging_dup") == 0)
    {
        if (consistent)
            TEST_VERDICT("A server repeating its first page passed the "
                         "paging check");
    }
    else if (!consistent)
        TEST_VERDICT("%s returns different features for different page "
                     "sizes", tg.server);

    TEST_SUCCESS;

cleanup:
    te_string_free(&tg.url);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
