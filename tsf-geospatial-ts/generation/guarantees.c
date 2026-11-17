/** @file
 * @brief generation group
 *
 * Every generated item is what the generator says it is: an item
 * promised valid passes full validation (and lies inside the bounds
 * asked for, unless it is a planted outlier); an item promised to carry
 * a defect is reported with exactly that defect.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "generation/guarantees"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo.h"
#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *kind;
    te_string p = TE_STRING_INIT;
    tapi_geo_gen_item *items = NULL;
    unsigned int n = 0, i, valid = 0, defective = 0, outliers = 0;
    tapi_geo_bbox box = { 100, 200, 300, 400 };
    te_mi_logger *mi = NULL;

    TEST_START;
    TEST_GET_STRING_PARAM(kind);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_gen_guar");
    mi = tsapi_mi_open();

    te_string_append(&p, "count=150\nseed=77\nbounds=100 200 300 400\n%s",
                     strcmp(kind, "invalid") == 0 ? "allow_invalid=1" :
                     strcmp(kind, "polygons") == 0 ? "holes=1\nvertices=11" :
                     strcmp(kind, "lines") == 0 ? "vertices=9" :
                     strcmp(kind, "grid") == 0 ? "nx=5\nny=4\ncells=1" :
                     "");
    if (strcmp(kind, "trajectory") == 0)
    {
        te_string_reset(&p);
        te_string_append(&p, "count=150\nseed=77\nbounds=-9 38 -7 41");
    }
    CHECK_RC(tapi_geo_generate(sess.pco, kind, p.ptr, &items, &n, NULL));

    for (i = 0; i < n; i++)
    {
        tapi_geo_geometry *g = NULL;
        tapi_geo_validity v;
        te_string cat = TE_STRING_INIT;
        bool promised_valid = strcmp(items[i].expect, "valid") == 0 ||
                              strcmp(items[i].expect, "true") == 0 ||
                              strcmp(items[i].expect, "corner") == 0 ||
                              strcmp(items[i].expect, "edge") == 0 ||
                              strcmp(items[i].expect, "inside") == 0;

        rc = tapi_geo_parse(sess.pco, items[i].fmt, items[i].text, NULL,
                            NULL, &g, &cat);
        if (rc != 0)
        {
            if (strcmp(items[i].expect, "unclosed_ring") == 0 &&
                cat.ptr != NULL && strcmp(cat.ptr, "unclosed_ring") == 0)
            {
                defective++;
                te_string_free(&cat);
                continue;
            }
            TEST_VERDICT("A %s item promised '%s' does not parse", kind,
                         items[i].expect);
        }
        te_string_free(&cat);
        CHECK_RC(tapi_geo_validate(sess.pco, g, "all", NULL, &v));
        if (promised_valid)
        {
            unsigned int out = 0;

            if (!v.ok)
                TEST_VERDICT("A %s item promised valid has the defect %s",
                             kind, v.category);
            if (strcmp(kind, "trajectory") != 0)
            {
                CHECK_RC(tapi_geo_check_bounds(sess.pco, g, &box, &out));
                if (out != 0)
                    TEST_VERDICT("A %s item lies outside the bounds", kind);
            }
            valid++;
        }
        else if (strcmp(items[i].expect, "outlier") == 0 ||
                 strcmp(items[i].expect, "outside") == 0)
        {
            unsigned int out = 0;

            CHECK_RC(tapi_geo_check_bounds(sess.pco, g, &box, &out));
            if (out == 0)
                TEST_VERDICT("A planted %s lies inside the bounds",
                             items[i].expect);
            outliers++;
        }
        else
        {
            unsigned int k;
            bool found = false;

            for (k = 0; k < v.n_list; k++)
                found = found || strcmp(v.list[k].category,
                                        items[i].expect) == 0;
            if (!found)
                TEST_VERDICT("An item promised '%s' is not reported so",
                             items[i].expect);
            defective++;
        }
        tapi_geo_validity_free(&v);
        tapi_geo_geometry_free(sess.pco, g);
    }
    RING("%s: %u items, %u valid, %u defective as promised, %u outliers",
         kind, n, valid, defective, outliers);
    tsapi_mi_put(mi, "valid", valid);
    tsapi_mi_put(mi, "defective", defective);
    tsapi_mi_put(mi, "outliers", outliers);
    tsapi_mi_graph(mi, "kinds", "Items by promise, all kept", NULL,
                   "valid,defective,outliers");

    TEST_SUCCESS;

cleanup:
    tapi_geo_gen_items_free(items, n);
    te_string_free(&p);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
