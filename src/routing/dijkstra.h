#ifndef DIJKSTRA_H
#define DIJKSTRA_H

#include "../graph/graph.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct
{
    double *distance;
    int *previous;

    size_t vertexCount;
    int source;

    /* Search statistics. */
    size_t settledCount;     /* vertices popped and expanded */
    size_t pushCount;        /* priority-queue insertions */

} PathResult;


/* Calculate shortest paths from a source vertex. */
bool Dijkstra(
    const Graph *graph,
    int source,
    PathResult *result
);


/* Reconstruct a route from source to destination.
   Returns the number of vertices in the path.
   Returns 0 if no path exists or the buffer is too small. */
size_t DijkstraBuildPath(
    const PathResult *result,
    int destination,
    int *path,
    size_t pathCapacity
);


/* Release result memory. */
void DijkstraFree(PathResult *result);

#endif