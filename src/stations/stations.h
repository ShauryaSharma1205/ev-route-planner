#ifndef STATIONS_H
#define STATIONS_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_STATION_NAME 100

typedef enum
{
    STATION_OPERATIONAL,
    STATION_UNAVAILABLE,
    STATION_UNKNOWN

} StationStatus;


typedef struct
{
    int id;
    int vertexId;

    char name[MAX_STATION_NAME];

    double chargerPowerKW;

    StationStatus status;

} ChargingStation;


typedef struct
{
    ChargingStation *stations;

    size_t count;
    size_t capacity;

} StationManager;


/* Initialize and free manager */

void StationManagerInit(StationManager *manager);

void StationManagerFree(StationManager *manager);


/* Add a charging station */

int StationAdd(
    StationManager *manager,
    const char *name,
    int vertexId,
    double chargerPowerKW,
    StationStatus status
);


/* Retrieve a station */

ChargingStation *StationGet(
    StationManager *manager,
    int stationId
);


/* Find a station at a graph vertex */

ChargingStation *StationFindAtVertex(
    StationManager *manager,
    int vertexId
);


/* Update station status */

bool StationSetStatus(
    StationManager *manager,
    int stationId,
    StationStatus status
);


/* Check whether a station can currently be used */

bool StationIsUsable(const ChargingStation *station);


/* Display station information */

void StationPrintAll(const StationManager *manager);

#endif