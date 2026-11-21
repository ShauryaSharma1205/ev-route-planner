#include "gui/gui.h"
#include "graph/graph.h"
#include "graph/graph_loader.h"
#include "routing/route_service.h"
#include "routing/dijkstra.h"
#include "stations/stations.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CITY_COUNT 9
#define MAX_STATIONS_PER_ROAD 6

/* Road nodes are placed roughly this far apart along each traced road. */
#define ROAD_NODE_SPACING_KM 3.0
#define MAX_NODES_PER_ROAD 80

static double HaversineKm(double lat1, double lon1,
                          double lat2, double lon2)
{
    const double rad = 0.017453292519943295;
    double dLat = (lat2 - lat1) * rad;
    double dLon = (lon2 - lon1) * rad;

    double a = sin(dLat / 2) * sin(dLat / 2) +
               cos(lat1 * rad) * cos(lat2 * rad) *
               sin(dLon / 2) * sin(dLon / 2);

    return 6371.0 * 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
}

#define MAX_CANDIDATES 40
#define ANCHOR_RADIUS_KM 15.0

/* The k nearest OSM vertices within maxKm, nearest first. Returns count. */
static int NearestCandidates(const Graph *osm, double lat, double lon,
                             double maxKm, int *out, double *dist, int k)
{
    int count = 0;

    for (size_t i = 0; i < osm->vertexCount; i++) {
        const Vertex *v = &osm->nodes[i].vertex;

        if (fabs(v->latitude - lat) > 0.2 ||
            fabs(v->longitude - lon) > 0.2)
            continue;

        double d = HaversineKm(lat, lon, v->latitude, v->longitude);

        if (d >= maxKm)
            continue;

        if (count == k && d >= dist[count - 1])
            continue;

        int pos = count < k ? count++ : count - 1;

        while (pos > 0 && dist[pos - 1] > d) {
            dist[pos] = dist[pos - 1];
            out[pos] = out[pos - 1];
            pos--;
        }

        dist[pos] = d;
        out[pos] = (int)i;
    }

    return count;
}

/*
 * Pick one OSM road vertex per city that is actually part of the connected
 * road network. The nearest vertex to a city is often an isolated footpath
 * or one-way stub, so the vertex with the largest reachable set near the
 * first city becomes the "root" and every other city uses its nearest
 * vertex that is reachable from that root.
 */
static void ChooseAnchors(const Graph *osm,
                          const double coords[][2],
                          const char *const names[],
                          int anchors[])
{
    for (int i = 0; i < CITY_COUNT; i++)
        anchors[i] = -1;

    if (!osm || osm->vertexCount == 0)
        return;

    int cand[MAX_CANDIDATES];
    double cdist[MAX_CANDIDATES];

    int nc = NearestCandidates(osm, coords[0][0], coords[0][1],
                               ANCHOR_RADIUS_KM, cand, cdist, 12);

    size_t bestReach = 0;
    int root = -1;

    for (int c = 0; c < nc; c++) {
        PathResult res;

        if (!Dijkstra(osm, cand[c], &res))
            continue;

        size_t reach = 0;

        for (size_t v = 0; v < osm->vertexCount; v++) {
            if (res.distance[v] < DBL_MAX)
                reach++;
        }

        DijkstraFree(&res);

        if (reach > bestReach) {
            bestReach = reach;
            root = cand[c];
        }

        if (reach > osm->vertexCount / 2)
            break;
    }

    if (root < 0) {
        printf("No connected OSM road found near %s.\n", names[0]);
        return;
    }

    PathResult reachable;

    if (!Dijkstra(osm, root, &reachable))
        return;

    printf("OSM road network reachable from %s: %zu of %zu vertices.\n",
           names[0], bestReach, osm->vertexCount);

    anchors[0] = root;

    for (int city = 1; city < CITY_COUNT; city++) {
        nc = NearestCandidates(osm, coords[city][0], coords[city][1],
                               ANCHOR_RADIUS_KM, cand, cdist,
                               MAX_CANDIDATES);

        for (int c = 0; c < nc; c++) {
            if (reachable.distance[cand[c]] < DBL_MAX) {
                anchors[city] = cand[c];
                printf("  %-12s road anchor %.1f km away\n",
                       names[city], cdist[c]);
                break;
            }
        }

        if (anchors[city] < 0)
            printf("  %-12s no connected road within %.0f km "
                   "(straight fallback)\n",
                   names[city], ANCHOR_RADIUS_KM);
    }

    DijkstraFree(&reachable);
}

