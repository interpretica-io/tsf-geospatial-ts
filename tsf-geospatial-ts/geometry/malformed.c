/** @file
 * @brief geometry group
 *
 * Malformed input is refused with a defined error and a category, never
 * accepted half-way, and leaves the engine as it found it: the RPC
 * server answers afterwards and no object is left behind.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "geometry/malformed"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"

#include "tapi_geo.h"
#include "tsapi_geo.h"

typedef struct bad_input {
    const char *name;
    const char *fmt;
    const char *text;       /* NULL: built in the test */
    te_errno    error;      /* expected TE error code */
    const char *category;   /* expected category, or NULL */
} bad_input;

static const bad_input inputs[] = {
    { "unclosed_ring", "wkt", "POLYGON ((0 0, 1 0, 1 1, 0 1))",
      TE_ECORRUPTED, "unclosed_ring" },
    { "truncated_wkt", "wkt", "POLYGON ((0 0, 1 0, 1 1, 0 0)", TE_ECORRUPTED,
      NULL },
    { "too_few_points", "wkt", "LINESTRING (1 2)", TE_ECORRUPTED, NULL },
    { "unknown_type", "wkt", "TRIANGLE ((0 0, 1 0, 0 1, 0 0))", TE_ECORRUPTED,
      NULL },
    { "odd_hex", "wkb", "010100000", TE_ECORRUPTED, NULL },
    { "truncated_wkb", "wkb", "0101000000000000000000F03F", TE_ECORRUPTED,
      NULL },
    /* A line claiming 2^31 points in 9 bytes. */
    { "huge_count", "wkb", "010200000000000080", TE_ECORRUPTED, NULL },
    { "bad_byte_order", "wkb", "07010000000000000000000000000000000000F03F",
      TE_ECORRUPTED, NULL },
    { "trailing_bytes", "wkb",
      "0101000000000000000000F03F000000000000F03FDEADBEEF", TE_ECORRUPTED,
      NULL },
    { "bad_json", "geojson", "{\"type\":\"Point\",\"coordinates\":[1,",
      TE_ECORRUPTED, NULL },
    { "unknown_json_type", "geojson",
      "{\"type\":\"Pointless\",\"coordinates\":[1,2]}", TE_ECORRUPTED, NULL },
    { "deep_nesting", "wkt", NULL, TE_E2BIG, NULL },
    { "empty_text", "wkt", "   ", TE_ECORRUPTED, NULL },
};

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *input;
    const bad_input *b = NULL;
    tapi_geo_geometry *g = NULL;
    tapi_geo_stats before, after;
    te_string text = TE_STRING_INIT;
    te_string cat = TE_STRING_INIT;
    te_mi_logger *mi = NULL;
    unsigned int i;

    TEST_START;
    TEST_GET_STRING_PARAM(input);
    for (i = 0; i < TE_ARRAY_LEN(inputs); i++)
        if (strcmp(inputs[i].name, input) == 0)
            b = &inputs[i];
    if (b == NULL)
        TEST_FAIL("Unknown input %s", input);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_geom_malformed");
    mi = tsapi_mi_open();
    CHECK_RC(tapi_geo_stats_get(sess.pco, &before));

    if (b->text != NULL)
        te_string_append(&text, "%s", b->text);
    else
    {
        /* 1000 nested collections: deeper than any limit, and deep
         * enough to blow the stack of a recursive parser. */
        for (i = 0; i < 1000; i++)
            te_string_append(&text, "GEOMETRYCOLLECTION (");
        te_string_append(&text, "POINT (1 1)");
        for (i = 0; i < 1000; i++)
            te_string_append(&text, ")");
    }

    TEST_STEP("Parse '%s' as %s: refused", b->name, b->fmt);
    rc = tapi_geo_parse(sess.pco, b->fmt, text.ptr, NULL, NULL, &g, &cat);
    if (rc == 0)
        TEST_VERDICT("Malformed input %s was accepted", b->name);
    if (TE_RC_GET_ERROR(rc) != (unsigned int)b->error)
        TEST_VERDICT("Malformed input %s refused with %r, expected %s",
                     b->name, rc, te_rc_err2str(b->error));
    RING("Refused: %s", tapi_geo_last_error());
    if (b->category != NULL && strcmp(cat.ptr != NULL ? cat.ptr : "",
                                      b->category) != 0)
        TEST_VERDICT("Malformed input %s reported as '%s', not %s", b->name,
                     cat.ptr != NULL ? cat.ptr : "", b->category);

    TEST_STEP("The engine still answers and holds no new object");
    CHECK_RC(tapi_geo_available(sess.pco, NULL));
    CHECK_RC(tapi_geo_stats_get(sess.pco, &after));
    if (after.live != before.live)
        TEST_VERDICT("A refused input left %d object(s) behind",
                     (int)after.live - (int)before.live);
    if (after.session != before.session)
        TEST_VERDICT("The RPC server restarted while parsing %s", b->name);
    tsapi_mi_put_type(mi, TE_MI_MEAS_MEMORY, "rss_kb", (double)before.rss_kb,
                      TE_MI_MEAS_MULTIPLIER_KIBI);
    tsapi_mi_put_type(mi, TE_MI_MEAS_MEMORY, "rss_kb", (double)after.rss_kb,
                      TE_MI_MEAS_MULTIPLIER_KIBI);
    tsapi_mi_graph_type(mi, "rss", "RPC server memory before and after the "
                        "refused input", NULL, TE_MI_MEAS_MEMORY, "rss_kb");

    TEST_SUCCESS;

cleanup:
    tapi_geo_geometry_free(sess.pco, g);
    te_string_free(&text);
    te_string_free(&cat);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
