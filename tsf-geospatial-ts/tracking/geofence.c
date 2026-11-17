/** @file
 * @brief tracking group
 *
 * Geofences: a vehicle staying inside, leaving, entering, or never
 * coming near a zone; every sample judged against the expectation, the
 * crossings reported with their times.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "tracking/geofence"

#include "te_config.h"
#include "tapi_test.h"

#include <math.h>

#include "tapi_geo_track.h"
#include "tsapi_geo.h"

/* A 0.1 x 0.1 degree zone; the vehicle drives east along 41.15 N from
 * start_lon at 0.001 degree per sample. */
#define ZONE "POLYGON ((-8.65 41.10, -8.55 41.10, -8.55 41.20, -8.65 41.20, " \
             "-8.65 41.10))"

static const struct {
    const char *scenario;
    double start_lon;
    const char *expect;
    long long transitions;
    long long violations;
} cases[] = {
    { "inside", -8.64, "inside", 0, 0 },        /* 80 samples, all in */
    { "exit", -8.60, "inside", 1, 30 },         /* leaves at -8.55 */
    { "enter", -8.70, "outside", 1, 30 },       /* enters at -8.65 */
    { "outside", -8.90, "outside", 0, 0 },
};

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *scenario;
    tapi_geo_geometry *zone = NULL;
    tapi_geo_track *t = NULL;
    tapi_geo_obs obs[80];
    tapi_geo_geofence g;
    te_mi_logger *mi = NULL;
    unsigned int i;
    int c = -1;

    TEST_START;
    TEST_GET_STRING_PARAM(scenario);
    for (i = 0; i < TE_ARRAY_LEN(cases); i++)
        if (strcmp(cases[i].scenario, scenario) == 0)
            c = (int)i;
    if (c < 0)
        TEST_FAIL("Unknown scenario %s", scenario);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_trk_fence");
    mi = tsapi_mi_open();
    TSAPI_GEO_WKT(&sess, ZONE, "EPSG:4326", zone);
    for (i = 0; i < 80; i++)
    {
        /* Half-steps keep samples off the zone's edges. */
        obs[i].t = 1000 + i;
        obs[i].x = cases[c].start_lon + 0.001 * i + 0.0005;
        obs[i].y = 41.15;
        obs[i].z = NAN;
    }
    CHECK_RC(tapi_geo_track_load(sess.pco, obs, 80, "EPSG:4326",
                                 TAPI_GEO_AXIS_GIS, &t));

    TEST_STEP("Judge every sample: expected %s", cases[c].expect);
    CHECK_RC(tapi_geo_track_geofence(sess.pco, t, zone, cases[c].expect, &g));
    RING("inside %lld, outside %lld, transitions %lld, violations %lld "
         "(first at %g)\n%s", g.inside, g.outside, g.transitions,
         g.violations, g.first_violation_t,
         g.events.ptr != NULL ? g.events.ptr : "");
    tsapi_mi_put(mi, "inside", (double)g.inside);
    tsapi_mi_put(mi, "outside", (double)g.outside);
    tsapi_mi_put(mi, "violations", (double)g.violations);
    tsapi_mi_graph(mi, "fence", "Samples inside / outside / violating", NULL,
                   "inside,outside,violations");
    if (g.transitions != cases[c].transitions ||
        g.violations != cases[c].violations)
        TEST_VERDICT("%s: %lld transitions, %lld violations", scenario,
                     g.transitions, g.violations);
    if (g.ok != (cases[c].violations == 0))
        TEST_VERDICT("%s: verdict ok=%d", scenario, g.ok);
    if (cases[c].violations > 0 && !(g.first_violation_t == 1050))
        TEST_VERDICT("The first violation is at t=%g, expected 1050",
                     g.first_violation_t);
    if (cases[c].transitions > 0 && strstr(g.events.ptr,
                                           strcmp(scenario, "exit") == 0 ?
                                           "exit" : "enter") == NULL)
        TEST_VERDICT("The crossing is not reported");

    TEST_SUCCESS;

cleanup:
    tapi_geo_track_free(sess.pco, t);
    tapi_geo_geometry_free(sess.pco, zone);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
