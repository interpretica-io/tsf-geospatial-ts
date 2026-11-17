/** @file
 * @brief services group: the target a test checks
 *
 * A target names a server, a service and (for the control server) a
 * mode: "control_wms_bad_bbox", "mapserver_wfs", "pygeoapi_oapif".
 * The control modes other than "good" are known-bad controls: the check
 * must find their defect.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */
#ifndef __SVC_H__
#define __SVC_H__

#include "tapi_geo_ogc.h"
#include "tsapi_geo.h"

typedef struct svc_target {
    char server[16];
    char service[8];
    char mode[24];
    te_string url;
    tapi_geo_ogc_target t;
} svc_target;

/* Parse a target; skip the test when its server is not running. */
#define SVC_TARGET(_sess, _name, _tg) \
    do {                                                                \
        if (sscanf((_name), "%15[^_]_%7[^_]_%23s", (_tg)->server,       \
                   (_tg)->service, (_tg)->mode) < 2)                    \
            TEST_FAIL("Bad target %s", (_name));                        \
        if ((_tg)->mode[0] == '\0')                                     \
            snprintf((_tg)->mode, sizeof((_tg)->mode), "good");         \
        if (((_sess)->engine.features & TAPI_GEO_FEAT_CURL) == 0)       \
            TEST_SKIP("The engine was built without libcurl");          \
        if (!tsapi_geo_ogc_server_up((_sess), (_tg)->server))           \
            TEST_SKIP("%s is not running on the agent",                 \
                      (_tg)->server);                                   \
        if (!tsapi_geo_ogc_url((_tg)->server, (_tg)->service,           \
                               (_tg)->mode, &(_tg)->url))               \
            TEST_FAIL("%s does not offer %s", (_tg)->server,            \
                      (_tg)->service);                                  \
        memset(&(_tg)->t, 0, sizeof((_tg)->t));                         \
        (_tg)->t.service = strcmp((_tg)->service, "wfs") == 0 ?         \
                           TAPI_GEO_OGC_WFS :                           \
                           strcmp((_tg)->service, "oapif") == 0 ?       \
                           TAPI_GEO_OGC_OAPIF : TAPI_GEO_OGC_WMS;       \
        (_tg)->t.url = (_tg)->url.ptr;                                  \
    } while (0)

#endif
