#ifndef GUI_H
#define GUI_H

#include "raylib.h"
#include "../routing/route_service.h"
#include <stdbool.h>
#include <stddef.h>

#define LOCATION_COUNT 9

#define MAP_NORTH 30.822
#define MAP_SOUTH 29.817
#define MAP_WEST  77.053
#define MAP_EAST  78.596

typedef enum {
    STATUS_INFO,
    STATUS_SUCCESS,
    STATUS_ERROR
} StatusKind;

/* Search statistics of the last query, shown in the analysis card. */
typedef struct {
    bool valid;

    double searchMs;
    size_t stateSpace;
    size_t statesSettled;
    size_t heapPushes;
    size_t graphVertices;

    bool baselineValid;
    double baselineMs;
    double baselineDistanceKm;
    size_t baselineSettled;
} AlgoStats;

typedef struct {
    char origin[64];
    char destination[64];

    char batteryCapacity[16];
    char batteryCharge[16];
    char energyConsumption[16];

    char statusMessage[160];
    StatusKind statusKind;

    int originIndex;
    int destinationIndex;
    int openDropdown;
    int activeField;

    bool calculationRequested;
    bool routeCalculated;
    bool batterySufficient;
    bool fallbackUsed;

    /* Numeric results of the last successful route. */
    double distanceKm;
    double travelTimeHours;
    double drivingTimeHours;
    double chargingTimeHours;
    double energyRequiredKWh;
    double remainingEnergyKWh;
    double capacityKWh;
    double consumptionKWhPerKm;

    AlgoStats stats;

    Vector2 *routePoints;
    size_t routePointCount;

    Vector2 *chargeStops;            /* stops used by the current route */
    size_t chargeStopCount;

    const Vector2 *stationPoints;    /* all stations (not owned by GUI) */
    size_t stationPointCount;

    double originLat;
    double originLon;
    double destinationLat;
    double destinationLon;
} PlannerGUI;

typedef struct {
    Texture2D texture;
    bool loaded;
} MapData;

void MapDataInit(MapData *map);
bool LoadMapImage(MapData *map, const char *filename);
void FreeMapData(MapData *map);

Vector2 MapCoordinate(const MapData *map, double lon, double lat);

/* Call after InitWindow(): loads fonts, which need a GL context. */
void InitializeGUI(PlannerGUI *gui);
void UpdateGUI(PlannerGUI *gui);
void DrawGUI(const PlannerGUI *gui, const MapData *map);
/* Call before CloseWindow(). */
void ShutdownGUI(PlannerGUI *gui);

void GUISetStations(PlannerGUI *gui, const Vector2 *points, size_t count);
void GUISetRoute(PlannerGUI *gui, const RouteSummary *route);
void GUIFreeRoute(PlannerGUI *gui);

#endif
