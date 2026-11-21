# EV Route Planner — Dehradun region

A desktop route planner for electric vehicles, written in C11 with
[raylib](https://www.raylib.com/). Pick two of nine cities around Dehradun
(Uttarakhand, India), enter your battery size, current charge and energy
use, and it finds the **fastest** route, counting both driving and charging
time, that never lets the battery fall below a 10% reserve. It stops to
charge at roadside stations when it has to.

It was built for a Design and Analysis of Algorithms course. The core is
a Dijkstra search over *(location, battery level)* states, shown next to
a plain Dijkstra baseline.

> **This is a planning demo, not GPS navigation.** Road distances, speeds,
> charger locations and charger power are assumed values. See
> [Limitations](#limitations).

---

## Features

- 9 cities: Dehradun, Mussoorie, Chakrata, Rishikesh, Haridwar, Saharanpur,
  Paonta Sahib, New Tehri, Roorkee.
- 17 city-to-city roads drawn along real OpenStreetMap road paths, with a
  road node about every 3 km and 36 charging stations on them.
- Battery-aware routing: fastest route, 10% reserve, 0.01 kWh battery
  resolution, charging stops chosen automatically.
- Map view with the route, start/end pins, numbered charging stops and
  all stations.
- Results: distance, travel time (with driving time), energy used,
  charging stops (with time plugged in), and battery on arrival.
- **Search analysis** panel: states settled and heap pushes for the
  battery-aware search, next to plain Dijkstra on the same graph.

## Build and run

Requirements: `gcc` (C11), `make`, `pkg-config` and raylib 6.x installed
so that `pkg-config --cflags --libs raylib` works. Developed on Linux
(WSL2).

```sh
make          # builds ./ev_planner
make run      # builds and starts it (run from the repository root)
```

Run the program from the repository root. It loads
`data/processed/vertices.csv`, `data/processed/roads.csv`,
`data/processed/region_map.png` and `assets/fonts/*.ttf` using relative
paths. On start-up it reads about 250 k OSM vertices and 600 k road edges
and builds the city graph before the window opens. On the development
machine that takes about a second.

**Controls:** choose *From* / *To* from the drop-downs (*Swap* flips them),
type the battery values, then click **Calculate route** or press
**Enter**. **Tab** moves between the number fields. **F11** toggles
fullscreen.

Input limits: capacity above 0 and up to 300 kWh, charge 0–100%, and
consumption above 0 and up to 5 kWh/km. The defaults are 60 kWh, 50% and
0.35 kWh/km. A charge below the 10% reserve is rejected.

### Other make targets

| Target          | What it does                                                                 |
|-----------------|-------------------------------------------------------------------------------|
| `make test`     | Builds and runs every `tests/test_*.c` (6 suites).                            |
| `make baseline` | Runs four fixed routes headlessly and diffs them against `docs/baseline_routes.txt`. |
| `make sanitize` | Same routes plus start-up and teardown under ASan, UBSan and LeakSanitizer.   |
| `make clean`    | Removes build output.                                                          |

## Architecture

```
src/
├── main.c                  start-up: load OSM graph, build city graph, GUI loop
├── graph/
│   ├── graph.{c,h}         adjacency-list graph (vertices + linked-list edges)
│   └── graph_loader.{c,h}  loads vertices.csv / roads.csv into a Graph
├── routing/
│   ├── dijkstra.{c,h}          plain Dijkstra (binary min-heap)
│   ├── battery_routing.{c,h}   battery-aware Dijkstra over (vertex, battery) states
│   └── route_service.{c,h}     CalculateRoute(): validation, search, summary, stats
├── battery/battery.{c,h}   battery model: capacity, reserve, consumption, charge time
├── stations/stations.{c,h} charging stations (vertex, power, status)
└── gui/gui.{c,h}           raylib UI: sidebar, map, results, search analysis
tests/                      unit tests (make test)
tools/baseline_routes.c     headless regression driver (make baseline)
scripts/process_roads.py    OSM extract -> vertices.csv / roads.csv
```

Data flow on start-up and per query:

1. `GraphLoadFromCSV` loads the **OSM road graph**: 248,367 vertices and
   596,560 directed edges.
2. `BuildCityGraph` (in `main.c`) uses it to build the much smaller
   **routing graph**: 9 cities + 285 road nodes = 294 vertices, 604
   directed segments, 36 of the road nodes being charging stations.
3. Each *Calculate route* calls `CalculateRoute`, which runs
   `BatteryAwareDijkstra` on the routing graph, then plain `Dijkstra` from
   the same city as a baseline. The results go to the GUI.

## How the road graph is built

### 1. OpenStreetMap extract → CSV (`scripts/process_roads.py`)

The raw data comes from Geofabrik's India extracts. Uttarakhand and
Saharanpur (Uttar Pradesh) are in the **Central Zone**. Paonta Sahib
(Himachal Pradesh) is in the **Northern Zone**. So both are clipped to the
map area, merged, and reduced to roads with
[osmium-tool](https://osmcode.org/osmium-tool/):

```sh
mkdir -p data/raw/osm && cd data/raw/osm
wget https://download.geofabrik.de/asia/india/central-zone/central-zone-latest.osm.pbf -O central-zone.osm.pbf
wget https://download.geofabrik.de/asia/india/northern-zone/northern-zone-latest.osm.pbf -O northern-zone.osm.pbf
B=76.90,29.65,78.75,30.95
osmium extract -b $B --strategy=complete_ways central-zone.osm.pbf  -o project-central.osm.pbf
osmium extract -b $B --strategy=complete_ways northern-zone.osm.pbf -o project-northern.osm.pbf
osmium merge project-central.osm.pbf project-northern.osm.pbf -o project-region.osm.pbf
osmium tags-filter project-region.osm.pbf w/highway -o uttarakhand-roads.osm.pbf
cd ../../.. && python3 scripts/process_roads.py      # needs pyosmium
```

`process_roads.py`:

- keeps ways tagged `highway=` motorway, trunk, primary, secondary,
  tertiary (and their `_link`s), unclassified, residential, living_street
  or service. It drops ways tagged `access=no/private`, `motor_vehicle=no`,
  `motorcar=no` or `area=yes`;
- splits each way at intersections (nodes shared by more than one way), so
  graph vertices are intersections and dead ends only;
- measures each segment with the haversine formula over all its points;
- writes one directed edge per allowed direction (`oneway=yes/-1` and
  roundabouts give one direction, everything else gives both);
- takes the speed from `maxspeed` when present, otherwise a default per
  road class;
- writes `vertices.csv`, `roads.csv` and `road_geometry.csv` (the full
  point list per edge, which the C program does not use).

`vertices.csv` and `roads.csv` are committed, so you only need these steps
to refresh the data.

### 2. CSV → routing graph (`BuildCityGraph` in `src/main.c`)

1. **Anchoring.** Each city is snapped to an OSM vertex that is in the
   connected network. Near Dehradun, up to 12 of the nearest vertices are
   tried. The one that reaches the most of the network (plain Dijkstra
   from each) becomes the root. Every other city takes the nearest of up
   to 40 vertices within 15 km that the root can reach. On the current
   data, all nine anchors are within 0.4 km of their city.
2. **Tracing.** For each of the 17 roads, plain Dijkstra (by distance) on
   the OSM graph finds the road path between the two anchors. Roads that
   start at the same city reuse one search, so this takes 7 searches
   instead of 17. If no path exists, that road falls back to a straight
   line. On the current data all 17 roads trace.
3. **Road nodes.** A node is placed along the traced path about every 3 km
   (at most 80 per road).
4. **Stations.** Each road has a fixed number of stations (1–3, 36 in
   total), spread evenly over its road nodes. Every third station is
   120 kW and the rest are 60 kW.
5. **Edges.** Consecutive nodes are joined in both directions. Each
   segment gets a share of the road's **manual** distance in proportion to
   its straight-line length, so the segments add up exactly to the
   assumed city-to-city distance. Each road has one assumed speed.
   Travel time is distance ÷ speed.

The OSM data decides where a road runs on the map. Its length and speed
come from the manual table in `main.c`.

## Algorithms

Notation: *V* vertices, *E* directed edges, *K* charging stations, and
*L* battery levels.

### Dijkstra with a binary min-heap (`src/routing/dijkstra.c`)

Standard single-source shortest paths by **distance**. It uses an array
binary min-heap with *lazy deletion*: instead of decrease-key, a vertex is
pushed again whenever its distance improves, and outdated heap entries are
skipped when popped. It runs to completion (no early exit) and computes
distances to every reachable vertex.

- Time: **O((V + E) log V)**. Each edge relaxation pushes at most one entry,
  so the heap holds at most *E* + 1 entries, and log *E* ≤ 2 log *V*.
- Space: **O(V + E)**: distance and predecessor arrays plus the heap.

It is used three times: to choose anchors and trace roads on the OSM graph,
and as the battery-free baseline in the search-analysis panel.

### Battery-aware Dijkstra (`src/routing/battery_routing.c`)

The battery level becomes part of the state. With capacity *C* kWh at
0.01 kWh resolution there are *L* = ⌊100·*C*⌋ + 1 levels, and the search
runs over the product graph of **S = V × L** states
*(vertex, energy in 0.01 kWh units)*.

From a state *(u, e)* there are two kinds of transition:

- **Drive** along an edge *u → v* needing *r* = ⌈distance × consumption ×
  100⌉ units, allowed only if *e − r* ≥ reserve. Cost: the edge's travel
  time.
- **Charge** at an operational station on *u* (when *e* < full), always to
  **full**. Cost: (energy added ÷ charger power) hours.

The cost is total time (driving + charging). All costs are non-negative, so
Dijkstra's greedy order is valid. The first time any state at the
destination is popped, it is the fastest feasible arrival over all battery
levels, and the search stops there. The path, charging stops and arrival
battery come from the predecessor array.

Energy is rounded conservatively: the starting charge is rounded down, and
the reserve and each road's consumption are rounded up.

- Time: **O((S + E·L) log S + S·K)**. Each state is settled at most once
  and relaxes its out-edges, so *E·L* drive transitions in total. Charging
  scans the *K* stations for each settled state. All *S* entries of each
  array are also initialised before every query.
- Space: **O(S)** for five per-state arrays (cost, distance, predecessor,
  action, station: 32 bytes per state), plus the heap.
- *L* grows with the battery capacity, so the algorithm is
  **pseudo-polynomial** in the input. With the 294-vertex routing graph and
  a 60 kWh battery, *S* = 294 × 6001 = 1,764,294 states, about 56 MB.

Measured on that graph (60 kWh, 0.35 kWh/km, `make baseline`):

| Query                         | Result                     | States settled | % of S |
|-------------------------------|----------------------------|---------------:|-------:|
| Dehradun → Mussoorie, 50%     | 34 km, no stop             |         28,119 |   1.6% |
| Dehradun → Chakrata, 50%      | 90 km, 1 stop (EV-03)      |        320,878 |  18.2% |
| Saharanpur → New Tehri, 50%   | 162 km, 1 stop (EV-30)     |        845,835 |  47.9% |
| Dehradun → Chakrata, 12%      | infeasible                 |             11 |  <0.1% |

Plain Dijkstra settles all 294 vertices for every query.

## Tests

`make test` runs six suites. `tests/test_routing_edge_cases.c` checks the
router on small hand-made graphs where the answer can be worked out by
hand:
- a trip needing no charge, and one needing two stops (with exact total
  time);
- an unavailable station, a road leg longer than the full range, and an
  unreachable destination;
- starting exactly at the reserve, and a trip where the reserve forces a
  top-up first;
- a detour to reach a charger;
- agreement with plain Dijkstra when the battery never binds;
- consistent search statistics, and `CalculateRoute` input validation.

`make baseline` checks that the four routes above are unchanged, and
`make sanitize` runs them under the address, undefined-behaviour and leak
sanitizers.

## Limitations

- **Not GPS navigation.** There is no live position, turn-by-turn guidance
  or traffic. Routes run between nine fixed cities only.
- **Assumed road values.** Each of the 17 roads has a manually assumed
  distance and a single assumed speed. The OSM path only decides where the
  road is drawn, and the drawn line joins road nodes about 3 km apart with
  straight segments.
- **Assumed chargers.** The 36 stations are evenly spaced along the roads
  by the program. They are not real charger locations, and their power
  (60 or 120 kW) is assumed. All of them are always available.
- **Simple charging model.** Charging always fills the battery to 100%,
  takes energy ÷ power hours (no slow-down near full), and has no queueing
  or plug-in overhead.
- **Simple energy model.** Consumption is a constant kWh/km: no effect from
  elevation, speed, temperature or load. The battery level is tracked in
  0.01 kWh steps.
- The map image is a fixed raster of the region (Web Mercator), so it
  becomes soft when the window is large.

## Credits and licences

- Map data and road network © **OpenStreetMap contributors**, available
  under the [Open Database Licence](https://www.openstreetmap.org/copyright).
  Extracts via [Geofabrik](https://download.geofabrik.de/).
- UI font: DejaVu Sans, see `assets/fonts/DejaVu-LICENSE.txt`.
- Built with [raylib](https://www.raylib.com/).
