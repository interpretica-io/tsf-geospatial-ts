/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Suite helpers
 *
 * @author Maxim Menshikov <maxim.menshikov@interpretica.io>
 */

#define TE_LGR_USER "TSAPI GEO"

#include "te_config.h"

#include <math.h>
#include <stdarg.h>
#include <string.h>

#include "logger_api.h"
#include "te_alloc.h"
#include "te_string.h"
#include "tapi_file.h"
#include "tapi_cfg_base.h"
#include "tapi_rpc_stdio.h"
#include "tapi_test_log.h"
#include "tapi_geo_dataset.h"

#include "tsapi_geo.h"
#include "tsapi_py.h"

/* Fixed, so the services prologue and the tests find the servers'
 * files; per-test scratch directories hang under it. */
#define TSAPI_GEO_OGC_DIR "tsf-geospatial-ogc"

static te_errno
run(tsapi_geo_session *s, const char *cmd)
{
    rpc_wait_status st;

    RPC_AWAIT_IUT_ERROR(s->pco);
    s->pco->timeout = 20 * 60 * 1000;
    st = rpc_system(s->pco, cmd);
    if (st.flag != RPC_WAIT_STATUS_EXITED || st.value != 0)
    {
        WARN("'%s' did not succeed (flag %d, value %u)", cmd, st.flag,
             st.value);
        return TE_RC(TE_TAPI, TE_ESHCMD);
    }
    return 0;
}

/* See description in tsapi_geo.h */
te_errno
tsapi_geo_sh(tsapi_geo_session *s, const char *fmt, ...)
{
    te_string cmd = TE_STRING_INIT;
    va_list ap;
    te_errno rc;

    va_start(ap, fmt);
    te_string_append_va(&cmd, fmt, ap);
    va_end(ap);
    rc = run(s, cmd.ptr);
    te_string_free(&cmd);
    return rc;
}

/* See description in tsapi_geo.h */
te_errno
tsapi_geo_sh_out(tsapi_geo_session *s, te_string *out, const char *fmt, ...)
{
    te_string cmd = TE_STRING_INIT;
    te_string tmp = TE_STRING_INIT;
    char *content = NULL;
    va_list ap;
    te_errno rc;

    tsapi_geo_work(s, "sh_out.txt", &tmp);
    va_start(ap, fmt);
    te_string_append_va(&cmd, fmt, ap);
    va_end(ap);
    te_string_append(&cmd, " > %s 2>&1", tmp.ptr);
    rc = run(s, cmd.ptr);
    if (tapi_file_read_ta(s->ta, tmp.ptr, &content) == 0)
    {
        te_string_append(out, "%s", content);
        free(content);
    }
    te_string_free(&cmd);
    te_string_free(&tmp);
    return rc;
}

/* See description in tsapi_geo.h */
te_errno
tsapi_geo_session_init(tsapi_geo_session *s, const char *name)
{
    char *tmp;
    te_string base = TE_STRING_INIT;
    te_errno rc;

    memset(s, 0, sizeof(*s));
    s->ta = TSAPI_GEO_TA;
    rc = rcf_rpc_server_create(s->ta, name, &s->pco);
    if (rc != 0)
    {
        ERROR("Cannot create the RPC server '%s' on %s: %r", name, s->ta, rc);
        return rc;
    }
    rc = tapi_geo_available(s->pco, &s->engine);
    if (rc != 0)
    {
        WARN("No geospatial engine in the RPC server of %s: %r", s->ta, rc);
        return TE_RC(TE_TAPI, TE_EOPNOTSUPP);
    }
    tmp = tapi_cfg_base_get_ta_dir(s->ta, TAPI_CFG_BASE_TA_DIR_TMP);
    if (tmp == NULL)
        return TE_RC(TE_TAPI, TE_EFAIL);
    tapi_file_make_custom_pathname(&base, tmp, "-tsfgeo");
    s->work = TE_STRDUP(base.ptr);
    free(tmp);
    te_string_free(&base);
    rc = tsapi_geo_sh(s, "mkdir -p %s", s->work);
    RING("Geospatial engine on %s: GEOS %s, PROJ %s, GDAL %s (%u drivers), "
         "features 0x%x; scratch %s", s->ta, s->engine.geos, s->engine.proj,
         s->engine.gdal, s->engine.drivers, s->engine.features, s->work);
    return rc;
}

/* See description in tsapi_geo.h */
void
tsapi_geo_session_fini(tsapi_geo_session *s)
{
    if (s->pco != NULL && s->work != NULL)
        tsapi_geo_sh(s, "rm -rf %s", s->work);
    free(s->work);
    s->work = NULL;
    if (s->pco != NULL)
    {
        rcf_rpc_server_destroy(s->pco);
        s->pco = NULL;
    }
}

