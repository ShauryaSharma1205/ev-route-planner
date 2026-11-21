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

    /* Search statistics (filled even when no route is found). */
    size_t stateSpace;       /* vertices x battery levels */
    size_t statesSettled;    /* states popped and expanded */
    size_t heapPushes;       /* priority-queue insertions */

} BatteryRoute;


/* Find the fastest feasible route (driving + charging time) over
   (vertex, battery level) states, never dropping below the reserve and
   charging to full only at operational stations. */
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