/* Point located 'target' km along a polyline (cum = cumulative km). */
static void PointAlong(const double *lat, const double *lon,
                       const double *cum, size_t n, double target,
                       double *outLat, double *outLon)
{
    size_t i = 1;

    while (i < n - 1 && cum[i] < target)
        i++;

    double seg = cum[i] - cum[i - 1];
    double t = seg > 1e-9 ? (target - cum[i - 1]) / seg : 0.0;

    if (t < 0) t = 0;
    if (t > 1) t = 1;

    *outLat = lat[i - 1] + (lat[i] - lat[i - 1]) * t;
    *outLon = lon[i - 1] + (lon[i] - lon[i - 1]) * t;
}

typedef struct {
    int from;
    int to;
    double distance;   /* total road length, km */
    double speed;      /* km/h */
    int stations;      /* charging stations placed along this road */
    const char *name;
} RoadSpec;

/*
 * Build the routing graph:
 *   - 9 city vertices (ids 0..8)
 *   - many road nodes along every city-to-city road, following the real
 *     road path found by Dijkstra on the OSM graph (roughly every 3 km)
 *   - some of those road nodes are charging stations (36 in total)
 *
 * If the OSM graph cannot connect two cities, that road falls back to a
 * straight corridor with only its charging stations on it.
 *
 * Segment lengths are proportional to geographic span and add up to the
 * manual road distance, so city-to-city totals are unchanged.
 */
