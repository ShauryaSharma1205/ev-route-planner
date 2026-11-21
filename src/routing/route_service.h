#ifndef ROUTE_SERVICE_H
#define ROUTE_SERVICE_H

#include "graph/graph.h"
#include "stations/stations.h"
#include "raylib.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct RouteSummary {
    bool success;
    bool batterySufficient;

    double distanceKm;
    double travelTimeHours;

    double energyRequiredKWh;
    double remainingEnergyKWh;

    size_t routePointCount;
    Vector2 *routePoints;          /* x = longitude, y = latitude */

    Vector2 *chargeStops;          /* charging stops used, x = lon, y = lat */
    size_t chargeStopCount;

    double drivingTimeHours;
    double chargingTimeHours;

    char message[160];

    /* ---- algorithm statistics (for the analysis panel) ---- */
    bool statsValid;

    double searchMs;             /* BatteryAwareDijkstra wall time */
    size_t stateSpace;           /* vertices x battery levels */
    size_t statesSettled;
    size_t heapPushes;
    size_t graphVertices;

    /* Baseline: plain distance-only Dijkstra, ignoring the battery. */
    bool baselineValid;
    double baselineMs;
    double baselineDistanceKm;
    size_t baselineSettled;
} RouteSummary;

/* Battery-aware routing: fastest route (driving + charging time) that never
   drops below a 10% battery reserve, charging at stations when needed. */
bool CalculateRoute(
    const Graph *graph,
    const StationManager *stations,
    const char *origin,
    const char *destination,
    double batteryCapacityKWh,
    double batteryChargePercent,
    double consumptionKWhPerKm,
    RouteSummary *summary
);

void RouteSummaryFree(RouteSummary *summary);

#endif