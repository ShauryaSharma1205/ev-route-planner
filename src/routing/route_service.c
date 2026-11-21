#include "route_service.h"
#include "battery_routing.h"
#include "dijkstra.h"

#include <float.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* Fraction of battery capacity that must never be used. */
#define RESERVE_FRACTION 0.10

static char Lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c + 'a' - 'A') : c;
}

static bool NamesEqual(const char *a, const char *b)
{
    while (*a && *b) {
        if (Lower(*a) != Lower(*b))
            return false;
        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

/* City names are matched against the graph's own vertex names. */
static int FindCity(const Graph *graph, const char *name)
{
    if (!graph || !name)
        return -1;

    for (size_t i = 0; i < graph->vertexCount; i++) {
        if (NamesEqual(name, graph->nodes[i].vertex.name))
            return (int)i;
    }

    return -1;
}

/* Monotonic-enough wall clock in milliseconds (C11 timespec_get). */
static double NowMs(void)
{
    struct timespec ts;

    if (timespec_get(&ts, TIME_UTC) != TIME_UTC)
        return 0.0;

    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

/* Plain distance-only Dijkstra from the same source, for comparison. */
static void RunBaseline(const Graph *graph, int source, int target,
                        RouteSummary *summary)
{
    PathResult result;
    double t0 = NowMs();

    if (!Dijkstra(graph, source, &result))
        return;

    summary->baselineMs = NowMs() - t0;
    summary->baselineSettled = result.settledCount;
    summary->baselinePushes = result.pushCount;
    summary->baselineDistanceKm = result.distance[target] < DBL_MAX
        ? result.distance[target] : -1.0;
    summary->baselineValid = true;

    DijkstraFree(&result);
}

static const GraphEdge *FindFastestEdge(const Graph *graph, int from, int to)
{
    const GraphEdge *best = NULL;

    for (const GraphEdge *e = graph->nodes[from].edges; e; e = e->next) {
        if (e->destination != to)
            continue;

        if (!best || e->travelTimeHours < best->travelTimeHours)
            best = e;
    }

    return best;
}

void RouteSummaryFree(RouteSummary *summary)
{
    if (!summary)
        return;

    free(summary->routePoints);
    free(summary->chargeStops);

    summary->routePoints = NULL;
    summary->chargeStops = NULL;
    summary->pathLength = 0;
    summary->routePointCount = 0;
    summary->chargeStopCount = 0;
}

bool CalculateRoute(
    const Graph *graph,
    const StationManager *stations,
    const char *origin,
    const char *destination,
    double batteryCapacityKWh,
    double batteryChargePercent,
    double consumptionKWhPerKm,
    RouteSummary *summary)
{
    if (!summary)
        return false;

    memset(summary, 0, sizeof(*summary));

    int source = FindCity(graph, origin);
    int target = FindCity(graph, destination);

    if (!graph || !stations || source < 0 || target < 0 ||
        batteryCapacityKWh <= 0 ||
        batteryChargePercent < 0 ||
        batteryChargePercent > 100 ||
        consumptionKWhPerKm <= 0) {

        snprintf(summary->message, sizeof(summary->message),
                 "Invalid route or battery input.");
        return false;
    }

    if (source == target) {
        snprintf(summary->message, sizeof(summary->message),
                 "Origin and destination are identical.");
        return false;
    }

    summary->sourceVertex = source;
    summary->destinationVertex = target;

    summary->originLat = graph->nodes[source].vertex.latitude;
    summary->originLon = graph->nodes[source].vertex.longitude;
    summary->destinationLat = graph->nodes[target].vertex.latitude;
    summary->destinationLon = graph->nodes[target].vertex.longitude;

    BatterySpec spec;
    BatteryState state;

    double reserve = batteryCapacityKWh * RESERVE_FRACTION;

    if (!BatteryInit(&spec, &state, batteryCapacityKWh,
                     batteryChargePercent, consumptionKWhPerKm,
                     reserve)) {
        snprintf(summary->message, sizeof(summary->message),
                 "Invalid route or battery input.");
        return false;
    }

    if (state.energyKWh < reserve) {
        snprintf(summary->message, sizeof(summary->message),
                 "Battery is below the 10%% safety reserve. "
                 "Charge before departing.");
        return false;
    }

    BatteryRoute route = {0};

    double t0 = NowMs();
    bool found = BatteryAwareDijkstra(graph, &spec, &state, stations,
                                      source, target, &route);
    summary->searchMs = NowMs() - t0;

    summary->stateSpace = route.stateSpace;
    summary->statesSettled = route.statesSettled;
    summary->heapPushes = route.heapPushes;
    summary->graphVertices = graph->vertexCount;
    summary->statsValid = true;

    RunBaseline(graph, source, target, summary);

    if (!found) {
        snprintf(summary->message, sizeof(summary->message),
                 "No feasible route: battery cannot reach the next "
                 "charging station.");
        return false;
    }

    size_t cap = route.stepCount;

    Vector2 *points = malloc(cap * sizeof(Vector2));
    Vector2 *stops = malloc(cap * sizeof(Vector2));

    if (!points || !stops) {
        free(points);
        free(stops);
        BatteryRouteFree(&route);

        snprintf(summary->message, sizeof(summary->message),
                 "Route allocation failed.");
        return false;
    }

    size_t pointCount = 0;
    size_t stopCount = 0;
    int previous = -1;
    double drivingTime = 0.0;

    char stopList[100];
    size_t listUsed = 0;
    stopList[0] = '\0';

    for (size_t i = 0; i < route.stepCount; i++) {
        const RouteStep *step = &route.steps[i];
        int v = step->vertexId;
        const Vertex *vertex = &graph->nodes[v].vertex;

        Vector2 position = {
            (float)vertex->longitude,
            (float)vertex->latitude
        };

        /* A charging step repeats the same vertex: don't duplicate it. */
        if (v != previous) {
            points[pointCount++] = position;

            if (previous >= 0) {
                const GraphEdge *edge =
                    FindFastestEdge(graph, previous, v);

                if (edge)
                    drivingTime += edge->travelTimeHours;
            }
        }

        if (step->isChargingStop) {
            stops[stopCount++] = position;

            int n = snprintf(stopList + listUsed,
                             sizeof(stopList) - listUsed,
                             "%s%s",
                             listUsed ? ", " : "",
                             vertex->name);

            if (n > 0 && (size_t)n < sizeof(stopList) - listUsed)
                listUsed += (size_t)n;
        }

        previous = v;
    }

    double chargingTime = route.totalTimeHours - drivingTime;

    if (chargingTime < 0)
        chargingTime = 0;

    summary->routePoints = points;
    summary->routePointCount = pointCount;
    summary->pathLength = pointCount;

    summary->chargeStops = stops;
    summary->chargeStopCount = stopCount;

    summary->distanceKm = route.totalDistanceKm;
    summary->travelTimeHours = route.totalTimeHours;
    summary->drivingTimeHours = drivingTime;
    summary->chargingTimeHours = chargingTime;

    summary->initialEnergyKWh = state.energyKWh;
    summary->energyRequiredKWh =
        route.totalDistanceKm * consumptionKWhPerKm;
    summary->remainingEnergyKWh = route.finalBatteryKWh;

    summary->batterySufficient = true;
    summary->success = true;
    summary->fallbackUsed = false;

    if (stopCount == 0) {
        snprintf(summary->message, sizeof(summary->message),
                 "Fastest route. No charging stop needed.");
    } else {
        snprintf(summary->message, sizeof(summary->message),
                 "%zu charging stop%s (%s). Charging adds %.0f min.",
                 stopCount, stopCount == 1 ? "" : "s",
                 stopList, chargingTime * 60.0);
    }

    BatteryRouteFree(&route);

    return true;
}
