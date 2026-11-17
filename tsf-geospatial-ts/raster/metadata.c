/** @file
 * @brief raster group
 *
 * GeoTIFF metadata as written: size, bands, pixel type, NoData, CRS,
 * geotransform, resolution, extent and block layout, for every pixel
 * type and both layouts (tiled, striped).
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "raster/metadata"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo_raster.h"
#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *type;
    const char *layout;
    bool tiled;
    te_string path = TE_STRING_INIT;
    te_string p = TE_STRING_INIT;
    tapi_geo_raster *r = NULL;
    tapi_geo_raster_info i;
    te_mi_logger *mi = NULL;

    TEST_START;
    TEST_GET_STRING_PARAM(type);
    TEST_GET_STRING_PARAM(layout);
    tiled = strcmp(layout, "tiled") == 0;

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_ras_meta");
    mi = tsapi_mi_open();

    TEST_STEP("Write a 3-band %s GeoTIFF, %s", type, layout);
    te_string_append(&path, "%s/m.tif", sess.work);
    te_string_append(&p, "width=300\nheight=200\nbands=3\ntype=%s\n"
                     "crs=EPSG:32629\ngeotransform=530000 25 0 4560000 0 -25"
                     "\nnodata=7\ntiled=%d\nblock=64\npattern=checker\nhi=100",
                     type, tiled);
    CHECK_RC(tapi_geo_raster_write(sess.pco, path.ptr, p.ptr));

    TEST_STEP("Read the metadata back");
    CHECK_RC(tapi_geo_raster_open(sess.pco, path.ptr, NULL, &r));
    CHECK_RC(tapi_geo_raster_info_get(sess.pco, r, &i));
    RING("%s %dx%d x%d %s nodata %g, %s, block %dx%d, extent %.0f %.0f "
         "%.0f %.0f", i.driver, i.width, i.height, i.bands, i.type,
         i.nodata, i.crs, i.block_x, i.block_y, i.extent.minx, i.extent.miny,
         i.extent.maxx, i.extent.maxy);
    if (strcmp(i.driver, "GTiff") != 0 || i.width != 300 || i.height != 200 ||
        i.bands != 3)
        TEST_VERDICT("Size or bands wrong: %dx%d x%d", i.width, i.height,
                     i.bands);
    if (strcmp(i.type, type) != 0)
        TEST_VERDICT("Pixel type %s, written %s", i.type, type);
    if (!i.has_nodata || i.nodata != 7)
        TEST_VERDICT("NoData lost");
    if (strcmp(i.crs, "EPSG:32629") != 0 || !i.georeferenced)
        TEST_VERDICT("CRS '%s'", i.crs);
    if (i.res_x != 25 || i.res_y != 25 || i.rotated)
        TEST_VERDICT("Resolution %g x %g", i.res_x, i.res_y);
    if (i.extent.minx != 530000 || i.extent.maxx != 537500 ||
        i.extent.miny != 4555000 || i.extent.maxy != 4560000)
        TEST_VERDICT("Extent is not the geotransform's");
    if (tiled ? (i.block_x != 64 || i.block_y != 64) : i.block_x != 300)
        TEST_VERDICT("Block layout %dx%d for %s", i.block_x, i.block_y,
                     layout);
    tsapi_mi_put(mi, "block_x", i.block_x);
    tsapi_mi_put(mi, "block_y", i.block_y);
    tsapi_mi_graph(mi, "block", "Block size", NULL, "block_x,block_y");

    TEST_SUCCESS;

cleanup:
    tapi_geo_raster_close(sess.pco, r);
    te_string_free(&path);
    te_string_free(&p);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
