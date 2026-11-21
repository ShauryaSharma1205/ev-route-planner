#ifndef GRAPH_H
#define GRAPH_H

#include <stddef.h>
#include <stdbool.h>

#define MAX_VERTEX_NAME 64
#define MAX_ROAD_NAME 96

typedef struct
{
    int id;
    char name[MAX_VERTEX_NAME];

    double latitude;
    double longitude;

} Vertex;


typedef struct GraphEdge
{
    int destination;

    double distanceKm;

    double speedKmh;

    double travelTimeHours;

    char roadName[MAX_ROAD_NAME];

    struct GraphEdge *next;

} GraphEdge;

typedef struct
{
    Vertex vertex;
    GraphEdge *edges;

} GraphNode;


typedef struct
{
    GraphNode *nodes;

    size_t vertexCount;
    size_t capacity;

    size_t edgeCount;

} Graph;


/* Graph management */

void GraphInit(Graph *graph);

void GraphFree(Graph *graph);


/* Vertex operations */

int GraphAddVertex(
    Graph *graph,
    const char *name,
    double latitude,
    double longitude
);


/* Road operations */

bool GraphAddRoad(
    Graph *graph,
    int from,
    int to,
    double distanceKm,
    const char *roadName,
    bool bidirectional
);

bool GraphAddRoadWithSpeed(
    Graph *graph,
    int from,
    int to,
    double distanceKm,
    double speedKmh,
    const char *roadName,
    bool bidirectional
);

/* Utility functions */

void GraphPrintSummary(const Graph *graph);

void GraphPrint(const Graph *graph);

#endif