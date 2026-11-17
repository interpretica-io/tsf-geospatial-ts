/** @file
 * @brief tracking group
 *
 * Sanity of a series of positions: a 600-sample trajectory (12 m/s,
 * one sample a second) with one defect made on purpose - a jump, a
 * gap, two samples out of order, a repeated timestamp, an impossible
 * speed - is reported with exactly that defect, and the clean series
 * with none. Its length is speed x time on the ellipsoid.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "tracking/sanity"

#include "te_config.h"
#include "tapi_test.h"

#include <math.h>

#include "tapi_geo_track.h"
#include "tsapi_geo.h"

#define N 600

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    const char *defect;
    tapi_geo_gen_item *items = NULL;
    unsigned int n = 0, i, m = 0;
    tapi_geo_obs *obs = NULL;
    tapi_geo_track *t = NULL;
    tapi_geo_track_limits lim = { 30, 5, 200, 1 };
    tapi_geo_track_sanity s;
    te_mi_logger *mi = NULL;

    TEST_START;
    TEST_GET_STRING_PARAM(defect);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_trk_sanity");
    mi = tsapi_mi_open();

    TEST_STEP("A 600 s trajectory at 12 m/s near Porto");
    CHECK_RC(tapi_geo_generate(sess.pco, "trajectory",
                               "count=600\nspeed=12\ninterval=1\nheading=60\n"
                               "turn=0\nstart=-8.6 41.1", &items, &n, NULL));
    obs = calloc(n, sizeof(*obs));
    for (i = 0; i < n; i++)
    {
        double x, y;

        sscanf(items[i].text, "POINT (%lf %lf)", &x, &y);
        if (strcmp(defect, "gap") == 0 && i >= 300 && i < 320)
            continue;               /* 20 s without a fix */
        obs[m].t = items[i].t;
        obs[m].x = x;
        obs[m].y = y;
        obs[m].z = NAN;
        if (strcmp(defect, "jump") == 0 && i == 200)
            obs[m].y += 0.01;       /* ~1.1 km north and back */
        if (strcmp(defect, "duplicate_time") == 0 && i == 400)
            obs[m].t = obs[m - 1].t;
        if (strcmp(defect, "speed") == 0 && i >= 450)
            obs[m].x += 0.002;      /* ~170 m in one second, once */
        m++;
    }
    if (strcmp(defect, "out_of_order") == 0)
    {
        tapi_geo_obs tmp = obs[100];

        obs[100] = obs[101];
        obs[101] = tmp;
    }
    CHECK_RC(tapi_geo_track_load(sess.pco, obs, m, "EPSG:4326",
                                 TAPI_GEO_AXIS_GIS, &t));

    TEST_STEP("Check it");
    CHECK_RC(tapi_geo_track_check(sess.pco, t, &lim, &s));
    RING("samples %lld, out of order %lld, duplicate times %lld, gaps %lld "
         "(missing %lld), jumps %lld, impossible %lld; %.1f m in %.0f s, "
         "max %.1f m/s\n%s", s.samples, s.out_of_order, s.duplicate_time,
         s.gaps, s.missing, s.jumps, s.impossible, s.length_m, s.duration_s,
         s.max_speed_ms, s.events.ptr != NULL ? s.events.ptr : "");
    tsapi_mi_put(mi, "out_of_order", (double)s.out_of_order);
    tsapi_mi_put(mi, "duplicate_time", (double)s.duplicate_time);
    tsapi_mi_put(mi, "gaps", (double)s.gaps);
    tsapi_mi_put(mi, "jumps", (double)s.jumps);
    tsapi_mi_put(mi, "impossible", (double)s.impossible);
    tsapi_mi_put_type(mi, TE_MI_MEAS_UNITLESS_VALUE, "max_speed_ms", s.max_speed_ms,
                      TE_MI_MEAS_MULTIPLIER_PLAIN);
    tsapi_mi_graph(mi, "defects", "Defects found", NULL,
                   "out_of_order,duplicate_time,gaps,jumps,impossible");
    tsapi_mi_graph(mi, "speed", "Highest speed between samples (m/s)", NULL,
                   "max_speed_ms");

    if (strcmp(defect, "none") == 0)
    {
        if (!s.ok || s.missing != 0)
            TEST_VERDICT("A clean trajectory has defects");
        if (!tsapi_geo_near(s.length_m, 12.0 * 599, 1e-6))
            TEST_VERDICT("Length %.6f m, expected %.0f", s.length_m,
                         12.0 * 599);
    }
    else if (s.ok)
        TEST_VERDICT("The %s was not found", defect);
    if (strcmp(defect, "jump") == 0 && (s.jumps != 2 || s.impossible != 2))
        TEST_VERDICT("A jump out and back gave %lld jumps, %lld impossible "
                     "speeds", s.jumps, s.impossible);
    if (strcmp(defect, "gap") == 0 && (s.gaps != 1 || s.missing != 20))
        TEST_VERDICT("A 21 s gap gave %lld gaps, %lld missing", s.gaps,
                     s.missing);
    if (strcmp(defect, "out_of_order") == 0 && s.out_of_order != 1)
        TEST_VERDICT("A swapped pair gave %lld out of order", s.out_of_order);
    if (strcmp(defect, "duplicate_time") == 0 && s.duplicate_time != 1)
        TEST_VERDICT("A repeated time gave %lld duplicates",
                     s.duplicate_time);
    if (strcmp(defect, "speed") == 0 && (s.impossible != 1 || s.jumps != 0))
        TEST_VERDICT("One fast step gave %lld impossible speeds, %lld jumps",
                     s.impossible, s.jumps);

    TEST_SUCCESS;

cleanup:
    tapi_geo_track_free(sess.pco, t);
    tapi_geo_gen_items_free(items, n);
    free(obs);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
