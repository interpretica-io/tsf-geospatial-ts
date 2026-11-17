/** @file
 * @brief geometry group
 *
 * Every geometry type, in every dimensionality, survives a trip through
 * every format: WKT -> format -> geometry -> WKT gives the same
 * coordinates, Z and M included where the format carries them. GeoJSON
 * has no M: there the M ordinates must be dropped and Z kept, never the
 * other way round.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "geometry/roundtrip"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"

#include "tapi_geo.h"
#include "tsapi_geo.h"

/* Append one coordinate in the requested dimensionality. */
static void
coord(te_string *s, double x, double y, bool z, bool m, int k)
{
    te_string_append(s, "%.17g %.17g", x, y);
    if (z)
        te_string_append(s, " %.17g", x * 3 + y);
    if (m)
        te_string_append(s, " %d", 100 + k);
}

static void
ring(te_string *s, double x0, double y0, double d, bool z, bool m, int *k)
{
    te_string_append(s, "(");
    coord(s, x0, y0, z, m, (*k)++);
    te_string_append(s, ", ");
    coord(s, x0 + d, y0, z, m, (*k)++);
    te_string_append(s, ", ");
    coord(s, x0 + d, y0 + d, z, m, (*k)++);
    te_string_append(s, ", ");
    coord(s, x0, y0 + d, z, m, (*k)++);
    te_string_append(s, ", ");
    coord(s, x0, y0, z, m, (*k) - 4);
    te_string_append(s, ")");
}

/* The k-th fixture: Point, LineString, Polygon with a hole,
 * MultiPoint, MultiPolygon, GeometryCollection. */
static void
fixture(int i, bool z, bool m, te_string *s)
{
    const char *tag = z ? (m ? " ZM" : " Z") : (m ? " M" : "");
    int k = 0;

    switch (i)
    {
        case 0:
            te_string_append(s, "POINT%s (", tag);
            coord(s, -8.6113, 41.1496, z, m, k);
            te_string_append(s, ")");
            break;
        case 1:
            te_string_append(s, "LINESTRING%s (", tag);
            coord(s, 0.1, 0.2, z, m, k++);
            te_string_append(s, ", ");
            coord(s, 1.0000000000000002, 2.5, z, m, k++);
            te_string_append(s, ", ");
            coord(s, -3.75e-7, 1e10, z, m, k++);
            te_string_append(s, ")");
            break;
        case 2:
            te_string_append(s, "POLYGON%s (", tag);
            ring(s, 0, 0, 10, z, m, &k);
            te_string_append(s, ", ");
            ring(s, 2, 2, 3, z, m, &k);
            te_string_append(s, ")");
            break;
        case 3:
            te_string_append(s, "MULTIPOINT%s ((", tag);
            coord(s, 1, 2, z, m, k++);
            te_string_append(s, "), (");
            coord(s, 3, 4, z, m, k++);
            te_string_append(s, "))");
            break;
        case 4:
            te_string_append(s, "MULTIPOLYGON%s ((", tag);
            ring(s, 0, 0, 1, z, m, &k);
            te_string_append(s, "), (");
            ring(s, 5, 5, 2, z, m, &k);
            te_string_append(s, "))");
            break;
        default:
            te_string_append(s, "GEOMETRYCOLLECTION%s (POINT%s (", tag, tag);
            coord(s, 7, 7, z, m, k++);
            te_string_append(s, "), LINESTRING%s (", tag);
            coord(s, 0, 0, z, m, k++);
            te_string_append(s, ", ");
            coord(s, 1, 1, z, m, k++);
            te_string_append(s, "))");
            break;
    }
}

