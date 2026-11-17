/** @file
 * @brief generation group
 *
 * Every generator is a function of its options: the same seed gives the
 * same items byte for byte (and the same digest), another seed gives
 * other items, and identifiers run start_id, start_id + 1, ... in order.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "generation/determinism"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo.h"
#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    tsapi_geo_session other = { 0 };
    const char *kind;
    te_string p1 = TE_STRING_INIT, p2 = TE_STRING_INIT;
    te_string d1 = TE_STRING_INIT, d2 = TE_STRING_INIT, d3 = TE_STRING_INIT;
    tapi_geo_gen_item *a = NULL, *b = NULL, *c = NULL;
    unsigned int na = 0, nb = 0, nc = 0, i;
    te_mi_logger *mi = NULL;
    const char *extra;

    TEST_START;
    TEST_GET_STRING_PARAM(kind);
    extra = strcmp(kind, "invalid") == 0 ? "allow_invalid=1\n" :
            strcmp(kind, "grid") == 0 ? "nx=12\nny=9\n" :
            strcmp(kind, "trajectory") == 0 ? "turn=3\nnoise=2\n" : "";

    TEST_STEP("Open two sessions: the same output must come from both");
    TSAPI_GEO_SESSION(&sess, "pco_gen_det");
    TSAPI_GEO_SESSION(&other, "pco_gen_det2");
    mi = tsapi_mi_open();

    te_string_append(&p1, "%scount=100\nseed=1234\nstart_id=500", extra);
    te_string_append(&p2, "%scount=100\nseed=1235\nstart_id=500", extra);
    CHECK_RC(tapi_geo_generate(sess.pco, kind, p1.ptr, &a, &na, &d1));
    CHECK_RC(tapi_geo_generate(other.pco, kind, p1.ptr, &b, &nb, &d2));
    CHECK_RC(tapi_geo_generate(sess.pco, kind, p2.ptr, &c, &nc, &d3));
    RING("%s: %u items, digests %s / %s / other seed %s", kind, na, d1.ptr,
         d2.ptr, d3.ptr);
    if (na == 0 || na != nb)
        TEST_VERDICT("%s made %u and %u items from the same options", kind,
                     na, nb);
    if (strcmp(d1.ptr, d2.ptr) != 0)
        TEST_VERDICT("%s: the same seed gave different output", kind);
    for (i = 0; i < na; i++)
    {
        if (strcmp(a[i].text, b[i].text) != 0 || a[i].id != b[i].id ||
            strcmp(a[i].expect, b[i].expect) != 0)
            TEST_VERDICT("%s: item %u differs between two runs", kind, i);
        if (a[i].id != 500 + (long long)i)
            TEST_VERDICT("%s: item %u has id %lld", kind, i, a[i].id);
        tsapi_mi_put(mi, "text_bytes", (double)strlen(a[i].text));
    }
    /* Boundary points and grids do not depend on the seed. */
    if (strcmp(kind, "boundary") != 0 && strcmp(kind, "grid") != 0 &&
        strcmp(d1.ptr, d3.ptr) == 0)
        TEST_VERDICT("%s: another seed gave the same output", kind);
    tsapi_mi_graph(mi, "size", "Text size of each generated item", NULL,
                   "text_bytes");

    TEST_SUCCESS;

cleanup:
    tapi_geo_gen_items_free(a, na);
    tapi_geo_gen_items_free(b, nb);
    tapi_geo_gen_items_free(c, nc);
    te_string_free(&p1);
    te_string_free(&p2);
    te_string_free(&d1);
    te_string_free(&d2);
    te_string_free(&d3);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&other);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
