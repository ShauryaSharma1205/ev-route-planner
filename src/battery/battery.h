#ifndef BATTERY_H
#define BATTERY_H

#include <stdbool.h>

typedef struct
{
    double capacityKWh;
    double stateOfCharge;
    double consumptionKWhPerKm;
    double reserveKWh;

} BatterySpec;


typedef struct
{
    double energyKWh;

} BatteryState;


/* Initialize battery specifications and current state. */
bool BatteryInit(
    BatterySpec *spec,
    BatteryState *state,
    double capacityKWh,
    double stateOfCharge,
    double consumptionKWhPerKm,
    double reserveKWh
);


/* Calculate currently available energy. */
double BatteryAvailableEnergy(
    const BatterySpec *spec,
    const BatteryState *state
);


/* Calculate the maximum theoretical driving range. */
double BatteryRemainingRange(
    const BatterySpec *spec,
    const BatteryState *state
);


/* Calculate energy required for a road segment. */
double BatteryEnergyRequired(
    const BatterySpec *spec,
    double distanceKm
);


/* Check whether a road segment is traversable. */
bool BatteryCanTravel(
    const BatterySpec *spec,
    const BatteryState *state,
    double distanceKm
);


/* Consume energy after travelling. */
bool BatteryConsume(
    const BatterySpec *spec,
    BatteryState *state,
    double distanceKm
);


/* Recharge battery. */
bool BatteryCharge(
    const BatterySpec *spec,
    BatteryState *state,
    double energyKWh
);


/* Estimate charging time in minutes. */
double BatteryChargingTime(
    double energyKWh,
    double chargerPowerKW
);

#endif