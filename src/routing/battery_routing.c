#include "battery_routing.h"

#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <stdint.h>

#define ENERGY_SCALE 100.0   /* 0.01 kWh resolution */

typedef struct
{
    size_t state;
    double cost;

} HeapEntry;


typedef struct
{
    HeapEntry *entries;
    size_t size;
    size_t capacity;

} MinHeap;


typedef enum
{
    ACTION_START,
    ACTION_TRAVEL,
    ACTION_CHARGE

} Action;


/* ---------- MIN HEAP ---------- */

static bool HeapInit(MinHeap *heap)
{
    heap->entries = malloc(16 * sizeof(HeapEntry));

    if (heap->entries == NULL)
        return false;

    heap->size = 0;
    heap->capacity = 16;

    return true;
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
        if (heap->capacity > SIZE_MAX / 2)
            return false;

        size_t newCapacity = heap->capacity * 2;

        if (newCapacity > SIZE_MAX / sizeof(HeapEntry))
            return false;

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

    while (index > 0)
    {
        size_t parent = (index - 1) / 2;

        if (heap->entries[parent].cost <=
            heap->entries[index].cost)
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

    while (true)
    {
        size_t left = index * 2 + 1;
        size_t right = index * 2 + 2;
        size_t smallest = index;

        if (left < heap->size &&
            heap->entries[left].cost <
            heap->entries[smallest].cost)
        {
            smallest = left;
        }

        if (right < heap->size &&
            heap->entries[right].cost <
            heap->entries[smallest].cost)
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


/* ---------- STATE INDEXING ---------- */

/*
   Every state represents:

   (vertex ID, remaining battery units)

   Example:
   (5, 1200) = vertex 5 with 12.00 kWh (units of 0.01 kWh).
*/

static size_t StateIndex(
    size_t vertex,
    size_t energy,
    size_t energyLevels
)
{
    return vertex * energyLevels + energy;
}


/* ---------- ROUTE CLEANUP ---------- */

void BatteryRouteFree(BatteryRoute *route)
{
    if (route == NULL)
        return;

    free(route->steps);

    route->steps = NULL;
    route->stepCount = 0;

    route->totalDistanceKm = 0.0;
    route->totalTimeHours = 0.0;
    route->finalBatteryKWh = 0.0;

    route->stateSpace = 0;
    route->statesSettled = 0;
    route->heapPushes = 0;
}


/* ---------- MAIN ALGORITHM ---------- */

/*
   Time-optimized battery-aware Dijkstra.

   Optimization objective:
       Driving time + charging time

   Constraints:
       - Battery capacity
       - Minimum battery reserve
       - Charging station availability
       - Road energy requirements

   Battery resolution: 0.01 kWh.
*/

bool BatteryAwareDijkstra(
    const Graph *graph,
    const BatterySpec *spec,
    const BatteryState *initialState,
    const StationManager *stations,
    int source,
    int destination,
    BatteryRoute *route
)
{
    if (route == NULL)
        return false;

    BatteryRouteFree(route);

    if (graph == NULL || spec == NULL ||
        initialState == NULL || stations == NULL)
        return false;

    if (graph->nodes == NULL ||
        graph->vertexCount == 0 ||
        source < 0 || destination < 0 ||
        (size_t)source >= graph->vertexCount ||
        (size_t)destination >= graph->vertexCount)
        return false;

    if (stations->count > 0 && stations->stations == NULL)
        return false;

    if (!isfinite(spec->capacityKWh) ||
        !isfinite(spec->consumptionKWhPerKm) ||
        !isfinite(spec->reserveKWh) ||
        !isfinite(initialState->energyKWh) ||
        spec->capacityKWh <= 0 ||
        spec->consumptionKWhPerKm <= 0 ||
        spec->reserveKWh < 0 ||
        spec->reserveKWh >= spec->capacityKWh ||
        initialState->energyKWh < spec->reserveKWh ||
        initialState->energyKWh > spec->capacityKWh)
        return false;

    if (spec->capacityKWh >
        (double)(SIZE_MAX - 1) / ENERGY_SCALE)
        return false;


    /* Convert battery capacity into 0.01 kWh units. */

    size_t maxEnergy = (size_t)floor(
        spec->capacityKWh * ENERGY_SCALE + 1e-9
    );

    size_t initialEnergy = (size_t)floor(
        initialState->energyKWh * ENERGY_SCALE + 1e-9
    );

    size_t reserveEnergy = (size_t)ceil(
        spec->reserveKWh * ENERGY_SCALE - 1e-9
    );

    if (initialEnergy < reserveEnergy ||
        initialEnergy > maxEnergy)
        return false;

    size_t energyLevels = maxEnergy + 1;
    size_t vertexCount = graph->vertexCount;

    if (vertexCount > SIZE_MAX / energyLevels)
        return false;

    size_t stateCount = vertexCount * energyLevels;

    if (stateCount > SIZE_MAX / sizeof(double) ||
        stateCount > SIZE_MAX / sizeof(size_t) ||
        stateCount > SIZE_MAX / sizeof(int) ||
        stateCount > SIZE_MAX / sizeof(Action))
        return false;

    route->stateSpace = stateCount;


    /* ---------- ALLOCATE STATE ARRAYS ---------- */

    double *cost = malloc(
        stateCount * sizeof(double)
    );

    double *totalDistance = calloc(
        stateCount,
        sizeof(double)
    );

    size_t *previous = malloc(
        stateCount * sizeof(size_t)
    );

    Action *action = malloc(
        stateCount * sizeof(Action)
    );

    int *stationUsed = malloc(
        stateCount * sizeof(int)
    );

    if (cost == NULL || totalDistance == NULL ||
        previous == NULL || action == NULL ||
        stationUsed == NULL)
    {
        free(cost);
        free(totalDistance);
        free(previous);
        free(action);
        free(stationUsed);

        return false;
    }


    /* ---------- INITIALIZE ARRAYS ---------- */

    for (size_t i = 0; i < stateCount; i++)
    {
        cost[i] = DBL_MAX;
        totalDistance[i] = 0.0;

        previous[i] = SIZE_MAX;
        action[i] = ACTION_START;
        stationUsed[i] = -1;
    }


    /* ---------- INITIALIZE HEAP ---------- */

    MinHeap heap;

    if (!HeapInit(&heap))
    {
        free(cost);
        free(totalDistance);
        free(previous);
        free(action);
        free(stationUsed);

        return false;
    }


    /* ---------- INITIALIZE SOURCE STATE ---------- */

    size_t startState = StateIndex(
        (size_t)source,
        initialEnergy,
        energyLevels
    );

    cost[startState] = 0.0;
    totalDistance[startState] = 0.0;

    if (!HeapPush(&heap, (HeapEntry){startState, 0.0}))
    {
        HeapFree(&heap);

        free(cost);
        free(totalDistance);
        free(previous);
        free(action);
        free(stationUsed);

        return false;
    }

    route->heapPushes = 1;


    /* ---------- DIJKSTRA SEARCH ---------- */

    size_t finalState = SIZE_MAX;

    HeapEntry current;

    while (HeapPop(&heap, &current))
    {
        size_t currentState = current.state;

        if (current.cost > cost[currentState])
            continue;

        route->statesSettled++;

        size_t u = currentState / energyLevels;
        size_t energy = currentState % energyLevels;


        /* Destination reached. */

        if (u == (size_t)destination)
        {
            finalState = currentState;
            break;
        }


        /* ---------- TRAVEL TRANSITIONS ---------- */

        const GraphEdge *edge = graph->nodes[u].edges;

        while (edge != NULL)
        {
            int v = edge->destination;

            if (v < 0 || (size_t)v >= vertexCount ||
                !isfinite(edge->distanceKm) ||
                edge->distanceKm < 0 ||
                !isfinite(edge->travelTimeHours) ||
                edge->travelTimeHours <= 0)
            {
                edge = edge->next;
                continue;
            }

            double requiredEnergy =
                edge->distanceKm *
                spec->consumptionKWhPerKm;

            if (!isfinite(requiredEnergy) ||
                requiredEnergy < 0 ||
                requiredEnergy >
                    (double)SIZE_MAX / ENERGY_SCALE)
            {
                edge = edge->next;
                continue;
            }

            size_t requiredUnits = (size_t)ceil(
                requiredEnergy * ENERGY_SCALE - 1e-9
            );

            if (requiredUnits <= energy &&
                energy - requiredUnits >= reserveEnergy)
            {
                size_t remaining = energy - requiredUnits;

                size_t nextState = StateIndex(
                    (size_t)v,
                    remaining,
                    energyLevels
                );

                double newCost =
                    current.cost + edge->travelTimeHours;

                if (newCost < cost[nextState])
                {
                    cost[nextState] = newCost;

                    totalDistance[nextState] =
                        totalDistance[currentState] +
                        edge->distanceKm;

                    previous[nextState] = currentState;
                    action[nextState] = ACTION_TRAVEL;
                    stationUsed[nextState] = -1;

                    if (!HeapPush(
                        &heap,
                        (HeapEntry){nextState, newCost}
                    ))
                    {
                        finalState = SIZE_MAX;
                        goto cleanup;
                    }

                    route->heapPushes++;
                }
            }

            edge = edge->next;
        }


        /* ---------- CHARGING TRANSITIONS ---------- */

        if (energy < maxEnergy)
        {
            for (size_t i = 0; i < stations->count; i++)
            {
                const ChargingStation *station =
                    &stations->stations[i];

                if (station->vertexId != (int)u)
                    continue;

                if (!StationIsUsable(station))
                    continue;

                size_t chargedState = StateIndex(
                    u,
                    maxEnergy,
                    energyLevels
                );

                double energyToAdd =
                    (maxEnergy - energy) / ENERGY_SCALE;

                double chargingHours =
                    BatteryChargingTime(
                        energyToAdd,
                        station->chargerPowerKW
                    ) / 60.0;

                if (!isfinite(chargingHours) ||
                    chargingHours < 0)
                    continue;

                double newCost =
                    current.cost + chargingHours;

                if (newCost < cost[chargedState])
                {
                    cost[chargedState] = newCost;

                    totalDistance[chargedState] =
                        totalDistance[currentState];

                    previous[chargedState] = currentState;
                    action[chargedState] = ACTION_CHARGE;
                    stationUsed[chargedState] = station->id;

                    if (!HeapPush(
                        &heap,
                        (HeapEntry){chargedState, newCost}
                    ))
                    {
                        finalState = SIZE_MAX;
                        goto cleanup;
                    }

                    route->heapPushes++;
                }
            }
        }
    }


    /* ---------- RECONSTRUCT ROUTE ---------- */

    if (finalState == SIZE_MAX)
        goto cleanup;

    size_t stepCount = 0;
    size_t cursor = finalState;

    while (cursor != startState)
    {
        if (cursor == SIZE_MAX ||
            previous[cursor] == SIZE_MAX ||
            stepCount >= stateCount)
        {
            goto cleanup;
        }

        stepCount++;
        cursor = previous[cursor];
    }

    stepCount++;

    RouteStep *steps = malloc(
        stepCount * sizeof(RouteStep)
    );

    if (steps == NULL)
        goto cleanup;

    cursor = finalState;

    for (size_t i = stepCount; i > 0; i--)
    {
        size_t vertex = cursor / energyLevels;
        size_t energy = cursor % energyLevels;

        steps[i - 1].vertexId = (int)vertex;

        steps[i - 1].batteryAfterKWh =
            energy / ENERGY_SCALE;

        steps[i - 1].isChargingStop =
            action[cursor] == ACTION_CHARGE;

        steps[i - 1].chargedAtStationId =
            stationUsed[cursor];

        if (i > 1)
            cursor = previous[cursor];
    }


    /* ---------- SAVE ROUTE RESULTS ---------- */

    route->steps = steps;
    route->stepCount = stepCount;

    route->totalDistanceKm =
        totalDistance[finalState];

    route->totalTimeHours =
        cost[finalState];

    route->finalBatteryKWh =
        (finalState % energyLevels) / ENERGY_SCALE;

    finalState = SIZE_MAX;


cleanup:

    HeapFree(&heap);

    free(cost);
    free(totalDistance);
    free(previous);
    free(action);
    free(stationUsed);

    return route->steps != NULL;
}