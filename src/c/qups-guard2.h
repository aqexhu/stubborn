#ifndef STUBBORN_UPS_H
#define STUBBORN_UPS_H

#include <stdint.h>
#include <stdbool.h>

// Stubborn UPS HAT driver for Raspberry Pi
// Based on qups-guard2 from aqexhu/qups-guard
// Supports Eternal, Balance, and Stamina models

typedef enum {
    STUBBORN_MODEL_ETERNAL = 0,  // Supercapacitor (formerly SC)
    STUBBORN_MODEL_BALANCE = 1,  // Hybrid Capacitor (formerly HC)
    STUBBORN_MODEL_STAMINA_LF = 2,   // LiFePO4 battery
    STUBBORN_MODEL_STAMINA_NA = 3,   // Sodium-ion battery
    STUBBORN_MODEL_STAMINA_MAX = 4   // Multichem battery
} stubborn_model_t;

// GPIO pin configuration for different DIP switch settings
typedef struct {
    const char *dip_pattern;
    unsigned int pfo_pin;  // Power Fail Output
    unsigned int lim_pin;  // Limit/Input
    unsigned int shd_pin;  // Shutdown
} stubborn_pin_config_t;

// Main UPS control functions
int stubborn_init(const char *dip_pattern);
void stubborn_cleanup(void);
int stubborn_get_power_status(void);  // 1 = OK, 0 = Fail
int stubborn_get_energy_level(void);  // 1 = High, 0 = Low
void stubborn_set_shutdown_delay(int seconds);
int stubborn_get_model(void);

#endif // STUBBORN_UPS_H