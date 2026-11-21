#include "../src/battery/battery.h"

#include <stdio.h>

int main(void)
{
    BatterySpec spec;
    BatteryState state;

    if (!BatteryInit(
        &spec,
        &state,
        60.0,
        80.0,
        0.18,
        5.0
    ))
    {
        printf("Battery initialization failed.\n");
        return 1;
    }

    printf("\n========== BATTERY TEST ==========\n");

    printf("Battery capacity: %.2f kWh\n",
           spec.capacityKWh);

    printf("Current energy: %.2f kWh\n",
           state.energyKWh);

    printf("Available energy: %.2f kWh\n",
           BatteryAvailableEnergy(&spec, &state));

    printf("Remaining range: %.2f km\n",
           BatteryRemainingRange(&spec, &state));

    /* Attempt a 100 km journey */

    double distance = 100.0;

    printf("\nAttempting %.2f km journey...\n", distance);

    printf("Energy required: %.2f kWh\n",
           BatteryEnergyRequired(&spec, distance));

    if (BatteryConsume(&spec, &state, distance))
    {
        printf("Journey successful!\n");
    }
    else
    {
        printf("Journey impossible with current battery.\n");
    }

    printf("Remaining energy: %.2f kWh\n",
           state.energyKWh);

    /* Simulate charging */

    printf("\nCharging with a 30 kW charger...\n");

    double chargeAmount = 20.0;

    double chargingTime =
        BatteryChargingTime(chargeAmount, 30.0);

    BatteryCharge(&spec, &state, chargeAmount);

    printf("Energy added: %.2f kWh\n", chargeAmount);

    printf("Charging time: %.2f minutes\n", chargingTime);

    printf("Final battery: %.2f kWh\n",
           state.energyKWh);

    printf("==================================\n");

    return 0;
}