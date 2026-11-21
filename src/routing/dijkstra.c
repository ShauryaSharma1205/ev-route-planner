#include "dijkstra.h"

#include <stdlib.h>
#include <float.h>

/* ---------- MIN-HEAP ---------- */

typedef struct
{
    int vertex;
    double distance;

} HeapEntry;


typedef struct
{
    HeapEntry *entries;
    size_t size;
    size_t capacity;

} MinHeap;


static bool HeapInit(MinHeap *heap)
{
    heap->capacity = 16;
    heap->size = 0;

    heap->entries = malloc(
        heap->capacity * sizeof(HeapEntry)
    );

    return heap->entries != NULL;
}


static void HeapFree(MinHeap *heap)
{
    free(heap->entries);

    heap->entries = NULL;
    heap->size = 0;
    heap->capacity = 0;
}


static void HeapSwap(HeapEntry *a, HeapEntry *b)
{
    HeapEntry temp = *a;
    *a = *b;
    *b = temp;
}


static bool HeapPush(MinHeap *heap, HeapEntry entry)
{
    if (heap->size == heap->capacity)
    {
        size_t newCapacity = heap->capacity * 2;

        HeapEntry *newEntries = realloc(
            heap->entries,
            newCapacity * sizeof(HeapEntry)
        );

        if (newEntries == NULL)
            return false;

        heap->entries = newEntries;
        heap->capacity = newCapacity;
    }

    size_t index = heap->size++;

    heap->entries[index] = entry;

    /* Restore min-heap property */

    while (index > 0)
    {
        size_t parent = (index - 1) / 2;

        if (heap->entries[parent].distance <=
            heap->entries[index].distance)
            break;

        HeapSwap(
            &heap->entries[parent],
            &heap->entries[index]
        );

        index = parent;
    }

    return true;
}


static bool HeapPop(MinHeap *heap, HeapEntry *result)
{
    if (heap->size == 0)
        return false;

    *result = heap->entries[0];

    heap->size--;

    if (heap->size == 0)
        return true;

    heap->entries[0] = heap->entries[heap->size];

    size_t index = 0;

    /* Restore min-heap property */

    while (true)
    {
        size_t left = 2 * index + 1;
        size_t right = 2 * index + 2;
        size_t smallest = index;

        if (left < heap->size &&
            heap->entries[left].distance <
            heap->entries[smallest].distance)
        {
            smallest = left;
        }

        if (right < heap->size &&
            heap->entries[right].distance <
            heap->entries[smallest].distance)
        {
            smallest = right;
        }

        if (smallest == index)
            break;

        HeapSwap(
            &heap->entries[index],
            &heap->entries[smallest]
        );

        index = smallest;
    }

    return true;
}


/* ---------- DIJKSTRA ---------- */

bool Dijkstra(
    const Graph *graph,
    int source,
    PathResult *result
)
{
    if (graph == NULL || result == NULL)
        return false;

    result->distance = NULL;
    result->previous = NULL;
    result->vertexCount = 0;
    result->source = -1;
    result->settledCount = 0;
    result->pushCount = 0;

    if (graph->nodes == NULL ||
        graph->vertexCount == 0 ||
        source < 0 ||
        (size_t)source >= graph->vertexCount)
    {
        return false;
    }

    size_t count = graph->vertexCount;

    result->distance = malloc(count * sizeof(double));
    result->previous = malloc(count * sizeof(int));

    if (result->distance == NULL ||
        result->previous == NULL)
    {
        DijkstraFree(result);
        return false;
    }

    result->vertexCount = count;
    result->source = source;

    for (size_t i = 0; i < count; i++)
    {
        result->distance[i] = DBL_MAX;
        result->previous[i] = -1;
    }

    MinHeap heap;

    if (!HeapInit(&heap))
    {
        DijkstraFree(result);
        return false;
    }

    result->distance[source] = 0.0;

    if (!HeapPush(&heap, (HeapEntry){source, 0.0}))
    {
        HeapFree(&heap);
        DijkstraFree(result);
        return false;
    }

    result->pushCount = 1;

    HeapEntry current;

    while (HeapPop(&heap, &current))
    {
        int u = current.vertex;

        /* Ignore outdated heap entries */

        if (current.distance > result->distance[u])
            continue;

        result->settledCount++;

        GraphEdge *edge = graph->nodes[u].edges;

        while (edge != NULL)
        {
            int v = edge->destination;

            double newDistance =
                result->distance[u] + edge->distanceKm;

            if (newDistance < result->distance[v])
            {
                result->distance[v] = newDistance;
                result->previous[v] = u;

                HeapEntry next =
                {
                    v,
                    newDistance
                };

                if (!HeapPush(&heap, next))
                {
                    HeapFree(&heap);
                    DijkstraFree(result);
                    return false;
                }

                result->pushCount++;
            }

            edge = edge->next;
        }
    }

    HeapFree(&heap);

    return true;
}


/* ---------- PATH RECONSTRUCTION ---------- */

size_t DijkstraBuildPath(
    const PathResult *result,
    int destination,
    int *path,
    size_t pathCapacity
)
{
    if (result == NULL ||
        result->distance == NULL ||
        result->previous == NULL ||
        path == NULL)
    {
        return 0;
    }

    if (destination < 0 ||
        (size_t)destination >= result->vertexCount)
    {
        return 0;
    }

    if (result->distance[destination] == DBL_MAX)
        return 0;

    size_t count = 0;
    int current = destination;

    /* Count vertices in the route */

    while (current != result->source)
    {
        if (current < 0 ||
            (size_t)current >= result->vertexCount ||
            count >= result->vertexCount)
        {
            return 0;
        }

        count++;
        current = result->previous[current];
    }

    count++;

    if (count > pathCapacity)
        return 0;

    /* Fill path backwards */

    current = destination;

    for (size_t i = count; i > 0; i--)
    {
        path[i - 1] = current;

        if (i > 1)
            current = result->previous[current];
    }

    return count;
}


/* ---------- MEMORY CLEANUP ---------- */

void DijkstraFree(PathResult *result)
{
    if (result == NULL)
        return;

    free(result->distance);
    free(result->previous);

    result->distance = NULL;
    result->previous = NULL;

    result->vertexCount = 0;
    result->source = -1;
}