static bool BuildCityGraph(
    Graph *g,
    const Graph *osm,
    StationManager *stations,
    int *tracedRoads,
    int *fallbackRoads)
{
    GraphInit(g);

    const char *names[CITY_COUNT] = {
        "Dehradun",
        "Mussoorie",
        "Chakrata",
        "Rishikesh",
        "Haridwar",
        "Saharanpur",
        "Paonta Sahib",
        "New Tehri",
        "Roorkee"
    };

    const double coordinates[CITY_COUNT][2] = {
        {30.3165, 78.0322}, // 0 Dehradun
        {30.4598, 78.0644}, // 1 Mussoorie
        {30.7037, 77.8625}, // 2 Chakrata
        {30.0869, 78.2676}, // 3 Rishikesh
        {29.9457, 78.1642}, // 4 Haridwar
        {29.9640, 77.5460}, // 5 Saharanpur
        {30.4380, 77.6240}, // 6 Paonta Sahib
        {30.3753, 78.4804}, // 7 New Tehri
        {29.8543, 77.8880}  // 8 Roorkee
    };

    for (int i = 0; i < CITY_COUNT; i++) {
        if (GraphAddVertex(
                g,
                names[i],
                coordinates[i][0],
                coordinates[i][1]) < 0) {
            GraphFree(g);
            return false;
        }
    }

    int anchors[CITY_COUNT];
    ChooseAnchors(osm, coordinates, names, anchors);

    /* Roads with the same 'from' city are adjacent so the Dijkstra
       result from that city can be reused (7 searches, not 17). */
    const RoadSpec roads[] = {
        {0, 1,  34, 35, 1, "Dehradun-Mussoorie Road"},
        {0, 2,  90, 40, 3, "Dehradun-Chakrata Road"},
        {0, 3,  45, 40, 1, "Dehradun-Rishikesh Road"},
        {0, 4,  55, 45, 2, "Dehradun-Haridwar Road"},
        {0, 7, 105, 35, 3, "Dehradun-New Tehri Road"},
        {0, 5,  68, 50, 2, "Dehradun-Saharanpur Road"},
        {0, 6,  50, 45, 2, "Dehradun-Paonta Sahib Road"},
        {0, 8,  70, 50, 2, "Dehradun-Roorkee Road"},
        {1, 7,  95, 30, 3, "Mussoorie-New Tehri Road"},
        {2, 1, 110, 30, 3, "Chakrata-Mussoorie Road"},
        {3, 4,  25, 40, 1, "Rishikesh-Haridwar Road"},
        {3, 7,  75, 35, 2, "Rishikesh-New Tehri Road"},
        {4, 7, 115, 35, 3, "Haridwar-New Tehri Road"},
        {4, 8,  32, 50, 1, "Haridwar-Roorkee Road"},
        {8, 5,  30, 55, 1, "Roorkee-Saharanpur Road"},
        {6, 5,  90, 50, 3, "Paonta Sahib-Saharanpur Road"},
        {6, 2,  90, 35, 3, "Paonta Sahib-Chakrata Road"}
    };

    size_t roadCount = sizeof(roads) / sizeof(roads[0]);
    int stationNumber = 0;
    int roadNodeNumber = 0;

    int *pathBuf = NULL;

    if (osm && osm->vertexCount > 0)
        pathBuf = malloc(osm->vertexCount * sizeof(int));

    PathResult cache;
    bool cacheValid = false;
    int cacheCity = -1;
    int cacheSource = -1;

    bool ok = true;

    for (size_t r = 0; r < roadCount && ok; r++) {
        int n = roads[r].stations;

        double lat0 = coordinates[roads[r].from][0];
        double lon0 = coordinates[roads[r].from][1];
        double lat1 = coordinates[roads[r].to][0];
        double lon1 = coordinates[roads[r].to][1];

        /* ----- trace the real road path on the OSM graph ----- */

        size_t pathCount = 0;

        if (pathBuf) {
            if (cacheCity != roads[r].from) {
                if (cacheValid)
                    DijkstraFree(&cache);

                cacheCity = roads[r].from;
                cacheSource = anchors[roads[r].from];
                cacheValid = cacheSource >= 0 &&
                             Dijkstra(osm, cacheSource, &cache);

                printf("Tracing roads from %s...\n",
                       names[roads[r].from]);
            }

            int target = anchors[roads[r].to];

            if (cacheValid && target >= 0 && target != cacheSource) {
                pathCount = DijkstraBuildPath(
                    &cache, target, pathBuf, osm->vertexCount
                );
            }
        }

        bool traced = pathCount >= 2;

        size_t pn = traced ? pathCount + 2 : 2;

        double *plat = malloc(pn * sizeof(double));
        double *plon = malloc(pn * sizeof(double));
        double *pcum = malloc(pn * sizeof(double));

        if (!plat || !plon || !pcum) {
            free(plat);
            free(plon);
            free(pcum);
            ok = false;
            break;
        }

        plat[0] = lat0;
        plon[0] = lon0;

        if (traced) {
            for (size_t i = 0; i < pathCount; i++) {
                const Vertex *v = &osm->nodes[pathBuf[i]].vertex;
                plat[i + 1] = v->latitude;
                plon[i + 1] = v->longitude;
            }
        }

        plat[pn - 1] = lat1;
        plon[pn - 1] = lon1;

        pcum[0] = 0.0;

        for (size_t i = 1; i < pn; i++) {
            pcum[i] = pcum[i - 1] + HaversineKm(
                plat[i - 1], plon[i - 1], plat[i], plon[i]);
        }

        double length = pcum[pn - 1];

        if (traced)
            (*tracedRoads)++;
        else
            (*fallbackRoads)++;

        /* ----- decide how many nodes and which ones are stations ----- */

        int nodes = traced ? (int)(length / ROAD_NODE_SPACING_KM) : 0;

        if (nodes < n)
            nodes = n;

        if (nodes > MAX_NODES_PER_ROAD)
            nodes = MAX_NODES_PER_ROAD;

        bool isStation[MAX_NODES_PER_ROAD + 2] = {false};

        for (int j = 1; j <= n; j++) {
            int k = (int)lround((double)j * (nodes + 1) / (n + 1));

            if (k < 1) k = 1;
            if (k > nodes) k = nodes;

            for (int d = 0; d <= nodes; d++) {
                if (k + d <= nodes && !isStation[k + d]) {
                    k += d;
                    break;
                }
                if (k - d >= 1 && !isStation[k - d]) {
                    k -= d;
                    break;
                }
            }

            isStation[k] = true;
        }

        /* ----- create the nodes along the path ----- */

        int chain[MAX_NODES_PER_ROAD + 2];
        chain[0] = roads[r].from;

        for (int k = 1; k <= nodes && ok; k++) {
            double lat, lon;

            PointAlong(plat, plon, pcum, pn,
                       length * k / (nodes + 1), &lat, &lon);

            char name[MAX_VERTEX_NAME];

            if (isStation[k])
                snprintf(name, sizeof(name), "EV-%02d", ++stationNumber);
            else
                snprintf(name, sizeof(name), "R-%03d", ++roadNodeNumber);

            int id = GraphAddVertex(g, name, lat, lon);

            if (id < 0) {
                ok = false;
                break;
            }

            if (isStation[k]) {
                /* every third station is a faster 120 kW charger */
                double power = (stationNumber % 3 == 0) ? 120.0 : 60.0;

                if (StationAdd(stations, name, id, power,
                               STATION_OPERATIONAL) < 0) {
                    ok = false;
                    break;
                }
            }

            chain[k] = id;
        }

        free(plat);
        free(plon);
        free(pcum);

        if (!ok)
            break;

        chain[nodes + 1] = roads[r].to;

        /* ----- connect the chain, scaled to the manual road length ----- */

        double weight[MAX_NODES_PER_ROAD + 1];
        double total = 0.0;

        for (int s = 0; s <= nodes; s++) {
            const Vertex *a = &g->nodes[chain[s]].vertex;
            const Vertex *b = &g->nodes[chain[s + 1]].vertex;

            weight[s] = HaversineKm(a->latitude, a->longitude,
                                    b->latitude, b->longitude);

            if (weight[s] < 0.2)
                weight[s] = 0.2;

            total += weight[s];
        }

        for (int s = 0; s <= nodes; s++) {
            double segment = roads[r].distance * weight[s] / total;

            if (!GraphAddRoadWithSpeed(
                    g,
                    chain[s],
                    chain[s + 1],
                    segment,
                    roads[r].speed,
                    roads[r].name,
                    true)) {
                ok = false;
                break;
            }
        }
    }

    if (cacheValid)
        DijkstraFree(&cache);

    free(pathBuf);

    if (!ok) {
        GraphFree(g);
        return false;
    }

    return true;
}

