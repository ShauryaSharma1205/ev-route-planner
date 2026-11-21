#include "graph.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


/* ---------- GRAPH INITIALIZATION ---------- */

void GraphInit(Graph *graph)
{
    graph->nodes = NULL;

    graph->vertexCount = 0;
    graph->capacity = 0;
    graph->edgeCount = 0;
}


/* ---------- GRAPH DESTRUCTION ---------- */

void GraphFree(Graph *graph)
{
    if (graph == NULL)
        return;

    for (size_t i = 0; i < graph->vertexCount; i++)
    {
        GraphEdge *current = graph->nodes[i].edges;

        while (current != NULL)
        {
            GraphEdge *next = current->next;

            free(current);

            current = next;
        }
    }

    free(graph->nodes);

    GraphInit(graph);
}


/* ---------- ADD VERTEX ---------- */

int GraphAddVertex(
    Graph *graph,
    const char *name,
    double latitude,
    double longitude
)
{
    if (graph == NULL || name == NULL)
        return -1;

    if (!isfinite(latitude) || !isfinite(longitude))
        return -1;

    if (latitude < -90 || latitude > 90 ||
        longitude < -180 || longitude > 180)
        return -1;


    /* Expand storage when necessary */

    if (graph->vertexCount == graph->capacity)
    {
        size_t newCapacity =
            graph->capacity == 0 ? 16 : graph->capacity * 2;

        GraphNode *newNodes = realloc(
            graph->nodes,
            newCapacity * sizeof(GraphNode)
        );

        if (newNodes == NULL)
            return -1;

        graph->nodes = newNodes;
        graph->capacity = newCapacity;
    }


    int id = (int)graph->vertexCount;

    GraphNode *node = &graph->nodes[graph->vertexCount];

    node->vertex.id = id;

    snprintf(
        node->vertex.name,
        sizeof(node->vertex.name),
        "%s",
        name
    );

    node->vertex.latitude = latitude;
    node->vertex.longitude = longitude;

    node->edges = NULL;

    graph->vertexCount++;

    return id;
}


/* ---------- ADD DIRECTED EDGE ---------- */

static bool AddDirectedEdge(
    Graph *graph,
    int from,
    int to,
    double distanceKm,
    double speedKmh,
    const char *roadName
)
{
    GraphEdge *edge = malloc(sizeof(GraphEdge));

    if (edge == NULL)
        return false;

    edge->destination = to;
    edge->distanceKm = distanceKm;
    edge->speedKmh = speedKmh;

    edge->travelTimeHours = distanceKm / speedKmh;

    snprintf(
        edge->roadName,
        sizeof(edge->roadName),
        "%s",
        roadName
    );

    edge->next = graph->nodes[from].edges;
    graph->nodes[from].edges = edge;

    graph->edgeCount++;

    return true;
}


/* ---------- ADD ROAD ---------- */

bool GraphAddRoadWithSpeed(
    Graph *graph,
    int from,
    int to,
    double distanceKm,
    double speedKmh,
    const char *roadName,
    bool bidirectional
)
{
    if (graph == NULL || roadName == NULL)
        return false;

    if (from < 0 || to < 0 ||
        from == to ||
        (size_t)from >= graph->vertexCount ||
        (size_t)to >= graph->vertexCount)
        return false;

    if (!isfinite(distanceKm) ||
        !isfinite(speedKmh) ||
        distanceKm <= 0 ||
        speedKmh <= 0 ||
        speedKmh > 250)
        return false;

    if (!AddDirectedEdge(
        graph, from, to, distanceKm, speedKmh, roadName
    ))
        return false;

    if (bidirectional)
    {
        if (!AddDirectedEdge(
            graph, to, from, distanceKm, speedKmh, roadName
        ))
        {
            GraphEdge *edge = graph->nodes[from].edges;

            graph->nodes[from].edges = edge->next;

            free(edge);

            graph->edgeCount--;

            return false;
        }
    }

    return true;
}


/* Backward-compatible road creation.
   Default estimated speed: 40 km/h. */

bool GraphAddRoad(
    Graph *graph,
    int from,
    int to,
    double distanceKm,
    const char *roadName,
    bool bidirectional
)
{
    return GraphAddRoadWithSpeed(
        graph,
        from,
        to,
        distanceKm,
        40.0,
        roadName,
        bidirectional
    );
}


/* ---------- PRINT GRAPH SUMMARY ---------- */

void GraphPrintSummary(const Graph *graph)
{
    if (graph == NULL)
        return;

    printf("\n========== GRAPH SUMMARY ==========\n");

    printf("Vertices: %zu\n", graph->vertexCount);

    printf("Directed edges: %zu\n", graph->edgeCount);

    printf("===================================\n");
}


/* ---------- PRINT GRAPH ---------- */

void GraphPrint(const Graph *graph)
{
    if (graph == NULL)
        return;

    for (size_t i = 0; i < graph->vertexCount; i++)
    {
        const GraphNode *node = &graph->nodes[i];

        printf(
            "\n%d: %s\n",
            node->vertex.id,
            node->vertex.name
        );

        const GraphEdge *edge = node->edges;

        while (edge != NULL)
        {
            printf(
                "  -> %d | %.2f km | %.1f km/h | %.2f hours | %s\n",
                edge->destination,
                edge->distanceKm,
                edge->speedKmh,
                edge->travelTimeHours,
                edge->roadName
            );

            edge = edge->next;
        }
    }
}