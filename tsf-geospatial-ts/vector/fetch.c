/** @file
 * @brief vector group
 *
 * Fetching is bounded and exact: pages by offset and limit cover the
 * layer once, an attribute filter and a bounding-box filter return what
 * the data says they should (counted independently by the test), and a
 * single feature's geometry comes back as an engine geometry with the
 * layer's CRS.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "vector/fetch"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo_dataset.h"
#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    te_string path = TE_STRING_INIT;
    te_string page = TE_STRING_INIT;
    tapi_geo_dataset *ds = NULL;
    tapi_geo_geometry *g = NULL;
    tapi_geo_info info;
    unsigned int got, total = 0;
    long long off;
    te_mi_logger *mi = NULL;

    TEST_START;

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_vec_fetch");
    mi = tsapi_mi_open();

    TEST_STEP("A 10 x 10 grid of cells, ids 1..100, value = id mod 1000");
    te_string_append(&path, "%s/grid.gpkg", sess.work);
    CHECK_RC(tapi_geo_dataset_generate(sess.pco, path.ptr, "GPKG",
                                       "EPSG:4326", "grid",
                                       "nx=10\nny=10\ncells=1\nbounds=0 0 10 "
                                       "10", NULL, NULL, NULL, NULL));
    CHECK_RC(tapi_geo_dataset_open(sess.pco, path.ptr, NULL, &ds));

    TEST_STEP("Pages of 7 cover the 100 features exactly once");
    for (off = 0; off < 120; off += 7)
    {
        CHECK_RC(tapi_geo_dataset_fetch(sess.pco, ds, NULL, off, 7,
                                        "geometry=none", &page, &got));
        total += got;
        tsapi_mi_put(mi, "page_size", got);
    }
    if (total != 100)
        TEST_VERDICT("Pages returned %u features, not 100", total);
    tsapi_mi_graph(mi, "pages", "Features per page of 7", NULL, "page_size");

    TEST_STEP("An attribute filter: value < 25 is 24 cells");
    te_string_reset(&page);
    CHECK_RC(tapi_geo_dataset_fetch(sess.pco, ds, NULL, 0, 1000,
                                    "where=value < 25\ngeometry=none", &page,
                                    &got));
    if (got != 24)
        TEST_VERDICT("value < 25 matched %u features, expected 24", got);

    TEST_STEP("A box filter: [2.5 2.5, 4.5 4.5] touches 3 x 3 cells");
    te_string_reset(&page);
    CHECK_RC(tapi_geo_dataset_fetch(sess.pco, ds, NULL, 0, 1000,
                                    "bbox=2.5 2.5 4.5 4.5\ngeometry=wkt",
                                    &page, &got));
    if (got != 9)
        TEST_VERDICT("The box matched %u features, expected 9", got);
    if (strstr(page.ptr, "POLYGON") == NULL)
        TEST_VERDICT("Fetched features carry no WKT geometry");

    TEST_STEP("One feature's geometry, as an engine geometry");
    CHECK_RC(tapi_geo_dataset_geometry(sess.pco, ds, NULL, 1, &g));
    CHECK_RC(tapi_geo_info_get(sess.pco, g, &info));
    if (strcmp(info.type, "Polygon") != 0 ||
        strcmp(info.crs, "EPSG:4326") != 0)
        TEST_VERDICT("Feature 1 came back as %s in '%s'", info.type,
                     info.crs);

    TEST_SUCCESS;

cleanup:
    tapi_geo_geometry_free(sess.pco, g);
    tapi_geo_dataset_close(sess.pco, ds);
    te_string_free(&path);
    te_string_free(&page);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
