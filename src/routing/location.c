#include "location.h"

#include <math.h>
#include <string.h>
#include <ctype.h>

#define EARTH_RADIUS_KM 6371.0
#define PI 3.14159265358979323846

static const KnownLocation locations[] = {
    {"dehradun", 30.3165, 78.0322},
    {"mussoorie", 30.4598, 78.0644},
    {"chakrata", 30.7037, 77.8625},
    {"rishikesh", 30.0869, 78.2676},
    {"haridwar", 29.9457, 78.1642},
    {"new tehri", 30.3753, 78.4804}
};

static double ToRadians(double degrees)
{
    return degrees * PI / 180.0;
}

static double Haversine(
    double lat1, double lon1,
    double lat2, double lon2)
{
    double dLat = ToRadians(lat2 - lat1);
    double dLon = ToRadians(lon2 - lon1);

    lat1 = ToRadians(lat1);
    lat2 = ToRadians(lat2);

    double a =
        sin(dLat / 2.0) * sin(dLat / 2.0) +
        cos(lat1) * cos(lat2) *
        sin(dLon / 2.0) * sin(dLon / 2.0);

    return EARTH_RADIUS_KM *
        2.0 * atan2(sqrt(a), sqrt(1.0 - a));
}

static void Normalize(
    const char *input,
    char *output,
    size_t size)
{
    size_t i = 0;

    while (input[i] && i < size - 1) {
        output[i] = (char)tolower((unsigned char)input[i]);
        i++;
    }

    output[i] = '\0';

    while (i > 0 && output[i - 1] == ' ') {
        output[--i] = '\0';
    }
}

bool GetLocationCoordinates(
    const char *name,
    double *lat,
    double *lon)
{
    if (!name || !lat || !lon)
        return false;

    char normalized[64];

    Normalize(name, normalized, sizeof(normalized));

    size_t count = sizeof(locations) / sizeof(locations[0]);

    for (size_t i = 0; i < count; i++) {
        if (strcmp(normalized, locations[i].name) == 0) {
            *lat = locations[i].latitude;
            *lon = locations[i].longitude;
            return true;
        }
    }

    return false;
}

int FindNearestVertex(
    const Graph *graph,
    double latitude,
    double longitude,
    double *distanceKm)
{
    if (!graph || !graph->nodes || graph->vertexCount == 0)
        return -1;

    double bestDistance = INFINITY;
    int bestVertex = -1;

    for (size_t i = 0; i < graph->vertexCount; i++) {

        const Vertex *vertex = &graph->nodes[i].vertex;

        double distance = Haversine(
            latitude,
            longitude,
            vertex->latitude,
            vertex->longitude
        );

        if (distance < bestDistance) {
            bestDistance = distance;
            bestVertex = (int)i;
        }
    }

    if (distanceKm)
        *distanceKm = bestDistance;

    return bestVertex;
}