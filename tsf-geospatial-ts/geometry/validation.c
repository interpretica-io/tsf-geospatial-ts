/** @file
 * @brief geometry group
 *
 * Each defect the validator knows is generated on purpose (the
 * generator's invalid kinds, opt-in) and must be reported with its own
 * category, a location where it has one, and nothing repaired: the
 * geometry serialises the same after validation as before.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "geometry/validation"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"

#include <math.h>

#include "tapi_geo.h"
#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *defect;
    const char *kind;
    tapi_geo_gen_item *items = NULL;
    unsigned int n = 0;
    unsigned int i;
    te_string p = TE_STRING_INIT;
    te_mi_logger *mi = NULL;

    TEST_START;
    TEST_GET_STRING_PARAM(defect);
    /* Generator kind -> the category the validator must report. */
    kind = strcmp(defect, "self_intersection") == 0 ? "bowtie" :
           strcmp(defect, "unclosed_ring") == 0 ? "unclosed" :
           strcmp(defect, "duplicate_vertex") == 0 ? "duplicate_vertex" :
           defect;

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_geom_valid");
    mi = tsapi_mi_open();

    TEST_STEP("Generate 8 geometries with the defect %s", defect);
    te_string_append(&p, "count=8\nseed=21\nallow_invalid=1\nkinds=%s\n"
                     "bounds=0 0 1000 1000", kind);
    CHECK_RC(tapi_geo_generate(sess.pco, "invalid", p.ptr, &items, &n, NULL));
    if (n != 8)
        TEST_VERDICT("The generator made %u geometries, not 8", n);

    for (i = 0; i < n; i++)
    {
        tapi_geo_geometry *g = NULL;
        tapi_geo_validity v;
        te_string cat = TE_STRING_INIT;
        te_string before = TE_STRING_INIT;
        te_string after = TE_STRING_INIT;
        char id[32];
        unsigned int k;
        bool found = false;

        if (strcmp(items[i].expect, defect) != 0 &&
            !(strcmp(defect, "unclosed_ring") == 0 &&
              strcmp(items[i].expect, "unclosed_ring") == 0))
            TEST_VERDICT("The generator promises '%s' for %s",
                         items[i].expect, defect);
        rc = tapi_geo_parse(sess.pco, items[i].fmt, items[i].text, NULL,
                            NULL, &g, &cat);
        if (strcmp(defect, "unclosed_ring") == 0)
        {
            /* An unclosed ring is not a geometry at all: the parser
             * refuses it and names the defect. */
            if (rc == 0)
                TEST_VERDICT("A polygon with an unclosed ring parsed");
            if (cat.ptr == NULL || strcmp(cat.ptr, "unclosed_ring") != 0)
                TEST_VERDICT("An unclosed ring was refused as '%s'",
                             cat.ptr != NULL ? cat.ptr : "");
            tsapi_mi_put(mi, "issues", 1);
            te_string_free(&cat);
            continue;
        }
        CHECK_RC(rc);
        CHECK_RC(tapi_geo_serialize(sess.pco, g, TAPI_GEO_FMT_WKB, -1,
                                    &before));
        snprintf(id, sizeof(id), "item-%lld", items[i].id);
        CHECK_RC(tapi_geo_validate(sess.pco, g, "all", "orientation=ccw",
                                   &v));
        for (k = 0; k < v.n_list; k++)
            found = found || strcmp(v.list[k].category, defect) == 0;
        if (v.ok || !found)
        {
            ERROR("%s: ok=%d, %u issue(s), first '%s' (%s)", items[i].text,
                  v.ok, v.issues, v.category, v.reason);
            TEST_VERDICT("The defect %s was not reported", defect);
        }
        /* Defects with a place must say where. */
        if ((strcmp(defect, "self_intersection") == 0 ||
             strcmp(defect, "ring_self_intersection") == 0 ||
             strcmp(defect, "duplicate_vertex") == 0) &&
            !isfinite(v.list[0].x))
            TEST_VERDICT("The %s has no location", defect);
        CHECK_RC(tapi_geo_serialize(sess.pco, g, TAPI_GEO_FMT_WKB, -1,
                                    &after));
        if (strcmp(before.ptr, after.ptr) != 0)
            TEST_VERDICT("Validation changed the geometry");
        tsapi_mi_put(mi, "issues", v.issues);
        tapi_geo_validity_free(&v);
        tapi_geo_geometry_free(sess.pco, g);
        te_string_free(&cat);
        te_string_free(&before);
        te_string_free(&after);
    }
    tsapi_mi_graph(mi, "issues", "Issues reported per defective geometry",
                   NULL, "issues");

    TEST_SUCCESS;

cleanup:
    tapi_geo_gen_items_free(items, n);
    te_string_free(&p);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
