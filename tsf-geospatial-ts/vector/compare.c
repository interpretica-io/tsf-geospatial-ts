/** @file
 * @brief vector group
 *
 * Dataset comparison finds exactly what changed and nothing else. A
 * GeoJSON dataset of 300 polygons is copied and changed in one known
 * way by an independent program (Python's json module, on the agent):
 * features reordered, moved, re-valued, deleted, inserted, nudged
 * within tolerance, or relabelled into another CRS. Order never
 * matters; identity is the "id" field.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "vector/compare"

#include "te_config.h"
#include "tapi_test.h"

#include "tapi_geo_dataset.h"
#include "tsapi_geo.h"

/* Rewrite a FeatureCollection with one kind of change. */
static const char *const editor =
"import json, sys\n"
"src, dst, change = sys.argv[1:4]\n"
"fc = json.load(open(src))\n"
"f = fc['features']\n"
"def shift(c, d):\n"
"    if isinstance(c[0], list):\n"
"        return [shift(x, d) for x in c]\n"
"    return [c[0] + d, c[1]] + c[2:]\n"
"byid = {x['properties']['id']: x for x in f}\n"
"if change == 'reordered':\n"
"    f.reverse()\n"
"elif change == 'moved':\n"
"    for i in (11, 22, 33):\n"
"        g = byid[i]['geometry']\n"
"        g['coordinates'] = shift(g['coordinates'], 0.01)\n"
"elif change == 'attribute':\n"
"    for i in (5, 6, 7):\n"
"        byid[i]['properties']['value'] += 1\n"
"elif change == 'deleted':\n"
"    f[:] = [x for x in f if x['properties']['id'] not in (40, 41)]\n"
"elif change == 'inserted':\n"
"    for i in (9001, 9002):\n"
"        n = json.loads(json.dumps(f[0]))\n"
"        n['properties']['id'] = i\n"
"        f.append(n)\n"
"elif change == 'within_tolerance':\n"
"    for x in f:\n"
"        x['geometry']['coordinates'] = shift(x['geometry']['coordinates'],"
" 1e-9)\n"
"elif change == 'crs':\n"
"    fc['crs'] = {'type': 'name', 'properties':"
" {'name': 'urn:ogc:def:crs:EPSG::3857'}}\n"
"json.dump(fc, open(dst, 'w'))\n";

static const struct {
    const char *change;
    long long matched, only_a, only_b, geom, attr;
    bool crs_ok;
} cases[] = {
    { "none", 300, 0, 0, 0, 0, true },
    { "reordered", 300, 0, 0, 0, 0, true },
    { "moved", 300, 0, 0, 3, 0, true },
    { "attribute", 300, 0, 0, 0, 3, true },
    { "deleted", 298, 0, 0, 0, 0, true },
    { "inserted", 300, 0, 2, 0, 0, true },
    { "within_tolerance", 300, 0, 0, 0, 0, true },
    { "crs", 0, 0, 0, 0, 0, false },
};

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *change;
    te_string a = TE_STRING_INIT;
    te_string b = TE_STRING_INIT;
    te_string ed = TE_STRING_INIT;
    tapi_geo_dataset *da = NULL, *db = NULL;
    tapi_geo_ds_cmp_opts opts = TAPI_GEO_DS_CMP_OPTS_DEFAULT;
    tapi_geo_ds_diff d;
    te_mi_logger *mi = NULL;
    unsigned int i;
    int c = -1;

    TEST_START;
    TEST_GET_STRING_PARAM(change);
    for (i = 0; i < TE_ARRAY_LEN(cases); i++)
        if (strcmp(cases[i].change, change) == 0)
            c = (int)i;
    if (c < 0)
        TEST_FAIL("Unknown change %s", change);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_vec_cmp");
    mi = tsapi_mi_open();

    TEST_STEP("Write 300 polygons, then a copy with the change '%s'",
              change);
    te_string_append(&a, "%s/a.geojson", sess.work);
    te_string_append(&b, "%s/b.geojson", sess.work);
    te_string_append(&ed, "%s/edit.py", sess.work);
    CHECK_RC(tapi_geo_dataset_generate(sess.pco, a.ptr, "GeoJSON",
                                       "EPSG:4326", "polygons",
                                       "count=300\nseed=6\nbounds=-9 38 -7 "
                                       "41", NULL, NULL, NULL, NULL));
    CHECK_RC(tsapi_geo_put(&sess, "edit.py", editor));
    CHECK_RC(tsapi_geo_sh(&sess, "python3 %s %s %s %s", ed.ptr, a.ptr, b.ptr,
                          change));

    TEST_STEP("Compare them by id");
    CHECK_RC(tapi_geo_dataset_open(sess.pco, a.ptr, NULL, &da));
    CHECK_RC(tapi_geo_dataset_open(sess.pco, b.ptr, NULL, &db));
    opts.id_field = "id";
    if (strcmp(change, "within_tolerance") == 0)
    {
        opts.geometry = "hausdorff";
        opts.tolerance = 1e-6;
    }
    CHECK_RC(tapi_geo_dataset_compare(sess.pco, da, db, &opts, &d));
    RING("crs %d, matched %lld, only A %lld, only B %lld, geometry %lld, "
         "attribute %lld, Hausdorff %g\n%s", d.crs_compatible, d.matched,
         d.only_a, d.only_b, d.geometry_diff, d.attribute_diff,
         d.max_hausdorff, d.records.ptr != NULL ? d.records.ptr : "");
    tsapi_mi_put(mi, "matched", (double)d.matched);
    tsapi_mi_put(mi, "only_a", (double)d.only_a);
    tsapi_mi_put(mi, "only_b", (double)d.only_b);
    tsapi_mi_put(mi, "geometry_diff", (double)d.geometry_diff);
    tsapi_mi_put(mi, "attribute_diff", (double)d.attribute_diff);
    tsapi_mi_graph(mi, "diff", "Comparison counts", NULL,
                   "matched,only_a,only_b,geometry_diff,attribute_diff");
    if (d.crs_compatible != cases[c].crs_ok)
        TEST_VERDICT("CRS %scompatible after '%s'",
                     d.crs_compatible ? "" : "not ", change);
    if (!cases[c].crs_ok)
    {
        if (d.equal)
            TEST_VERDICT("Datasets in different CRS compare equal");
    }
    else
    {
        /* 'deleted' leaves two features only in A. */
        long long only_a = strcmp(change, "deleted") == 0 ? 2 :
                           cases[c].only_a;

        if (d.matched != cases[c].matched || d.only_a != only_a ||
            d.only_b != cases[c].only_b ||
            d.geometry_diff != cases[c].geom ||
            d.attribute_diff != cases[c].attr)
            TEST_VERDICT("'%s' compared as matched %lld, only A %lld, only "
                         "B %lld, geometry %lld, attribute %lld", change,
                         d.matched, d.only_a, d.only_b, d.geometry_diff,
                         d.attribute_diff);
        if (d.equal != (strcmp(change, "none") == 0 ||
                        strcmp(change, "reordered") == 0 ||
                        strcmp(change, "within_tolerance") == 0))
            TEST_VERDICT("'%s' is %sequal", change, d.equal ? "" : "not ");
    }
    tapi_geo_ds_diff_free(&d);

    TEST_SUCCESS;

cleanup:
    tapi_geo_dataset_close(sess.pco, da);
    tapi_geo_dataset_close(sess.pco, db);
    te_string_free(&a);
    te_string_free(&b);
    te_string_free(&ed);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
