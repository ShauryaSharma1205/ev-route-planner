#include "../src/stations/stations.h"

#include <stdio.h>

int main(void)
{
    StationManager manager;

    StationManagerInit(&manager);

    /* Register sample stations */

    int station1 = StationAdd(
        &manager,
        "Dehradun EV Hub",
        0,
        60.0,
        STATION_OPERATIONAL
    );

    int station2 = StationAdd(
        &manager,
        "Mussoorie Charging Point",
        1,
        30.0,
        STATION_OPERATIONAL
    );

    int station3 = StationAdd(
        &manager,
        "Chakrata Charging Point",
        3,
        22.0,
        STATION_UNKNOWN
    );

    if (station1 < 0 || station2 < 0 || station3 < 0)
    {
        printf("Station registration failed.\n");

        StationManagerFree(&manager);
        return 1;
    }

    printf("\nStation registration successful.\n");

    StationPrintAll(&manager);

    /* Find station at vertex 1 */

    ChargingStation *found =
        StationFindAtVertex(&manager, 1);

    if (found != NULL)
    {
        printf(
            "\nFound station: %s\n",
            found->name
        );
    }

    /* Simulate station failure */

    printf("\nSimulating station failure...\n");

    StationSetStatus(
        &manager,
        station2,
        STATION_UNAVAILABLE
    );

    /* Check station availability */

    for (int i = 0; i < 3; i++)
    {
        ChargingStation *station =
            StationGet(&manager, i);

        printf(
            "%s: %s\n",
            station->name,
            StationIsUsable(station) ? "Usable" : "Not usable"
        );
    }

    StationPrintAll(&manager);

    StationManagerFree(&manager);

    return 0;
}