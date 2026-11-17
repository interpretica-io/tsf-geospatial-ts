/** @file
 * @brief geometry group
 *
 * One DE-9IM predicate over a table of geometry pairs whose answer is
 * known by construction (squares sharing an edge, a point on a boundary,
 * a line crossing a polygon...). Every pair is asked both ways round.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "geometry/predicates"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo.h"
#include "tsapi_geo.h"

#define SQ      "POLYGON ((0 0, 4 0, 4 4, 0 4, 0 0))"

static const struct {
    const char *a;
    const char *b;
    /* intersects contains within touches crosses overlaps disjoint
     * equals covers covered_by */
    const char *truth;
} pairs[] = {
    { SQ, "POINT (2 2)",                                    "1100000010" },
    { SQ, "POINT (4 2)",                                    "1001000010" },
    { SQ, "POINT (5 5)",                                    "0000001000" },
    { SQ, "POLYGON ((4 0, 8 0, 8 4, 4 4, 4 0))",            "1001000000" },
    { SQ, "POLYGON ((2 2, 6 2, 6 6, 2 6, 2 2))",            "1000010000" },
    { SQ, "POLYGON ((1 1, 2 1, 2 2, 1 2, 1 1))",            "1100000010" },
    { SQ, "POLYGON ((0 4, 4 4, 4 0, 0 0, 0 4))",            "1110000111" },
    { SQ, "LINESTRING (-1 2, 5 2)",                         "1000100000" },
    { SQ, "LINESTRING (1 1, 3 3)",                          "1100000010" },
    { SQ, "LINESTRING (0 0, 4 0)",                          "1001000010" },
    { "LINESTRING (0 0, 4 4)", "LINESTRING (0 4, 4 0)",     "1000100000" },
    { "LINESTRING (0 0, 4 4)", "LINESTRING (2 2, 6 6)",     "1000010000" },
};

static const char *const names[] = {
    "intersects", "contains", "within", "touches", "crosses", "overlaps",
    "disjoint", "equals", "covers", "covered_by"
};

/* The converse of a predicate: what b P' a means when a P b. */
static int
converse(int k)
{
    switch (k)
    {
        case 1: return 2;
        case 2: return 1;
        case 8: return 9;
        case 9: return 8;
        default: return k;
    }
}

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *predicate;
    int k = -1;
    unsigned int i;
    te_mi_logger *mi = NULL;
    unsigned int hits = 0;

    TEST_START;
    TEST_GET_STRING_PARAM(predicate);
    for (i = 0; i < TE_ARRAY_LEN(names); i++)
        if (strcmp(names[i], predicate) == 0)
            k = (int)i;
    if (k < 0)
        TEST_FAIL("Unknown predicate %s", predicate);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_geom_pred");
    mi = tsapi_mi_open();

    TEST_STEP("Ask %s of %u pairs, both ways round", predicate,
              (unsigned)TE_ARRAY_LEN(pairs));
    for (i = 0; i < TE_ARRAY_LEN(pairs); i++)
    {
        tapi_geo_geometry *a = NULL;
        tapi_geo_geometry *b = NULL;
        bool v = false;
        bool want = pairs[i].truth[k] == '1';
        bool want_rev = pairs[i].truth[converse(k)] == '1';
        char de9im[10];

        TSAPI_GEO_WKT(&sess, pairs[i].a, "EPSG:3857", a);
        TSAPI_GEO_WKT(&sess, pairs[i].b, "EPSG:3857", b);
        CHECK_RC(tapi_geo_predicate(sess.pco, predicate, a, b, NULL, &v,
                                    de9im));
        if (v != want)
            TEST_VERDICT("%s is %s for pair %u (DE-9IM %s), expected %s",
                         predicate, v ? "true" : "false", i, de9im,
                         want ? "true" : "false");
        CHECK_RC(tapi_geo_predicate(sess.pco, names[converse(k)], b, a, NULL,
                                    &v, NULL));
        if (v != want)
            TEST_VERDICT("%s and its converse disagree for pair %u",
                         predicate, i);
        CHECK_RC(tapi_geo_predicate(sess.pco, predicate, b, a, NULL, &v,
                                    NULL));
        if (v != want_rev)
            TEST_VERDICT("%s of the swapped pair %u is %s, expected %s",
                         predicate, i, v ? "true" : "false",
                         want_rev ? "true" : "false");
        hits += want;
        tsapi_mi_put(mi, "true", v ? 1 : 0);
        tapi_geo_geometry_free(sess.pco, a);
        tapi_geo_geometry_free(sess.pco, b);
    }
    RING("%s held for %u of %u pairs", predicate, hits,
         (unsigned)TE_ARRAY_LEN(pairs));
    tsapi_mi_graph(mi, "truth", "Predicate value of each swapped pair", NULL,
                   "true");

    TEST_SUCCESS;

cleanup:
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
