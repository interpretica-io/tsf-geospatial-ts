#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved.
#
# The suite's own OGC server: a small WMS 1.3.0 / WFS 2.0.0 capabilities
# endpoint and an OGC API - Features service over one fixed collection,
# standard library only. It exists for the known-bad controls: a mode in
# the URL path makes it wrong in one known way, so every check of
# tsf-geospatial's services group is shown to fire on a defect before
# its result on a real server (MapServer, pygeoapi) means anything.
#
#   http://127.0.0.1:PORT/<mode>/wms?SERVICE=WMS&REQUEST=GetCapabilities
#   http://127.0.0.1:PORT/<mode>/wfs?SERVICE=WFS&REQUEST=GetCapabilities
#   http://127.0.0.1:PORT/<mode>/ogcapi/...      (landing, conformance,
#                                                 collections, items)
#   http://127.0.0.1:PORT/secure/<mode>/...      (basic auth, unless the
#                                                 mode is no_auth)
#
# Modes: good, bad_bbox (axis-swapped geographic bounding boxes),
# bad_crs (a CRS nobody knows), invalid_geom (bow-tie features),
# paging_dup (the second page repeats the first), xml_exception (GetMap
# answers 200 with a ServiceException), wrong_size (GetMap image of the
# wrong size), no_auth (the /secure area lets anyone in).
#
# Usage: ogcserver.py <port> [user password]
import base64
import json
import struct
import sys
import zlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 18741
USER = sys.argv[2] if len(sys.argv) > 2 else "tsf-geo"
PASSWORD = sys.argv[3] if len(sys.argv) > 3 else "tsf-geo-fixture-3Wq"

# Ten 0.1 x 0.1 degree squares in a row near Porto, ids 1..10.
FEATURES = []
for i in range(10):
    x0 = -8.70 + 0.1 * i
    y0 = 41.10
    ring = [[x0, y0], [x0 + 0.1, y0], [x0 + 0.1, y0 + 0.1], [x0, y0 + 0.1],
            [x0, y0]]
    FEATURES.append({"type": "Feature", "id": i + 1,
                     "properties": {"name": "cell%d" % (i + 1), "rank": i},
                     "geometry": {"type": "Polygon", "coordinates": [ring]}})
EXTENT = [-8.70, 41.10, -7.70, 41.20]


def bowtie(f):
    x0, y0 = f["geometry"]["coordinates"][0][0]
    g = {"type": "Polygon", "coordinates": [[[x0, y0], [x0 + 0.1, y0 + 0.1],
                                             [x0 + 0.1, y0], [x0, y0 + 0.1],
                                             [x0, y0]]]}
    return dict(f, geometry=g)


