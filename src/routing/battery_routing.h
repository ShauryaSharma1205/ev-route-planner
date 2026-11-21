#ifndef BATTERY_ROUTING_H
#define BATTERY_ROUTING_H

#include "../graph/graph.h"
#include "../battery/battery.h"
#include "../stations/stations.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct
{
    int vertexId;

    double batteryAfterKWh;

    int chargedAtStationId;

    bool isChargingStop;

} RouteStep;


typedef struct
{
    RouteStep *steps;

    size_t stepCount;

    double totalDistanceKm;
    double totalTimeHours;

    double finalBatteryKWh;

} BatteryRoute;


/* Find the shortest feasible route considering battery energy
   and operational charging stations. */
bool BatteryAwareDijkstra(
    const Graph *graph,
    const BatterySpec *spec,
    const BatteryState *initialState,
    const StationManager *stations,
    int source,
    int destination,
    BatteryRoute *route
);


/* Release route memory. */
void BatteryRouteFree(BatteryRoute *route);

#endif