#include "battery.h"

#include <math.h>
#include <stddef.h>


/* ---------- INITIALIZATION ---------- */

bool BatteryInit(
    BatterySpec *spec,
    BatteryState *state,
    double capacityKWh,
    double stateOfCharge,
    double consumptionKWhPerKm,
    double reserveKWh
)
{
    if (spec == NULL || state == NULL)
        return false;

    if (!isfinite(capacityKWh) ||
        !isfinite(stateOfCharge) ||
        !isfinite(consumptionKWhPerKm) ||
        !isfinite(reserveKWh))
        return false;

    if (capacityKWh <= 0 ||
        stateOfCharge < 0 || stateOfCharge > 100 ||
        consumptionKWhPerKm <= 0 ||
        reserveKWh < 0 ||
        reserveKWh >= capacityKWh)
        return false;

    spec->capacityKWh = capacityKWh;
    spec->stateOfCharge = stateOfCharge;
    spec->consumptionKWhPerKm = consumptionKWhPerKm;
    spec->reserveKWh = reserveKWh;

    state->energyKWh =
        capacityKWh * stateOfCharge / 100.0;

    return true;
}


/* ---------- AVAILABLE ENERGY ---------- */

double BatteryAvailableEnergy(
    const BatterySpec *spec,
    const BatteryState *state
)
{
    if (spec == NULL || state == NULL)
        return 0.0;

    double available =
        state->energyKWh - spec->reserveKWh;

    return available > 0 ? available : 0.0;
}


/* ---------- REMAINING RANGE ---------- */

double BatteryRemainingRange(
    const BatterySpec *spec,
    const BatteryState *state
)
{
    if (spec == NULL || state == NULL ||
        spec->consumptionKWhPerKm <= 0)
        return 0.0;

    return BatteryAvailableEnergy(spec, state) /
           spec->consumptionKWhPerKm;
}


/* ---------- ENERGY REQUIREMENT ---------- */

double BatteryEnergyRequired(
    const BatterySpec *spec,
    double distanceKm
)
{
    if (spec == NULL ||
        !isfinite(distanceKm) ||
        distanceKm < 0)
        return -1.0;

    return distanceKm * spec->consumptionKWhPerKm;
}


/* ---------- TRAVEL FEASIBILITY ---------- */

bool BatteryCanTravel(
    const BatterySpec *spec,
    const BatteryState *state,
    double distanceKm
)
{
    if (spec == NULL || state == NULL)
        return false;

    double required =
        BatteryEnergyRequired(spec, distanceKm);

    if (required < 0)
        return false;

    return required <= BatteryAvailableEnergy(spec, state);
}


/* ---------- ENERGY CONSUMPTION ---------- */

bool BatteryConsume(
    const BatterySpec *spec,
    BatteryState *state,
    double distanceKm
)
{
    if (!BatteryCanTravel(spec, state, distanceKm))
        return false;

    double required =
        BatteryEnergyRequired(spec, distanceKm);

    state->energyKWh -= required;

    return true;
}


/* ---------- RECHARGING ---------- */

bool BatteryCharge(
    const BatterySpec *spec,
    BatteryState *state,
    double energyKWh
)
{
    if (spec == NULL || state == NULL ||
        !isfinite(energyKWh) ||
        energyKWh <= 0)
        return false;

    if (!isfinite(state->energyKWh) ||
        state->energyKWh < 0 ||
        state->energyKWh > spec->capacityKWh)
        return false;

    if (energyKWh > spec->capacityKWh - state->energyKWh)
        energyKWh = spec->capacityKWh - state->energyKWh;

    state->energyKWh += energyKWh;

    return true;
}


/* ---------- CHARGING TIME ---------- */

double BatteryChargingTime(
    double energyKWh,
    double chargerPowerKW
)
{
    if (!isfinite(energyKWh) ||
        !isfinite(chargerPowerKW) ||
        energyKWh < 0 ||
        chargerPowerKW <= 0)
        return -1.0;

    return (energyKWh / chargerPowerKW) * 60.0;
}