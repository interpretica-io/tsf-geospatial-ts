/** @file
 * @brief raster group
 *
 * Raster comparison finds the one difference made on purpose: pixel
 * noise (counted, with its size), NoData where the other has data, a
 * shifted geotransform, another CRS, another size, another pixel type -
 * and nothing between a raster and its identical twin.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "raster/compare"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo_raster.h"
#include "tsapi_geo.h"

#define BASE "width=256\nheight=256\ntype=Float32\npattern=gradient\na=1\n" \
             "bx=0.5\nby=0.25\ncrs=EPSG:3763\ngeotransform=-50000 10 0 "    \
             "150000 0 -10\nnodata=-9999\nblock=64\nseed=3\n"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *change;
    te_string pa = TE_STRING_INIT;
    te_string pb = TE_STRING_INIT;
    te_string p = TE_STRING_INIT;
    tapi_geo_raster *a = NULL, *b = NULL;
    tapi_geo_raster_diff d;
    te_mi_logger *mi = NULL;

    TEST_START;
    TEST_GET_STRING_PARAM(change);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_ras_cmp");
    mi = tsapi_mi_open();

    te_string_append(&pa, "%s/a.tif", sess.work);
    te_string_append(&pb, "%s/b.tif", sess.work);
    CHECK_RC(tapi_geo_raster_write(sess.pco, pa.ptr, BASE));
    te_string_append(&p, "%s", BASE);
    if (strcmp(change, "noise") == 0)
        te_string_append(&p, "noise_pixels=40\nnoise=0.75\n");
    else if (strcmp(change, "nodata") == 0)
        te_string_append(&p, "nodata_every=1000\n");
    else if (strcmp(change, "geotransform") == 0)
        te_string_append(&p, "geotransform=-49990 10 0 150000 0 -10\n");
    else if (strcmp(change, "crs") == 0)
        te_string_append(&p, "crs=EPSG:3857\n");
    else if (strcmp(change, "size") == 0)
        te_string_append(&p, "width=255\n");
    else if (strcmp(change, "type") == 0)
        te_string_append(&p, "type=Float64\n");
    TEST_STEP("A twin with the change '%s'", change);
    CHECK_RC(tapi_geo_raster_write(sess.pco, pb.ptr, p.ptr));
    CHECK_RC(tapi_geo_raster_open(sess.pco, pa.ptr, NULL, &a));
    CHECK_RC(tapi_geo_raster_open(sess.pco, pb.ptr, NULL, &b));

    TEST_STEP("Compare, tolerance 0.1");
    CHECK_RC(tapi_geo_raster_compare(sess.pco, a, b, 0.1, 0, &d));
    RING("equal %d, metadata %lld, pixels %lld of %lld, nodata %lld, max "
         "|d| %g, rmse %g\n%s", d.equal, d.metadata_diff, d.pixel_diff,
         d.compared, d.nodata_mismatch, d.max_abs_diff, d.rmse,
         d.records.ptr != NULL ? d.records.ptr : "");
    tsapi_mi_put(mi, "metadata_diff", (double)d.metadata_diff);
    tsapi_mi_put(mi, "pixel_diff", (double)d.pixel_diff);
    tsapi_mi_put(mi, "nodata_mismatch", (double)d.nodata_mismatch);
    tsapi_mi_put(mi, "max_abs_diff", d.max_abs_diff);
    tsapi_mi_graph(mi, "diff", "Differences found", NULL,
                   "metadata_diff,pixel_diff,nodata_mismatch,max_abs_diff");
    if (strcmp(change, "none") == 0)
    {
        if (!d.equal)
            TEST_VERDICT("Identical rasters compare different");
    }
    else if (d.equal)
        TEST_VERDICT("The change '%s' was not found", change);
    if (strcmp(change, "noise") == 0 &&
        (d.pixel_diff < 30 || d.pixel_diff > 40 ||
         !tsapi_geo_near(d.max_abs_diff, 0.75, 1e-6) || d.metadata_diff))
        TEST_VERDICT("Noise found as %lld pixels, max %g", d.pixel_diff,
                     d.max_abs_diff);
    if (strcmp(change, "nodata") == 0 &&
        (d.nodata_mismatch != (256 * 256 + 999) / 1000 || d.pixel_diff != 0))
        TEST_VERDICT("NoData mismatches %lld", d.nodata_mismatch);
    if ((strcmp(change, "geotransform") == 0 || strcmp(change, "crs") == 0 ||
         strcmp(change, "size") == 0 || strcmp(change, "type") == 0) &&
        d.metadata_diff != 1)
        TEST_VERDICT("The change '%s' gave %lld metadata differences",
                     change, d.metadata_diff);
    if (strcmp(change, "type") == 0 && d.pixel_diff != 0)
        TEST_VERDICT("Float32 vs Float64 of the same gradient differ in "
                     "value");
    tapi_geo_raster_diff_free(&d);

    TEST_SUCCESS;

cleanup:
    tapi_geo_raster_close(sess.pco, a);
    tapi_geo_raster_close(sess.pco, b);
    te_string_free(&pa);
    te_string_free(&pb);
    te_string_free(&p);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
