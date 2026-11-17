/** @file
 * @brief postgis group: the connection every test opens
 *
 * The prologue starts a PostGIS container on the agent; tests connect
 * to it through tsf-sql. No server (no docker on the agent) skips.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */
#ifndef __PG_H__
#define __PG_H__

#include "tapi_sql.h"
#include "tapi_geo_postgis.h"
#include "tsapi_geo.h"

#define PG_CONNECT(_sess, _conn) \
    do {                                                                \
        tapi_sql_config _cfg = { 0 };                                   \
        tapi_sql_error _err;                                            \
                                                                        \
        if (!tapi_sql_supports((_sess)->pco, TAPI_SQL_ENGINE_POSTGRES)) \
            TEST_SKIP("The agent's tsf-sql has no PostgreSQL driver");  \
        _cfg.engine = TAPI_SQL_ENGINE_POSTGRES;                         \
        _cfg.host = "127.0.0.1";                                        \
        _cfg.port = TSAPI_GEO_PG_PORT;                                  \
        _cfg.dbname = TSAPI_GEO_PG_DB;                                  \
        _cfg.user = TSAPI_GEO_PG_USER;                                  \
        _cfg.password = TSAPI_GEO_PG_PASSWORD;                          \
        _cfg.tls_mode = TAPI_SQL_TLS_DISABLE;                           \
        if (tapi_sql_connect_err((_sess)->pco, &_cfg, &(_conn),         \
                                 &_err) != 0)                           \
            TEST_SKIP("No PostGIS server: %s", _err.message);           \
    } while (0)

#endif