def png(w, h, rgb=(40, 120, 200)):
    rows = []
    for y in range(h):
        row = bytearray(b"\x00")
        for x in range(w):
            row += bytes(rgb if (x // 8 + y // 8) % 2 == 0
                         else (250, 250, 250))
        rows.append(bytes(row))
    raw = b"".join(rows)

    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def wms_caps(mode):
    w, s, e, n = EXTENT
    # The per-CRS box of EPSG:4326 in its own axis order, latitude
    # first, as WMS 1.3.0 wants it.
    bb = (s, w, n, e)
    if mode == "bad_bbox":
        # The geographic box written latitude first: a typical swap.
        w, s, e, n = s, w, n, e
    crs = "EPSG:999999" if mode == "bad_crs" else "EPSG:4326"
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<WMS_Capabilities version="1.3.0" xmlns="http://www.opengis.net/wms">
  <Service><Name>WMS</Name><Title>tsf-geospatial control ({mode})</Title>
  </Service>
  <Capability>
    <Request>
      <GetCapabilities><Format>text/xml</Format></GetCapabilities>
      <GetMap><Format>image/png</Format></GetMap>
    </Request>
    <Layer>
      <Title>root</Title>
      <CRS>{crs}</CRS><CRS>EPSG:3857</CRS>
      <Layer queryable="1">
        <Name>cells</Name><Title>cells</Title>
        <EX_GeographicBoundingBox>
          <westBoundLongitude>{w}</westBoundLongitude>
          <eastBoundLongitude>{e}</eastBoundLongitude>
          <southBoundLatitude>{s}</southBoundLatitude>
          <northBoundLatitude>{n}</northBoundLatitude>
        </EX_GeographicBoundingBox>
        <BoundingBox CRS="EPSG:4326" minx="{bb[0]}" miny="{bb[1]}"
                     maxx="{bb[2]}" maxy="{bb[3]}"/>
      </Layer>
    </Layer>
  </Capability>
</WMS_Capabilities>
"""


def wfs_caps(mode):
    w, s, e, n = EXTENT
    if mode == "bad_bbox":
        w, s, e, n = s, w, n, e
    crs = ("urn:ogc:def:crs:EPSG::999999" if mode == "bad_crs"
           else "urn:ogc:def:crs:EPSG::4326")
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<wfs:WFS_Capabilities version="2.0.0" xmlns:wfs="http://www.opengis.net/wfs/2.0"
    xmlns:ows="http://www.opengis.net/ows/1.1">
  <ows:ServiceIdentification><ows:Title>tsf-geospatial control ({mode})</ows:Title>
  </ows:ServiceIdentification>
  <wfs:FeatureTypeList>
    <wfs:FeatureType>
      <wfs:Name>cells</wfs:Name>
      <wfs:DefaultCRS>{crs}</wfs:DefaultCRS>
      <wfs:OtherCRS>urn:ogc:def:crs:EPSG::3857</wfs:OtherCRS>
      <ows:WGS84BoundingBox>
        <ows:LowerCorner>{w} {s}</ows:LowerCorner>
        <ows:UpperCorner>{e} {n}</ows:UpperCorner>
      </ows:WGS84BoundingBox>
    </wfs:FeatureType>
  </wfs:FeatureTypeList>
</wfs:WFS_Capabilities>
"""


class H(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def send(self, code, ctype, body):
        if isinstance(body, str):
            body = body.encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def authorised(self):
        want = "Basic " + base64.b64encode(
            ("%s:%s" % (USER, PASSWORD)).encode()).decode()
        return self.headers.get("Authorization") == want

    def do_GET(self):
        u = urlparse(self.path)
        parts = [p for p in u.path.split("/") if p]
        q = {k.upper(): v[0] for k, v in parse_qs(u.query).items()}
        qa = {k: v[0] for k, v in parse_qs(u.query).items()}
        if parts and parts[0] == "secure":
            parts = parts[1:]
            mode = parts[0] if parts else "good"
            if mode != "no_auth" and not self.authorised():
                self.send_response(401)
                self.send_header("WWW-Authenticate", 'Basic realm="geo"')
                self.send_header("Content-Length", "0")
                self.end_headers()
                return
        if not parts:
            return self.send(404, "text/plain", "no mode")
        mode, rest = parts[0], parts[1:]
        if rest[:1] == ["wms"]:
            req = q.get("REQUEST", "")
            if req == "GetCapabilities":
                return self.send(200, "text/xml", wms_caps(mode))
            if req == "GetMap":
                if mode == "xml_exception":
                    return self.send(200, "application/vnd.ogc.se_xml",
                                     '<?xml version="1.0"?><ServiceException'
                                     'Report version="1.3.0"><ServiceExcep'
                                     'tion>Layer not queryable</ServiceExce'
                                     'ption></ServiceExceptionReport>')
                w = int(q.get("WIDTH", "256"))
                h = int(q.get("HEIGHT", "256"))
                if mode == "wrong_size":
                    w, h = w // 2, h // 2
                return self.send(200, "image/png", png(w, h))
            return self.send(400, "text/plain", "bad request")
        if rest[:1] == ["wfs"]:
            if q.get("REQUEST", "") == "GetCapabilities":
                return self.send(200, "text/xml", wfs_caps(mode))
            return self.send(400, "text/plain", "only capabilities here")
        if rest[:1] == ["ogcapi"]:
            return self.ogcapi(mode, rest[1:], qa)
        return self.send(404, "text/plain", "not found")

    def ogcapi(self, mode, p, q):
        base = "http://127.0.0.1:%d/%s/ogcapi" % (PORT, mode)
        js = "application/json"
        if not p:
            return self.send(200, js, json.dumps({
                "title": "tsf-geospatial control",
                "links": [{"rel": "conformance", "href": base + "/conformance"},
                          {"rel": "data", "href": base + "/collections"}]}))
        if p == ["conformance"]:
            return self.send(200, js, json.dumps({"conformsTo": [
                "http://www.opengis.net/spec/ogcapi-features-1/1.0/conf/core",
                "http://www.opengis.net/spec/ogcapi-features-1/1.0/conf/geojson"]}))
        coll = {"id": "cells", "title": "cells", "itemType": "feature",
                "extent": {"spatial": {"bbox": [EXTENT],
                           "crs": "http://www.opengis.net/def/crs/OGC/1.3/CRS84"}},
                "links": [{"rel": "items", "type": "application/geo+json",
                           "href": base + "/collections/cells/items"}]}
        if mode == "bad_bbox":
            w, s, e, n = EXTENT
            coll["extent"]["spatial"]["bbox"] = [[s, w, n, e]]
        if p == ["collections"]:
            return self.send(200, js, json.dumps({"collections": [coll],
                                                  "links": []}))
        if p == ["collections", "cells"]:
            return self.send(200, js, json.dumps(coll))
        if p == ["collections", "cells", "items"]:
            limit = int(q.get("limit", "10"))
            offset = int(q.get("offset", "0"))
            feats = FEATURES
            if mode == "invalid_geom":
                feats = [bowtie(f) if f["id"] % 3 == 0 else f for f in feats]
            # The defect: from the second page on, the server ignores
            # the offset and serves the first page again.
            start = 0 if (mode == "paging_dup" and offset > 0) else offset
            page = feats[start:start + limit]
            links = []
            if offset + limit < len(feats):
                links.append({"rel": "next", "type": "application/geo+json",
                              "href": "%s/collections/cells/items?limit=%d&"
                                      "offset=%d" % (base, limit,
                                                     offset + limit)})
            return self.send(200, "application/geo+json", json.dumps({
                "type": "FeatureCollection", "features": page,
                "numberMatched": len(feats), "numberReturned": len(page),
                "links": links}))
        return self.send(404, js, json.dumps({"code": "NotFound"}))


if __name__ == "__main__":
    ThreadingHTTPServer(("127.0.0.1", PORT), H).serve_forever()
