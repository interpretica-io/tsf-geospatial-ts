/** @file
 * @brief raster group
 *
 * A window read returns exactly the pixels of the window: of a
 * gradient a + bx*x + by*y every value is known, at every offset, for
 * windows that cross tile boundaries; a window outside the raster is
 * refused.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "raster/window"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo_raster.h"
#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    static const struct { int x, y, w, h; } win[] = {
        { 0, 0, 1, 1 }, { 60, 60, 10, 10 }, { 120, 0, 17, 33 },
        { 255, 255, 1, 1 }, { 1, 2, 64, 3 },
    };
    tsapi_geo_session sess = { 0 };
    te_string path = TE_STRING_INIT;
    tapi_geo_raster *r = NULL;
    double *v = NULL;
    te_mi_logger *mi = NULL;
    unsigned int i;

    TEST_START;

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_ras_window");
    mi = tsapi_mi_open();
    te_string_append(&path, "%s/w.tif", sess.work);
    CHECK_RC(tapi_geo_raster_write(sess.pco, path.ptr,
                                   "width=256\nheight=256\ntype=Float64\n"
                                   "pattern=gradient\na=7\nbx=3\nby=1000\n"
                                   "block=64"));
    CHECK_RC(tapi_geo_raster_open(sess.pco, path.ptr, NULL, &r));

    for (i = 0; i < TE_ARRAY_LEN(win); i++)
    {
        int x, y;
        unsigned int bad = 0;

        TEST_SUBSTEP("Window %d,%d %dx%d", win[i].x, win[i].y, win[i].w,
                     win[i].h);
        free(v);
        v = calloc((size_t)win[i].w * win[i].h, sizeof(*v));
        CHECK_RC(tapi_geo_raster_read(sess.pco, r, 1, win[i].x, win[i].y,
                                      win[i].w, win[i].h, v, NULL));
        for (y = 0; y < win[i].h; y++)
            for (x = 0; x < win[i].w; x++)
                bad += v[y * win[i].w + x] !=
                       7 + 3.0 * (win[i].x + x) + 1000.0 * (win[i].y + y);
        if (bad > 0)
            TEST_VERDICT("%u wrong pixels in window %u", bad, i);
        tsapi_mi_put(mi, "pixels", (double)win[i].w * win[i].h);
    }
    tsapi_mi_graph(mi, "px", "Pixels read per window", NULL, "pixels");

    TEST_STEP("A window past the edge is refused");
    if (tapi_geo_raster_read(sess.pco, r, 1, 250, 250, 10, 10, NULL,
                             NULL) == 0)
        TEST_VERDICT("A window outside the raster was read");

    TEST_SUCCESS;

cleanup:
    free(v);
    tapi_geo_raster_close(sess.pco, r);
    te_string_free(&path);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
