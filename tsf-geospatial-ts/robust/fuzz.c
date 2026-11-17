/** @file
 * @brief robust group
 *
 * Mutation robustness of the parsers: 400 deterministic mutations of
 * valid inputs (bytes flipped, dropped, duplicated, truncated, numbers
 * replaced with extremes) are fed to the WKT, WKB, GeoJSON or CRS
 * parser. Every input is either accepted or refused with a defined
 * error; the RPC server never dies, and leaks no object. (For
 * coverage-guided fuzzing of the same entry points, tsf-fuzz drives
 * ci/geo_cli of tsf-geospatial with sanitizers.)
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "robust/fuzz"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo.h"
#include "tapi_geo_crs.h"
#include "tsapi_geo.h"

#define N 400

static const char *const seeds_wkt[] = {
    "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0), (2 2, 2 3, 3 3, 2 2))",
    "MULTILINESTRING ZM ((0 0 1 2, 1 1 2 3), (5 5 5 5, 6 6 6 6))",
    "GEOMETRYCOLLECTION (POINT (1 2), LINESTRING (0 0, 1 1))",
};
static const char *const seeds_wkb[] = {
    "0103000000010000000500000000000000000000000000000000000000000000000000"
    "2440000000000000000000000000000024400000000000002440000000000000000000"
    "000000000024400000000000000000000000000000000000",
    "0102000020E61000000200000000000000000000000000000000000000000000000000"
    "F03F000000000000F03F",
};
static const char *const seeds_json[] = {
    "{\"type\":\"Polygon\",\"coordinates\":[[[0,0],[1,0],[1,1],[0,1],[0,0]]]}",
    "{\"type\":\"FeatureCollection\",\"features\":[{\"type\":\"Feature\","
    "\"properties\":{\"a\":1},\"geometry\":{\"type\":\"Point\","
    "\"coordinates\":[1,2,3]}}]}",
};
static const char *const seeds_crs[] = {
    "EPSG:3763",
    "+proj=tmerc +lat_0=39.66 +lon_0=-8.13 +k=1 +x_0=0 +y_0=0 +ellps=GRS80 "
    "+units=m +type=crs",
    "urn:ogc:def:crs:EPSG::4326",
};

static uint64_t rs = 0x9e3779b97f4a7c15ULL;

static uint64_t
rnd(void)
{
    rs ^= rs << 13;
    rs ^= rs >> 7;
    rs ^= rs << 17;
    return rs;
}

/* One mutation of @p in into @p out. */
static void
mutate(const char *in, te_string *out, bool hex)
{
    size_t n = strlen(in);
    size_t pos = n > 0 ? rnd() % n : 0;
    static const char *const extremes[] = {
        "1e308", "-1e308", "nan", "inf", "-0", "1e-320", "99999999999999999",
        "0x7fffffff", "", ",", "(((", "]]]", "\"", "\\u0000"
    };

    switch (rnd() % 6)
    {
        case 0:     /* flip a byte (a hex digit for WKB) */
            te_string_append(out, "%s", in);
            out->ptr[pos] = hex ? "0123456789ABCDEF"[rnd() % 16] :
                                  (char)(32 + rnd() % 95);
            break;
        case 1:     /* drop a span */
        {
            size_t len = 1 + rnd() % 8;

            te_string_append(out, "%.*s%s", (int)pos, in,
                             pos + len < n ? in + pos + len : "");
            break;
        }
        case 2:     /* duplicate a span */
            te_string_append(out, "%.*s%s", (int)(pos + (n - pos) / 2), in,
                             in + pos);
            break;
        case 3:     /* truncate */
            te_string_append(out, "%.*s", (int)pos, in);
            break;
        case 4:     /* splice an extreme in */
            te_string_append(out, "%.*s%s%s", (int)pos, in,
                             hex ? "FFFFFFFF" :
                             extremes[rnd() % TE_ARRAY_LEN(extremes)],
                             in + pos);
            break;
        default:    /* repeat the whole input */
            te_string_append(out, "%s%s", in, in);
            break;
    }
}

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *target;
    const char *const *seeds;
    unsigned int nseeds, i;
    unsigned int accepted = 0, refused = 0, other = 0;
    tapi_geo_stats s0, s1;
    te_mi_logger *mi = NULL;
    bool hex;

    TEST_START;
    TEST_GET_STRING_PARAM(target);
    hex = strcmp(target, "wkb") == 0;
    seeds = strcmp(target, "wkt") == 0 ? seeds_wkt :
            hex ? seeds_wkb :
            strcmp(target, "geojson") == 0 ? seeds_json : seeds_crs;
    nseeds = strcmp(target, "wkt") == 0 ? TE_ARRAY_LEN(seeds_wkt) :
             hex ? TE_ARRAY_LEN(seeds_wkb) :
             strcmp(target, "geojson") == 0 ? TE_ARRAY_LEN(seeds_json) :
                                              TE_ARRAY_LEN(seeds_crs);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_rob_fuzz");
    mi = tsapi_mi_open();
    CHECK_RC(tapi_geo_stats_get(sess.pco, &s0));

    TEST_STEP("%d mutations of %u %s seeds", N, nseeds, target);
    for (i = 0; i < N; i++)
    {
        te_string in = TE_STRING_INIT;
        tapi_geo_geometry *g = NULL;

        mutate(seeds[i % nseeds], &in, hex);
        if (strcmp(target, "crs") == 0)
        {
            bool valid = false;

            rc = tapi_geo_crs_validate(sess.pco, in.ptr != NULL ? in.ptr :
                                       "", &valid, NULL);
            if (rc == 0)
                valid ? accepted++ : refused++;
        }
        else
        {
            rc = tapi_geo_parse(sess.pco, target, in.ptr != NULL ? in.ptr :
                                "", NULL, NULL, &g, NULL);
            if (rc == 0)
                accepted++;
            else if (TE_RC_GET_ERROR(rc) == TE_ECORRUPTED ||
                     TE_RC_GET_ERROR(rc) == TE_E2BIG ||
                     TE_RC_GET_ERROR(rc) == TE_EOPNOTSUPP)
            {
                refused++;
                rc = 0;
            }
            tapi_geo_geometry_free(sess.pco, g);
        }
        if (rc != 0)
        {
            other++;
            ERROR("Mutation %u '%s' gave %r: %s", i,
                  in.ptr != NULL ? in.ptr : "", rc, tapi_geo_last_error());
            if (TE_RC_GET_ERROR(rc) == TE_ECORRUPTED ||
                rcf_rpc_server_is_alive(sess.pco) == false)
                TEST_VERDICT("The RPC server died on a mutated %s input",
                             target);
        }
        if (i % 20 == 19)
        {
            tsapi_mi_put(mi, "accepted", accepted);
            tsapi_mi_put(mi, "refused", refused);
        }
        te_string_free(&in);
    }
    RING("%s: %u accepted, %u refused, %u other", target, accepted, refused,
         other);
    tsapi_mi_graph(mi, "outcome", "Accepted / refused mutations, every 20",
                   NULL, "accepted,refused");
    if (other > 0)
        TEST_VERDICT("%u mutated %s inputs gave an undefined error", other,
                     target);

    TEST_STEP("Nothing leaked, the same RPC server process");
    CHECK_RC(tapi_geo_stats_get(sess.pco, &s1));
    if (s1.session != s0.session)
        TEST_VERDICT("The RPC server was restarted during fuzzing");
    if (s1.live != s0.live)
        TEST_VERDICT("%d objects leaked", (int)s1.live - (int)s0.live);

    TEST_SUCCESS;

cleanup:
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
