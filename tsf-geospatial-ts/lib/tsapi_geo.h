/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Suite helpers
 *
 * What every test needs: an RPC server on the agent (the geospatial
 * engine runs in it), a scratch directory there, shell and file access
 * for fixtures, and thin wrappers over TE's MI measurement API so every
 * test charts what it measured. The services group adds the OGC
 * servers it tests against; the postgis group the database.
 *
 * @author Maxim Menshikov <maxim.menshikov@interpretica.io>
 */

#ifndef __TSAPI_GEO_H__
#define __TSAPI_GEO_H__

#include "rcf_rpc.h"
#include "te_errno.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_geo.h"

#ifdef __cplusplus
extern "C" {
#endif

/** The agent this suite works on. */
#define TSAPI_GEO_TA    "Agt_A"

/** One test's view of the agent. Zero-initialise before init. */
typedef struct tsapi_geo_session {
    const char      *ta;
    rcf_rpc_server  *pco;
    char            *work;      /**< scratch directory for this test */
    tapi_geo_engine  engine;
} tsapi_geo_session;

/**
 * Create the RPC server and the scratch directory, and read the
 * engine's versions. TE_EOPNOTSUPP when the agent has no engine (the
 * caller skips).
 */
extern te_errno tsapi_geo_session_init(tsapi_geo_session *s,
                                       const char *name);

extern void tsapi_geo_session_fini(tsapi_geo_session *s);

/** Open a session or skip the test. */
#define TSAPI_GEO_SESSION(_s, _name) \
    do {                                                                \
        te_errno _rc = tsapi_geo_session_init((_s), (_name));           \
        if (TE_RC_GET_ERROR(_rc) == TE_EOPNOTSUPP)                      \
            TEST_SKIP("The agent has no geospatial engine");            \
        CHECK_RC(_rc);                                                  \
    } while (0)

/** A path in the test's scratch directory (appended to @p out). */
extern const char *tsapi_geo_work(const tsapi_geo_session *s,
                                  const char *name, te_string *out);

/** Write a file on the agent. */
extern te_errno tsapi_geo_put(tsapi_geo_session *s, const char *name,
                              const char *content);

/** Run a shell command on the agent; 0 when it exits 0. */
extern te_errno tsapi_geo_sh(tsapi_geo_session *s, const char *fmt, ...)
    TE_LIKE_PRINTF(2, 3);

/** Run a command on the agent and capture its standard output. */
extern te_errno tsapi_geo_sh_out(tsapi_geo_session *s, te_string *out,
                                 const char *fmt, ...) TE_LIKE_PRINTF(3, 4);

/** |a - b| <= tol */
static inline bool
tsapi_geo_near(double a, double b, double tol)
{
    return a - b <= tol && b - a <= tol;
}

/** Parse a fixture WKT into @p _g or stop the test with a verdict. */
#define TSAPI_GEO_WKT(_s, _wkt, _crs, _g) \
    do {                                                                \
        if (tapi_geo_parse((_s)->pco, "wkt", (_wkt), (_crs), NULL,      \
                           &(_g), NULL) != 0)                           \
            TEST_VERDICT("A fixture geometry does not parse: %s",       \
                         tapi_geo_last_error());                        \
    } while (0)

/* ---- MI charts -------------------------------------------------- */

/** A measurement artifact for tool "tsf-geospatial". */
extern te_mi_logger *tsapi_mi_open(void);

/** One unitless data point of metric @p name. */
extern void tsapi_mi_put(te_mi_logger *l, const char *name, double v);

/** One data point of a typed metric (memory, time, rate...). */
extern void tsapi_mi_put_type(te_mi_logger *l, te_mi_meas_type type,
                              const char *name, double v,
                              te_mi_meas_multiplier mult);

/**
 * A line graph: @p x names the X metric (NULL: the sequence number),
 * @p ys is a comma-separated list of Y metrics.
 */
extern void tsapi_mi_graph(te_mi_logger *l, const char *view,
                           const char *title, const char *x,
                           const char *ys);

/** The same for typed Y metrics, all of one type. */
extern void tsapi_mi_graph_type(te_mi_logger *l, const char *view,
                                const char *title, const char *x,
                                te_mi_meas_type type, const char *ys);

/** Flush and free (NULL-safe). */
extern void tsapi_mi_close(te_mi_logger **l);

/* ---- OGC servers (services group) -------------------------------- */

/** Base URLs of the servers the services group tests. */
#define TSAPI_GEO_CONTROL_PORT   18741  /**< the suite's own server */
#define TSAPI_GEO_MAPSERVER_PORT 18742  /**< MapServer CGI */
#define TSAPI_GEO_PYGEOAPI_PORT  18743  /**< pygeoapi */

/** Fixture credentials of the control server's /secure area. */
#define TSAPI_GEO_OGC_USER       "tsf-geo"
#define TSAPI_GEO_OGC_PASSWORD   "tsf-geo-fixture-3Wq"

/**
 * The URL of a server: @p server is "control", "mapserver" or
 * "pygeoapi"; @p service "wms", "wfs" or "oapif"; @p mode a control
 * server behaviour ("good", "bad_bbox", "bad_crs", "invalid_geom",
 * "paging_dup", "xml_exception", "wrong_size", "no_auth").
 *
 * @return @c false when that server does not offer that service.
 */
extern bool tsapi_geo_ogc_url(const char *server, const char *service,
                              const char *mode, te_string *url);

/**
 * Start the OGC servers on the agent (suite services prologue): the
 * control server always, MapServer and pygeoapi when installed. The
 * fixtures they serve are written first.
 */
extern te_errno tsapi_geo_ogc_servers_up(tsapi_geo_session *s);

extern te_errno tsapi_geo_ogc_servers_down(tsapi_geo_session *s);

/** Is a server up (its port answers)? */
extern bool tsapi_geo_ogc_server_up(tsapi_geo_session *s,
                                    const char *server);

/* ---- PostGIS (postgis group) ------------------------------------- */

#define TSAPI_GEO_PG_PORT        55441
#define TSAPI_GEO_PG_USER        "geo"
#define TSAPI_GEO_PG_PASSWORD    "tsf-geo-pg-fixture-8Ns"
#define TSAPI_GEO_PG_DB          "geo"

/** Start a PostGIS container (needs docker on the agent). */
extern te_errno tsapi_geo_postgis_up(tsapi_geo_session *s);
extern te_errno tsapi_geo_postgis_down(tsapi_geo_session *s);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TSAPI_GEO_H__ */