/* See description in tsapi_geo.h */
const char *
tsapi_geo_work(const tsapi_geo_session *s, const char *name, te_string *out)
{
    te_string_append(out, "%s/%s", s->work != NULL ? s->work : "/tmp", name);
    return out->ptr;
}

/* See description in tsapi_geo.h */
te_errno
tsapi_geo_put(tsapi_geo_session *s, const char *name, const char *content)
{
    te_string p = TE_STRING_INIT;
    te_errno rc;

    tsapi_geo_work(s, name, &p);
    rc = tapi_file_create_ta(s->ta, p.ptr, "%s", content);
    if (rc != 0)
        ERROR("Cannot write %s on %s: %r", p.ptr, s->ta, rc);
    te_string_free(&p);
    return rc;
}

/* ---- MI ----------------------------------------------------------- */

te_mi_logger *
tsapi_mi_open(void)
{
    te_mi_logger *l = NULL;

    if (te_mi_logger_meas_create("tsf-geospatial", &l) != 0)
        return NULL;
    return l;
}

void
tsapi_mi_put(te_mi_logger *l, const char *name, double v)
{
    if (l != NULL && isfinite(v))
        te_mi_logger_add_meas(l, NULL, TE_MI_MEAS_UNITLESS_VALUE, name,
                              TE_MI_MEAS_AGGR_SINGLE, v,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
}

void
tsapi_mi_put_type(te_mi_logger *l, te_mi_meas_type type, const char *name,
                  double v, te_mi_meas_multiplier mult)
{
    if (l != NULL && isfinite(v))
        te_mi_logger_add_meas(l, NULL, type, name, TE_MI_MEAS_AGGR_SINGLE, v,
                              mult);
}

static void
graph(te_mi_logger *l, const char *view, const char *title, const char *x,
      te_mi_meas_type type, const char *ys)
{
    char *list;
    char *tok;
    char *save = NULL;

    if (l == NULL)
        return;
    te_mi_logger_add_meas_view(l, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH, view,
                               title);
    te_mi_logger_meas_graph_axis_add_name(l, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                                          view, TE_MI_GRAPH_AXIS_X,
                                          x != NULL ? x :
                                          TE_MI_GRAPH_AUTO_SEQNO);
    list = TE_STRDUP(ys);
    for (tok = strtok_r(list, ",", &save); tok != NULL;
         tok = strtok_r(NULL, ",", &save))
        te_mi_logger_meas_graph_axis_add(l, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                                         view, TE_MI_GRAPH_AXIS_Y, type, tok);
    free(list);
}

void
tsapi_mi_graph(te_mi_logger *l, const char *view, const char *title,
               const char *x, const char *ys)
{
    graph(l, view, title, x, TE_MI_MEAS_UNITLESS_VALUE, ys);
}

void
tsapi_mi_graph_type(te_mi_logger *l, const char *view, const char *title,
                    const char *x, te_mi_meas_type type, const char *ys)
{
    graph(l, view, title, x, type, ys);
}

void
tsapi_mi_close(te_mi_logger **l)
{
    if (*l != NULL)
        te_mi_logger_destroy(*l);
    *l = NULL;
}

/* ---- OGC servers -------------------------------------------------- */

static char *
ogc_dir(tsapi_geo_session *s)
{
    char *tmp = tapi_cfg_base_get_ta_dir(s->ta, TAPI_CFG_BASE_TA_DIR_TMP);
    te_string d = TE_STRING_INIT;

    if (tmp == NULL)
        return NULL;
    te_string_append(&d, "%s/%s", tmp, TSAPI_GEO_OGC_DIR);
    free(tmp);
    return d.ptr;
}

/* See description in tsapi_geo.h */
bool
tsapi_geo_ogc_url(const char *server, const char *service, const char *mode,
                  te_string *url)
{
    if (strcmp(server, "control") == 0)
    {
        const char *path = strcmp(service, "oapif") == 0 ? "ogcapi" : service;

        te_string_append(url, "http://127.0.0.1:%d/%s%s/%s",
                         TSAPI_GEO_CONTROL_PORT,
                         strcmp(mode, "no_auth") == 0 ||
                         strcmp(mode, "auth") == 0 ? "secure/" : "",
                         strcmp(mode, "auth") == 0 ? "good" : mode, path);
        return true;
    }
    if (strcmp(server, "mapserver") == 0)
    {
        if (strcmp(service, "oapif") == 0)
            return false;
        te_string_append(url, "http://127.0.0.1:%d/cgi-bin/mapserv.sh",
                         TSAPI_GEO_MAPSERVER_PORT);
        return true;
    }
    if (strcmp(server, "pygeoapi") == 0)
    {
        if (strcmp(service, "oapif") != 0)
            return false;
        te_string_append(url, "http://127.0.0.1:%d", TSAPI_GEO_PYGEOAPI_PORT);
        return true;
    }
    return false;
}

/* See description in tsapi_geo.h */
te_errno
tsapi_geo_ogc_servers_up(tsapi_geo_session *s)
{
    char *dir = ogc_dir(s);
    te_string p = TE_STRING_INIT;
    long long written = 0;
    te_errno rc;

    if (dir == NULL)
        return TE_RC(TE_TAPI, TE_EFAIL);
    rc = tsapi_geo_sh(s, "rm -rf %s && mkdir -p %s", dir, dir);
    if (rc != 0)
        goto out;
    te_string_append(&p, "%s/ogcserver.py", dir);
    rc = tapi_file_create_ta(s->ta, p.ptr, "%s", tsapi_py_ogcserver);
    te_string_reset(&p);
    te_string_append(&p, "%s/servers.sh", dir);
    if (rc == 0)
        rc = tapi_file_create_ta(s->ta, p.ptr, "%s", tsapi_py_servers);
    if (rc != 0)
        goto out;
    /* What the real servers serve: a 10 x 5 grid of cells near Porto,
     * written by the module under test, ids 1..50. */
    te_string_reset(&p);
    te_string_append(&p, "%s/cells.gpkg", dir);
    rc = tapi_geo_dataset_generate(s->pco, p.ptr, "GPKG", "EPSG:4326", "grid",
                                   "cells=1\nnx=10\nny=5\nbounds=-8.7 41.1 "
                                   "-7.7 41.6\nlayer_name=cells", NULL,
                                   &written, NULL, NULL);
    te_string_reset(&p);
    te_string_append(&p, "%s/cells.geojson", dir);
    if (rc == 0)
        rc = tapi_geo_dataset_generate(s->pco, p.ptr, "GeoJSON", "EPSG:4326",
                                       "grid", "cells=1\nnx=10\nny=5\n"
                                       "bounds=-8.7 41.1 -7.7 41.6\n"
                                       "layer_name=cells", NULL, &written,
                                       NULL, NULL);
    if (rc != 0)
        goto out;
    rc = tsapi_geo_sh(s, "bash %s/servers.sh ogc-up %s %d %d %d", dir, dir,
                      TSAPI_GEO_CONTROL_PORT, TSAPI_GEO_MAPSERVER_PORT,
                      TSAPI_GEO_PYGEOAPI_PORT);
out:
    te_string_free(&p);
    free(dir);
    return rc;
}

/* See description in tsapi_geo.h */
te_errno
tsapi_geo_ogc_servers_down(tsapi_geo_session *s)
{
    char *dir = ogc_dir(s);
    te_errno rc;

    if (dir == NULL)
        return TE_RC(TE_TAPI, TE_EFAIL);
    rc = tsapi_geo_sh(s, "test ! -f %s/servers.sh || bash %s/servers.sh "
                      "ogc-down %s", dir, dir, dir);
    free(dir);
    return rc;
}

/* See description in tsapi_geo.h */
bool
tsapi_geo_ogc_server_up(tsapi_geo_session *s, const char *server)
{
    int port = strcmp(server, "control") == 0 ? TSAPI_GEO_CONTROL_PORT :
               strcmp(server, "mapserver") == 0 ? TSAPI_GEO_MAPSERVER_PORT :
               TSAPI_GEO_PYGEOAPI_PORT;

    return tsapi_geo_sh(s, "python3 -c 'import socket; socket.create_"
                        "connection((\"127.0.0.1\", %d), 2).close()'",
                        port) == 0;
}

/* ---- PostGIS ------------------------------------------------------ */

static te_errno
pg(tsapi_geo_session *s, const char *what)
{
    char *dir = ogc_dir(s);
    te_string p = TE_STRING_INIT;
    te_errno rc;

    if (dir == NULL)
        return TE_RC(TE_TAPI, TE_EFAIL);
    rc = tsapi_geo_sh(s, "mkdir -p %s", dir);
    te_string_append(&p, "%s/servers.sh", dir);
    if (rc == 0)
        rc = tapi_file_create_ta(s->ta, p.ptr, "%s", tsapi_py_servers);
    if (rc == 0)
        rc = tsapi_geo_sh(s, "TSF_GEO_PG_PORT=%d bash %s %s %s",
                          TSAPI_GEO_PG_PORT, p.ptr, what, dir);
    te_string_free(&p);
    free(dir);
    return rc;
}

te_errno
tsapi_geo_postgis_up(tsapi_geo_session *s)
{
    return pg(s, "pg-up");
}

te_errno
tsapi_geo_postgis_down(tsapi_geo_session *s)
{
    return pg(s, "pg-down");
}
