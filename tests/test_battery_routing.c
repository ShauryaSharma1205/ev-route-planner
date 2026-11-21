#include "../src/graph/graph.h"
#include "../src/battery/battery.h"
#include "../src/stations/stations.h"
#include "../src/routing/battery_routing.h"

#include <stdio.h>

int main(void)
{
    Graph graph;
    BatterySpec spec;
    BatteryState state;
    StationManager stations;
    BatteryRoute route = {0};

    GraphInit(&graph);
    StationManagerInit(&stations);

    /* Create locations. */

    int dehradun = GraphAddVertex(
        &graph, "Dehradun", 30.3165, 78.0322
    );

    int mussoorie = GraphAddVertex(
        &graph, "Mussoorie", 30.4598, 78.0644
    );

    int chakrata = GraphAddVertex(
        &graph, "Chakrata", 30.7037, 77.8628
    );

    if (dehradun < 0 || mussoorie < 0 || chakrata < 0)
        goto failure;

    /* Create roads. */

    if (!GraphAddRoad(
        &graph, dehradun, mussoorie,
        35.0, "Mussoorie Road", true
    ))
        goto failure;

    if (!GraphAddRoad(
    &graph, mussoorie, chakrata,
    55.0, "Hill Road", true
    ))
        goto failure;

    /* Initialize a small EV battery. */

    if (!BatteryInit(
        &spec,
        &state,
        20.0,
        100.0,
        0.30,
        2.0
    ))
        goto failure;

    /* Register a charging station in Mussoorie. */

    if (StationAdd(
        &stations,
        "Mussoorie EV Hub",
        mussoorie,
        30.0,
        STATION_OPERATIONAL
    ) < 0)
        goto failure;

    printf("\n========== BATTERY ROUTING TEST ==========\n");

    printf("Starting energy: %.2f kWh\n",
           state.energyKWh);

    printf("Battery capacity: %.2f kWh\n",
           spec.capacityKWh);

    printf("Reserve: %.2f kWh\n",
           spec.reserveKWh);

    printf("\nCalculating route...\n");

    if (!BatteryAwareDijkstra(
        &graph,
        &spec,
        &state,
        &stations,
        dehradun,
        chakrata,
        &route
    ))
    {
        printf("No feasible route found.\n");
        goto failure;
    }

    printf("\nRoute found!\n");

    printf("Total distance: %.2f km\n",
           route.totalDistanceKm);

    printf("Total journey time: %.2f hours\n", route.totalTimeHours);

    printf("Final battery: %.2f kWh\n",
           route.finalBatteryKWh);

    printf("\nRoute details:\n");

    for (size_t i = 0; i < route.stepCount; i++)
    {
        RouteStep *step = &route.steps[i];

        printf(
            "%s | Battery: %.1f kWh",
            graph.nodes[step->vertexId].vertex.name,
            step->batteryAfterKWh
        );

        if (step->isChargingStop)
        {
            ChargingStation *station =
                StationGet(
                    &stations,
                    step->chargedAtStationId
                );

            printf(
                " | CHARGED AT %s",
                station != NULL ? station->name : "UNKNOWN"
            );
        }

        printf("\n");
    }

    printf("==========================================\n");

    BatteryRouteFree(&route);
    StationManagerFree(&stations);
    GraphFree(&graph);

    return 0;


failure:

    printf("Test failed.\n");

    BatteryRouteFree(&route);
    StationManagerFree(&stations);
    GraphFree(&graph);

    return 1;
}