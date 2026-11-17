/** @file
 * @brief raster group
 *
 * Raster formats besides GeoTIFF: a Cloud Optimized GeoTIFF, a PNG
 * written without a CRS (and not reported georeferenced), and Erdas
 * Imagine (HFA). Each is written through GDAL and read back with the
 * metadata it can carry.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "raster/formats"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo_raster.h"
#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *driver;
    const char *ext;
    te_string path = TE_STRING_INIT;
    te_string p = TE_STRING_INIT;
    tapi_geo_raster *r = NULL;
    tapi_geo_raster_info i;
    bool georef;
    te_mi_logger *mi = NULL;

    TEST_START;
    TEST_GET_STRING_PARAM(driver);
    ext = strcmp(driver, "COG") == 0 ? "tif" :
          strcmp(driver, "PNG") == 0 ? "png" :
          strcmp(driver, "HFA") == 0 ? "img" : "tif";
    georef = strcmp(driver, "PNG") != 0;

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_ras_formats");
    if (!tapi_geo_driver_supported(sess.pco, driver, true))
        TEST_SKIP("The agent's GDAL cannot write %s", driver);
    mi = tsapi_mi_open();

    TEST_STEP("Write a 3-band Byte image through %s", driver);
    te_string_append(&path, "%s/img.%s", sess.work, ext);
    /* Web Mercator around Porto, 256 x 256 at ~38 m. */
    te_string_append(&p, "driver=%s\nwidth=256\nheight=256\nbands=3\n"
                     "type=Byte\npattern=checker\nhi=255\ncrs=%s\n"
                     "geotransform=-958826.08 38.2185 0 5038873.2 0 -38.2185"
                     "\ntiled=0", driver, georef ? "EPSG:3857" : "");
    CHECK_RC(tapi_geo_raster_write(sess.pco, path.ptr, p.ptr));

    TEST_STEP("Read it back");
    CHECK_RC(tapi_geo_raster_open(sess.pco, path.ptr, NULL, &r));
    CHECK_RC(tapi_geo_raster_info_get(sess.pco, r, &i));
    RING("%s: %dx%d x%d %s, %s, georeferenced %d, layout '%s', overviews %d",
         i.driver, i.width, i.height, i.bands, i.type, i.crs,
         i.georeferenced, i.compression, i.overviews);
    tsapi_mi_put(mi, "width", i.width);
    tsapi_mi_put(mi, "bands", i.bands);
    tsapi_mi_put(mi, "overviews", i.overviews);
    tsapi_mi_graph(mi, "img", "Width, bands, overviews read back", NULL,
                   "width,bands,overviews");
    if (i.bands < 3 || strcmp(i.type, "Byte") != 0)
        TEST_VERDICT("%s read back as %d %s band(s)", driver, i.bands,
                     i.type);
    if (i.width != 256 || i.height != 256)
        TEST_VERDICT("%s read back %dx%d", driver, i.width, i.height);
    if (georef && strcmp(i.crs, "EPSG:3857") != 0)
        TEST_VERDICT("%s lost the CRS: '%s'", driver, i.crs);
    if (!georef && i.georeferenced)
        TEST_VERDICT("A PNG claims georeferencing it cannot carry");

    TEST_SUCCESS;

cleanup:
    tapi_geo_raster_close(sess.pco, r);
    te_string_free(&path);
    te_string_free(&p);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
