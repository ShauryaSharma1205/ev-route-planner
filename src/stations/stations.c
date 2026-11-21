#include "stations.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


/* ---------- INITIALIZATION ---------- */

void StationManagerInit(StationManager *manager)
{
    if (manager == NULL)
        return;

    manager->stations = NULL;
    manager->count = 0;
    manager->capacity = 0;
}


/* ---------- MEMORY CLEANUP ---------- */

void StationManagerFree(StationManager *manager)
{
    if (manager == NULL)
        return;

    free(manager->stations);

    StationManagerInit(manager);
}


/* ---------- ADD STATION ---------- */

int StationAdd(
    StationManager *manager,
    const char *name,
    int vertexId,
    double chargerPowerKW,
    StationStatus status
)
{
    if (manager == NULL || name == NULL || name[0] == '\0')
        return -1;

    if (vertexId < 0 ||
        !isfinite(chargerPowerKW) ||
        chargerPowerKW <= 0)
        return -1;

    if (status < STATION_OPERATIONAL ||
        status > STATION_UNKNOWN)
        return -1;

    if (strlen(name) >= MAX_STATION_NAME)
        return -1;


    /* Expand storage when necessary */

    if (manager->count == manager->capacity)
    {
        size_t newCapacity =
            manager->capacity == 0 ? 8 : manager->capacity * 2;

        ChargingStation *newStations = realloc(
            manager->stations,
            newCapacity * sizeof(ChargingStation)
        );

        if (newStations == NULL)
            return -1;

        manager->stations = newStations;
        manager->capacity = newCapacity;
    }


    int id = (int)manager->count;

    ChargingStation *station =
        &manager->stations[manager->count];

    station->id = id;
    station->vertexId = vertexId;

    snprintf(
        station->name,
        sizeof(station->name),
        "%s",
        name
    );

    station->chargerPowerKW = chargerPowerKW;
    station->status = status;

    manager->count++;

    return id;
}


/* ---------- RETRIEVE STATION ---------- */

ChargingStation *StationGet(
    StationManager *manager,
    int stationId
)
{
    if (manager == NULL || stationId < 0 ||
        (size_t)stationId >= manager->count)
        return NULL;

    return &manager->stations[stationId];
}


/* ---------- FIND STATION BY VERTEX ---------- */

ChargingStation *StationFindAtVertex(
    StationManager *manager,
    int vertexId
)
{
    if (manager == NULL || vertexId < 0)
        return NULL;

    for (size_t i = 0; i < manager->count; i++)
    {
        if (manager->stations[i].vertexId == vertexId)
            return &manager->stations[i];
    }

    return NULL;
}


/* ---------- UPDATE STATUS ---------- */

bool StationSetStatus(
    StationManager *manager,
    int stationId,
    StationStatus status
)
{
    ChargingStation *station =
        StationGet(manager, stationId);

    if (station == NULL)
        return false;

    if (status < STATION_OPERATIONAL ||
        status > STATION_UNKNOWN)
        return false;

    station->status = status;

    return true;
}


/* ---------- CHECK AVAILABILITY ---------- */

bool StationIsUsable(const ChargingStation *station)
{
    if (station == NULL)
        return false;

    return station->status == STATION_OPERATIONAL;
}


/* ---------- PRINT STATIONS ---------- */

void StationPrintAll(const StationManager *manager)
{
    if (manager == NULL)
        return;

    printf("\n========== CHARGING STATIONS ==========\n");

    if (manager->count == 0)
    {
        printf("No charging stations registered.\n");
    }

    for (size_t i = 0; i < manager->count; i++)
    {
        const ChargingStation *station =
            &manager->stations[i];

        const char *status;

        switch (station->status)
        {
            case STATION_OPERATIONAL:
                status = "Operational";
                break;

            case STATION_UNAVAILABLE:
                status = "Unavailable";
                break;

            default:
                status = "Unknown";
                break;
        }

        printf(
            "\nID: %d\n"
            "Name: %s\n"
            "Graph vertex: %d\n"
            "Charger power: %.2f kW\n"
            "Status: %s\n",
            station->id,
            station->name,
            station->vertexId,
            station->chargerPowerKW,
            status
        );
    }

    printf("\n=======================================\n");
}