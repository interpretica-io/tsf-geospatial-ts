/** @file
 * @brief tracking group
 *
 * Distance to a route: a vehicle driving parallel to a straight route
 * at a known lateral offset (25 m, 100 m) is that far from it at every
 * fix, measured geodesically; driving on it, zero. Fixes beyond a
 * tolerance are counted off-route.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "tracking/route"

#include "te_config.h"
#include "tapi_test.h"

#include <math.h>

#include "tapi_geo_track.h"
#include "tapi_geo_crs.h"
#include "tsapi_geo.h"

int
main(int argc, char **argv)
{
    tsapi_geo_session sess = { 0 };
    int offset;
    tapi_geo_geometry *route = NULL;
    tapi_geo_track *t = NULL;
    tapi_geo_obs obs[200];
    double xy[2 * 200], ll[2 * 200];
    tapi_geo_track_error e;
    tapi_geo_xform_info xi;
    te_mi_logger *mi = NULL;
    unsigned int i;

    TEST_START;
    TEST_GET_INT_PARAM(offset);

    TEST_STEP("Open a session on the agent");
    TSAPI_GEO_SESSION(&sess, "pco_trk_route");
    mi = tsapi_mi_open();

    TEST_STEP("A 10 km route due north in PT-TM06, fixes %d m east of it",
              offset);
    TSAPI_GEO_WKT(&sess, "LINESTRING (0 100000, 0 110000)", "EPSG:3763",
                  route);
    for (i = 0; i < 200; i++)
    {
        xy[2 * i] = offset;
        xy[2 * i + 1] = 100250 + 47.5 * i;
    }
    CHECK_RC(tapi_geo_transform_coords(sess.pco, "EPSG:3763", "EPSG:4326",
                                       TAPI_GEO_AXIS_GIS, NULL, xy, 200, 2,
                                       ll, &xi));
    for (i = 0; i < 200; i++)
    {
        obs[i].t = i;
        obs[i].x = ll[2 * i];
        obs[i].y = ll[2 * i + 1];
        obs[i].z = NAN;
    }
    CHECK_RC(tapi_geo_track_load(sess.pco, obs, 200, "EPSG:4326",
                                 TAPI_GEO_AXIS_GIS, &t));

    TEST_STEP("Distance of every fix to the route");
    CHECK_RC(tapi_geo_track_route(sess.pco, t, route, 10, &e));
    RING("offset %d m: mean %.3f, max %.3f, off route %lld", offset, e.mean,
         e.max, e.over);
    for (i = 0; i < e.n_errors; i++)
        tsapi_mi_put(mi, "distance_m", e.errors[i]);
    tsapi_mi_graph(mi, "dist", "Distance of each fix to the route (m)", NULL,
                   "distance_m");
    /* PT-TM06 scale near its central meridian is ~1: a projected metre
     * is a ground metre to 1e-4 here. */
    if (fabs(e.mean - offset) > 0.01 * offset + 0.01 ||
        fabs(e.max - offset) > 0.01 * offset + 0.01)
        TEST_VERDICT("Fixes %d m off the route measured %.3f m (max %.3f)",
                     offset, e.mean, e.max);
    if (e.over != (offset > 10 ? 200 : 0))
        TEST_VERDICT("%lld fixes off route at tolerance 10 m", e.over);
    tapi_geo_track_error_free(&e);

    TEST_SUCCESS;

cleanup:
    tapi_geo_track_free(sess.pco, t);
    tapi_geo_geometry_free(sess.pco, route);
    tsapi_mi_close(&mi);
    tsapi_geo_session_fini(&sess);
    TEST_END;
}
