#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved.
#
# The servers the services and postgis groups test against, started on
# the agent for one run and stopped after it.
#
# Usage: servers.sh ogc-up|ogc-down|pg-up|pg-down <work directory> \
#                   [control port] [mapserver port] [pygeoapi port]
#
# ogc-up expects <work>/cells.gpkg and <work>/cells.geojson (written by
# the prologue with tsf-geospatial itself) and <work>/ogcserver.py, and
# starts:
#  - the suite's control server (always; standard-library Python);
#  - MapServer as a CGI behind Python's CGI handler, WMS 1.3.0 and WFS
#    2.0.0 over cells.gpkg (when /usr/lib/cgi-bin/mapserv is there);
#  - pygeoapi, OGC API - Features over cells.geojson (when
#    /opt/pygeoapi/bin/pygeoapi is there).
# pg-up starts a PostGIS container on 127.0.0.1:${TSF_GEO_PG_PORT}
# (needs a docker client and socket). Every server listens on loopback.
set -euo pipefail

cmd="${1:?usage}"
work="${2:?usage}"
cport="${3:-18741}"
mport="${4:-18742}"
gport="${5:-18743}"
pg_name=tsf-geospatial-ts-postgis
pg_port="${TSF_GEO_PG_PORT:-55441}"
pg_image="${TSF_GEO_PG_IMAGE:-postgis/postgis:16-3.4}"

wait_port() {
    local i
    for i in $(seq 1 "${2:-60}") ; do
        if python3 - "$1" <<'PY' 2>/dev/null ; then return 0 ; fi
import socket, sys
s = socket.create_connection(("127.0.0.1", int(sys.argv[1])), 1)
s.close()
PY
        sleep 1
    done
    return 1
}

ogc_up() {
    cd "${work}"
    nohup python3 "${work}/ogcserver.py" "${cport}" tsf-geo \
        tsf-geo-fixture-3Wq > "${work}/control.log" 2>&1 &
    echo $! > "${work}/control.pid"
    wait_port "${cport}" 30 || { echo "control server did not start" >&2 ;
                                 exit 1 ; }

    if [ -x /usr/lib/cgi-bin/mapserv ] ; then
        mkdir -p "${work}/cgi-bin"
        cat > "${work}/geo.map" <<EOF_MAP
MAP
  NAME "tsfgeo"
  EXTENT -8.7 41.1 -7.7 41.6
  SIZE 256 256
  PROJECTION "init=epsg:4326" END
  WEB
    METADATA
      "ows_title" "tsf-geospatial MapServer fixture"
      "ows_onlineresource" "http://127.0.0.1:${mport}/cgi-bin/mapserv.sh?"
      "ows_srs" "EPSG:4326 EPSG:3857"
      "ows_enable_request" "*"
    END
  END
  LAYER
    NAME "cells"
    TYPE POLYGON
    STATUS ON
    CONNECTIONTYPE OGR
    CONNECTION "${work}/cells.gpkg"
    DATA "cells"
    PROJECTION "init=epsg:4326" END
    METADATA
      "ows_title" "cells"
      "gml_include_items" "all"
      "gml_featureid" "id"
      "gml_types" "auto"
    END
    CLASS STYLE COLOR 40 120 200 OUTLINECOLOR 0 0 0 END END
  END
END
EOF_MAP
        # MapServer 8 reads its configuration file first; the map file
        # comes from there, not from the request.
        printf 'CONFIG\n  ENV\n    MS_MAPFILE "%s/geo.map"\n  END\nEND\n' \
            "${work}" > "${work}/mapserver.conf"
        printf '#!/bin/sh\nexport MAPSERVER_CONFIG_FILE=%s/mapserver.conf\nexec /usr/lib/cgi-bin/mapserv\n' \
            "${work}" > "${work}/cgi-bin/mapserv.sh"
        chmod +x "${work}/cgi-bin/mapserv.sh"
        nohup python3 -W ignore -m http.server --cgi --bind 127.0.0.1 \
            "${mport}" > "${work}/mapserver.log" 2>&1 &
        echo $! > "${work}/mapserver.pid"
        wait_port "${mport}" 30 || echo "MapServer did not start" >&2
    fi

    if [ -x /opt/pygeoapi/bin/pygeoapi ] ; then
        cat > "${work}/pygeoapi.yml" <<YML
server:
  bind: {host: 127.0.0.1, port: ${gport}}
  url: http://127.0.0.1:${gport}
  mimetype: application/json; charset=UTF-8
  encoding: utf-8
  languages: [en-US]
  limits: {default_items: 10, max_items: 1000}
  map: {url: "https://tile.openstreetmap.org/{z}/{x}/{y}.png", attribution: "OSM"}
logging: {level: ERROR}
metadata:
  identification:
    title: tsf-geospatial pygeoapi fixture
    description: grid cells for the services group
    keywords: [test]
    keywords_type: theme
    terms_of_service: https://creativecommons.org/licenses/by/4.0/
    url: https://github.com/interpretica-io/tsf-geospatial
  license: {name: CC-BY 4.0, url: "https://creativecommons.org/licenses/by/4.0/"}
  provider: {name: tsf-geospatial, url: "https://github.com/interpretica-io/tsf-geospatial"}
  contact: {name: tsf-geospatial}
resources:
  cells:
    type: collection
    title: cells
    description: grid cells
    keywords: [cells]
    links: []
    extents:
      spatial: {bbox: [-8.7, 41.1, -7.7, 41.6], crs: "http://www.opengis.net/def/crs/OGC/1.3/CRS84"}
    providers:
      - type: feature
        name: GeoJSON
        data: ${work}/cells.geojson
        id_field: id
YML
        export PYGEOAPI_CONFIG="${work}/pygeoapi.yml"
        export PYGEOAPI_OPENAPI="${work}/openapi.yml"
        /opt/pygeoapi/bin/pygeoapi openapi generate "${PYGEOAPI_CONFIG}" \
            --output-file "${PYGEOAPI_OPENAPI}" > "${work}/pygeoapi.log" 2>&1
        nohup /opt/pygeoapi/bin/pygeoapi serve >> "${work}/pygeoapi.log" \
            2>&1 &
        echo $! > "${work}/pygeoapi.pid"
        wait_port "${gport}" 60 || echo "pygeoapi did not start" >&2
    fi
}

