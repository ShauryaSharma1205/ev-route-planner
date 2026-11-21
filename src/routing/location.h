#ifndef LOCATION_H
#define LOCATION_H

#include "graph/graph.h"
#include <stdbool.h>

typedef struct {
    char name[64];
    double latitude;
    double longitude;
} KnownLocation;

bool GetLocationCoordinates(const char *name, double *lat, double *lon);

int FindNearestVertex(
    const Graph *graph,
    double latitude,
    double longitude,
    double *distanceKm
);

#endif