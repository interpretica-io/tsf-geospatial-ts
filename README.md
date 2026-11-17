# tsf-geospatial-ts

The Test Environment (TE) suite of
[tsf-geospatial](https://github.com/interpretica-io/tsf-geospatial):
geometry, reference systems, datasets, rasters, tracking, generation,
OGC services and PostGIS, processed on a Test Agent and checked from
the tests.

Every check is held against something the module did not compute
itself:

- published references: the worked examples of IOGP Guidance Note 7-2
  (one per projection method), GeographicLib's geodesic and planimeter
  examples;
- closed forms: areas of overlaid squares, buffers converging on
  pi r^2, the statistics of a linear gradient raster, the Rayleigh
  error of a receiver with Gaussian noise;
- defects planted at known places, by the generator or by an
  independent program (Python on the agent, `ogr2ogr`, SQL in PostGIS),
  that must be found exactly - no more, no fewer;
- a known-bad control next to every check: the suite's own OGC server
  (`lib/py/ogcserver.py`) has a mode for each defect a service check
  looks for (axis-swapped bounding boxes, unknown CRS, invalid
  geometries, a page served twice, an exception sent with 200, an image
  of the wrong size, a protected area that lets anyone in), and the
  bounds test runs corners-only to show what densifying prevents.

## Groups

| Group | What it covers |
|---|---|
| `geometry` | WKT/WKB/EWKB/GeoJSON round trips in XY, XYZ, XYM, XYZM; malformed input; DE-9IM predicates both ways round; overlay, buffer, simplify; planar and geodesic measures; every validation category; explicit repair; units; bounds and centroids |
| `crs` | lookup, GN7-2 examples forward and back, axis order, accuracy/ballpark/fallback reporting, areas of use, CRS comparison, densified bounds, geometry round trips with Z and M, invalid definitions |
| `vector` | every format writes and reads back the same features; planted defects found exactly; comparison of independently changed copies; a hand-written GeoJSON; CRS inconsistency across layers; 200 000 features in bounded memory; bounded fetches and filters |
| `raster` | GeoTIFF metadata for every pixel type and layout; tiled statistics against closed forms; comparison finding the one change; NoData; 8192 x 8192 in tiles; COG, PNG, MBTiles; window reads |
| `tracking` | jumps, gaps, ordering, repeated times, impossible speeds; error statistics; distance to a route; geofences; tracks from a projected GeoPackage |
| `generation` | determinism across RPC servers; every item is what it is promised to be; invalid only on request |
| `services` | WMS/WFS capabilities, GetMap, features, paging, access control, OGC API - Features conformance - against MapServer, pygeoapi and the control server |
| `postgis` | stored defects, SRIDs and types; spatial index use; PostGIS and GEOS agreeing on every predicate; stored vs expected geometries |
| `robust` | every limit; 400 mutated inputs per parser; cleanup after failure; handles scoped to their RPC server, RPC timeouts; driver allowlist |
| `perf` | parsing, transformation, predicate and dataset-loading throughput, cold and warm, against input complexity |

Every test charts what it measured (TE MI line graphs in the HTML log).

## Running

TE is expected beside the suite:

```bash
git clone https://github.com/interpretica-io/test-environment ../test-environment
./scripts/run.sh docker guess --cfg=localhost | tee run.out
./scripts/check-run.sh run.out
```

The Docker image (`scripts/docker/build/Dockerfile`, Ubuntu 24.04)
carries GEOS 3.12, PROJ 9.4, GDAL 3.8, MapServer (CGI) and pygeoapi;
the PostGIS server runs in a sibling container started through the
agent's docker client. Without docker on the agent the postgis group
skips; without MapServer or pygeoapi their iterations skip.
`TE_DOCKER_CACHE=<volume>` keeps the build tree between runs.

`run.sh` exits 0 whenever the run completes; `check-run.sh` decides
from the Tester's summary whether every iteration matched
`conf/trc.xml`. The TRC is generated from the packages:
`python3 scripts/gen-trc.py`.

## License

MIT. See [LICENSE](LICENSE).
