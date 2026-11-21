/*
 * Headless baseline driver (`make baseline`).
 *
 * Compiles the real src/main.c with its main() renamed (-Dmain=app_main),
 * so BuildCityGraph() and CalculateRoute() run exactly as in the app, and
 * prints a fixed set of routes. Timing values are not printed, so the
 * output is deterministic and can be diffed against
 * docs/baseline_routes.txt to prove a change did not alter routing.
 */
#include "main.c"   /* resolved through -Isrc */

#undef main

typedef struct {
    const char *from;
    const char *to;
    double charge;
} Case;

int main(void)
{
    Graph roadGraph, cityGraph;
    StationManager stations;

    GraphInit(&roadGraph);
    GraphInit(&cityGraph);
    StationManagerInit(&stations);

    if (!GraphLoadFromCSV(&roadGraph, "data/processed/vertices.csv",
                          "data/processed/roads.csv"))
        return 1;

    int traced = 0, fallback = 0;
    if (!BuildCityGraph(&cityGraph, &roadGraph, &stations,
                        &traced, &fallback))
        return 1;

    printf("@@ graph nodes=%zu edges=%zu stations=%zu traced=%d fallback=%d\n",
           cityGraph.vertexCount, cityGraph.edgeCount, stations.count,
           traced, fallback);

    const Case cases[] = {
        {"Dehradun",   "Mussoorie", 50.0},
        {"Dehradun",   "Chakrata",  50.0},
        {"Saharanpur", "New Tehri", 50.0},
        {"Dehradun",   "Chakrata",  12.0},
    };

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        RouteSummary r;
        bool ok = CalculateRoute(&cityGraph, &stations, cases[i].from,
                                 cases[i].to, 60.0, cases[i].charge, 0.35,
                                 &r);

        printf("@@ %s -> %s @ %.0f%%: ok=%d", cases[i].from, cases[i].to,
               cases[i].charge, ok);

        if (ok)
            printf(" distance=%.4f km time=%.6f h driving=%.6f h "
                   "charging=%.6f h energy=%.4f kWh left=%.4f kWh "
                   "stops=%zu points=%zu",
                   r.distanceKm, r.travelTimeHours, r.drivingTimeHours,
                   r.chargingTimeHours, r.energyRequiredKWh,
                   r.remainingEnergyKWh, r.chargeStopCount,
                   r.routePointCount);

        printf(" settled=%zu pushes=%zu baseline_km=%.4f msg=\"%s\"\n",
               r.statesSettled, r.heapPushes, r.baselineDistanceKm,
               r.message);

        RouteSummaryFree(&r);
    }

    StationManagerFree(&stations);
    GraphFree(&cityGraph);
    GraphFree(&roadGraph);
    return 0;
}