int
main(int argc, char **argv)
{
    static const char *const types[] = {
        "Point", "LineString", "Polygon", "MultiPoint", "MultiPolygon",
        "GeometryCollection"
    };
    tsapi_geo_session sess = { 0 };
    const char *format;
    const char *dims;
    tapi_geo_format fmt;
    te_mi_logger *mi = NULL;
    bool z, m;
    const char *want_dims;
    int i;

    TEST_START;
    TEST_GET_STRING_PARAM(format);
    TEST_GET_STRING_PARAM(dims);

    fmt = strcmp(format, "wkb") == 0 ? TAPI_GEO_FMT_WKB :
          strcmp(format, "ewkb") == 0 ? TAPI_GEO_FMT_EWKB :
          strcmp(format, "geojson") == 0 ? TAPI_GEO_FMT_GEOJSON :
                                           TAPI_GEO_FMT_WKT;
    z = strchr(dims + 2, 'z') != NULL;
    m = strchr(dims + 2, 'm') != NULL;
    /* GeoJSON carries Z but no M. */
    want_dims = fmt == TAPI_GEO_FMT_GEOJSON ? (z ? "xyz" : "xy") : dims;

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_geom_roundtrip");
    if (m && (sess.engine.features & TAPI_GEO_FEAT_M) == 0)
        TEST_SKIP("The agent's GEOS has no M coordinates (needs 3.12)");
    mi = tsapi_mi_open();

    for (i = 0; i < (int)TE_ARRAY_LEN(types); i++)
    {
        te_string wkt = TE_STRING_INIT;
        te_string ser = TE_STRING_INIT;
        te_string back = TE_STRING_INIT;
        te_string orig = TE_STRING_INIT;
        tapi_geo_geometry *g = NULL;
        tapi_geo_geometry *g2 = NULL;
        tapi_geo_info info;

        fixture(i, z, m, &wkt);
        TEST_SUBSTEP("%s %s through %s", types[i], dims, format);
        CHECK_RC(tapi_geo_parse(sess.pco, "wkt", wkt.ptr, "EPSG:4326", NULL,
                                &g, NULL));
        CHECK_RC(tapi_geo_info_get(sess.pco, g, &info));
        if (strcmp(info.type, types[i]) != 0 || strcmp(info.dims, dims) != 0)
            TEST_VERDICT("WKT parsed as %s %s, not %s %s", info.type,
                         info.dims, types[i], dims);
        CHECK_RC(tapi_geo_serialize(sess.pco, g, fmt, -1, &ser));
        CHECK_RC(tapi_geo_parse(sess.pco, format, ser.ptr, NULL, NULL, &g2,
                                NULL));
        CHECK_RC(tapi_geo_info_get(sess.pco, g2, &info));
        if (strcmp(info.type, types[i]) != 0)
            TEST_VERDICT("%s came back from %s as %s", types[i], format,
                         info.type);
        if (strcmp(info.dims, want_dims) != 0)
            TEST_VERDICT("%s %s came back from %s as %s, expected %s",
                         types[i], dims, format, info.dims, want_dims);
        if (fmt == TAPI_GEO_FMT_EWKB && strcmp(info.crs, "EPSG:4326") != 0)
            TEST_VERDICT("EWKB lost the SRID: '%s'", info.crs);

        /* Same coordinates: compare full-precision WKT of both, after
         * dropping M from the original where the format cannot carry
         * it (serialising with 3 output dimensions). */
        CHECK_RC(tapi_geo_serialize(sess.pco, g2, TAPI_GEO_FMT_WKT, -1,
                                    &back));
        if (fmt == TAPI_GEO_FMT_GEOJSON && m)
        {
            tapi_geo_res r = TAPI_GEO_RES_INIT;

            CHECK_RC(tapi_geo_call(sess.pco, TAPI_GEO_RPC_GEOM_SERIALIZE,
                                   g->handle, 0, "wkt", "",
                                   z ? "dims=3" : "dims=2", &r));
            te_string_append(&orig, "%s", r.report.ptr);
            tapi_geo_rpc_res_free(&r);
        }
        else
            CHECK_RC(tapi_geo_serialize(sess.pco, g, TAPI_GEO_FMT_WKT, -1,
                                        &orig));
        if (strcmp(orig.ptr, back.ptr) != 0)
        {
            ERROR("Before: %s\nAfter:  %s", orig.ptr, back.ptr);
            TEST_VERDICT("%s %s does not survive %s", types[i], dims,
                         format);
        }
        tsapi_mi_put_type(mi, TE_MI_MEAS_MEMORY, "encoded_bytes",
                          (double)ser.len, TE_MI_MEAS_MULTIPLIER_PLAIN);
        tsapi_mi_put(mi, "vertices", (double)info.npoints);
        CHECK_RC(tapi_geo_geometry_free(sess.pco, g));
        CHECK_RC(tapi_geo_geometry_free(sess.pco, g2));
        te_string_free(&wkt);
        te_string_free(&ser);
        te_string_free(&back);
        te_string_free(&orig);
    }
    tsapi_mi_graph_type(mi, "size", "Encoded size per geometry type "
                        "(Point, Line, Polygon, MultiPoint, MultiPolygon, "
                        "Collection)", NULL, TE_MI_MEAS_MEMORY,
                        "encoded_bytes");
    tsapi_mi_graph(mi, "vertices", "Vertices per geometry type", NULL,
                   "vertices");

    TEST_SUCCESS;

cleanup:
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