int main(void)
{
    Graph roadGraph;
    Graph cityGraph;
    StationManager stations;
    Vector2 *stationPoints = NULL;
    size_t stationPointCount = 0;

    MapData map;
    PlannerGUI gui;

    GraphInit(&roadGraph);
    GraphInit(&cityGraph);
    StationManagerInit(&stations);
    MapDataInit(&map);

    puts("Loading OSM road network...");

    if (!GraphLoadFromCSV(
            &roadGraph,
            "data/processed/vertices.csv",
            "data/processed/roads.csv")) {

        fprintf(stderr, "Could not load road network.\n");
        GraphFree(&roadGraph);
        return 1;
    }

    printf(
        "Loaded %zu OSM vertices and %zu edges.\n",
        roadGraph.vertexCount,
        roadGraph.edgeCount
    );

    puts("Building city routing graph with charging stations...");

    int traced = 0;
    int fallback = 0;

    if (!BuildCityGraph(&cityGraph, &roadGraph, &stations,
                        &traced, &fallback)) {
        fprintf(stderr, "Could not build city graph.\n");
        StationManagerFree(&stations);
        GraphFree(&roadGraph);
        return 1;
    }

    printf(
        "Graph: %zu nodes = %d cities + %zu road nodes "
        "(%zu of them charging stations), %zu directed segments.\n",
        cityGraph.vertexCount,
        CITY_COUNT,
        cityGraph.vertexCount - CITY_COUNT,
        stations.count,
        cityGraph.edgeCount
    );

    printf(
        "Roads following real OSM paths: %d, straight fallback: %d.\n",
        traced,
        fallback
    );

    /* Station map markers: x = longitude, y = latitude */
    stationPointCount = stations.count;
    stationPoints = malloc(
        (stationPointCount ? stationPointCount : 1) * sizeof(Vector2)
    );

    if (!stationPoints) {
        fprintf(stderr, "Out of memory.\n");
        StationManagerFree(&stations);
        GraphFree(&cityGraph);
        GraphFree(&roadGraph);
        return 1;
    }

    for (size_t i = 0; i < stationPointCount; i++) {
        const Vertex *v =
            &cityGraph.nodes[stations.stations[i].vertexId].vertex;

        stationPoints[i] = (Vector2){
            (float)v->longitude,
            (float)v->latitude
        };
    }

    SetConfigFlags(
        FLAG_WINDOW_RESIZABLE |
        FLAG_VSYNC_HINT
    );

    InitWindow(
        1600,
        1000,
        "EV Charging Route Planner"
    );

    SetWindowMinSize(1100, 750);
    SetTargetFPS(60);

    /* Texture loading needs the graphics context created by InitWindow(). */
    if (!LoadMapImage(&map, "data/processed/region_map.png"))
        fprintf(stderr,
                "Warning: could not load data/processed/region_map.png\n");

    InitializeGUI(&gui);
    GUISetStations(&gui, stationPoints, stationPointCount);

    puts("Application ready.");

    while (!WindowShouldClose()) {

        if (IsKeyPressed(KEY_F11))
            ToggleFullscreen();

        UpdateGUI(&gui);

        if (gui.calculationRequested) {

            gui.calculationRequested = false;

            RouteSummary route = {0};

            double capacity =
                atof(gui.batteryCapacity);

            double charge =
                atof(gui.batteryCharge);

            double consumption =
                atof(gui.energyConsumption);

            bool success = CalculateRoute(
                &cityGraph,
                &stations,
                gui.origin,
                gui.destination,
                capacity,
                charge,
                consumption,
                &route
            );

            GUISetRoute(&gui, &route);

            if (!success) {
                printf("Route error: %s\n", route.message);
            } else {
                printf(
                    "%s -> %s: %.2f km\n",
                    gui.origin,
                    gui.destination,
                    route.distanceKm
                );

                printf(
                    "Estimated time: %.2f hours\n",
                    route.travelTimeHours
                );

                printf(
                    "Energy required: %.2f kWh\n",
                    route.energyRequiredKWh
                );

                printf(
                    "Remaining battery: %.2f kWh\n",
                    route.remainingEnergyKWh
                );

                printf(
                    "Charging stops: %zu (%.0f min charging)\n",
                    route.chargeStopCount,
                    route.chargingTimeHours * 60.0
                );
            }

            if (route.statsValid) {
                printf(
                    "Search: battery-aware %.3f ms, %zu of %zu states "
                    "settled, %zu heap pushes | plain Dijkstra %.3f ms, "
                    "%zu of %zu vertices settled\n",
                    route.searchMs,
                    route.statesSettled,
                    route.stateSpace,
                    route.heapPushes,
                    route.baselineMs,
                    route.baselineSettled,
                    route.graphVertices
                );
            }

            RouteSummaryFree(&route);
        }

        BeginDrawing();
        DrawGUI(&gui, &map);
        EndDrawing();
    }

    ShutdownGUI(&gui);
    FreeMapData(&map);

    CloseWindow();

    free(stationPoints);
    StationManagerFree(&stations);

    GraphFree(&cityGraph);
    GraphFree(&roadGraph);

    return 0;
}