ogc_down() {
    local p
    for p in control mapserver pygeoapi ; do
        if [ -f "${work}/${p}.pid" ] ; then
            kill "$(cat "${work}/${p}.pid")" 2>/dev/null || true
            rm -f "${work}/${p}.pid"
        fi
    done
}

pg_up() {
    command -v docker >/dev/null || { echo "no docker client" >&2 ; exit 3 ; }
    docker rm -f "${pg_name}" >/dev/null 2>&1 || true
    printf 'POSTGRES_USER=geo\nPOSTGRES_PASSWORD=tsf-geo-pg-fixture-8Ns\nPOSTGRES_DB=geo\n' \
        > "${work}/pg.env"
    docker run -d --name "${pg_name}" --env-file "${work}/pg.env" \
        -p "127.0.0.1:${pg_port}:5432" "${pg_image}" >/dev/null
    rm -f "${work}/pg.env"
    # Up when it answers a query with PostGIS loaded, not when it opens
    # the port: the image restarts the server after initialisation.
    for i in $(seq 1 120) ; do
        if docker exec "${pg_name}" psql -U geo -d geo -tAc \
               "SELECT postgis_version()" >/dev/null 2>&1 ; then
            wait_port "${pg_port}" 30 && exit 0
        fi
        sleep 1
    done
    echo "PostGIS did not start" >&2
    docker logs "${pg_name}" 2>&1 | tail -20 >&2
    exit 1
}

pg_down() {
    docker rm -f "${pg_name}" >/dev/null 2>&1 || true
}

case "${cmd}" in
    ogc-up) ogc_up ;;
    ogc-down) ogc_down ;;
    pg-up) pg_up ;;
    pg-down) pg_down ;;
    *) echo "unknown command ${cmd}" >&2 ; exit 2 ;;
esac
