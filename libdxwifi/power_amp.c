/**
 *  power_amp.c
 *  
 *  DESCRIPTION: see power_amp.h for description
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */

#include <stdbool.h>

#include <gpiod.h>

#include <libdxwifi/power_amp.h>

// PA_ENABLE is on MII1_TX_CLK which is mapped to GPIO3_9 (GPIO105)

#ifndef DXWIFI_PA_GPIO_CHIP
#define DXWIFI_PA_GPIO_CHIP 3
#endif // DXWIFI_PA_GPIO_CHIP

#ifndef DXWIFI_PA_GPIO_LINE
#define DXWIFI_PA_GPIO_LINE 9
#endif //DXWIFI_PA_GPIO_LINE


static struct power_amplifier {
    bool enabled;
    struct gpiod_chip* chip;
    struct gpiod_line* line;
} POWER_AMP = {
    .enabled = false, 
    .chip = NULL,
    .line = NULL
};


//
// See power_amp.h for description of non-static methods
//

pa_error_t enable_power_amplifier() {
    int status = 0;

    if(POWER_AMP.enabled) {
        return PA_ERROR;
    }

    POWER_AMP.chip = gpiod_chip_open_by_number(DXWIFI_PA_GPIO_CHIP);
    if(!POWER_AMP.chip) {
        return PA_OPEN_CHIP_FAIL;
    }

    POWER_AMP.line = gpiod_chip_get_line(POWER_AMP.chip, DXWIFI_PA_GPIO_LINE);
    if(!POWER_AMP.chip) {
        close_power_amplifier();
        return PA_OPEN_LINE_FAIL;
    }

    status = gpiod_line_request_output(POWER_AMP.line, "dxwifi", 0);
    if(status < 0) {
        close_power_amplifier();
        return PA_LINE_REQUEST_FAIL;
    }

    status = gpiod_line_set_value(POWER_AMP.line, 1);
    if( status < 0) {
        close_power_amplifier();
        return PA_ENABLE_FAIL;
    }

    POWER_AMP.enabled = true;
    return PA_OKAY;
}


pa_error_t close_power_amplifier() {
    pa_error_t status = PA_OKAY;
    if (POWER_AMP.enabled) {
        int status = gpiod_line_set_value(POWER_AMP.line, 0);
        if(status < 0) {
            status = PA_DISABLE_FAIL;
        }
        POWER_AMP.enabled = false;
    }
    if(POWER_AMP.line) {
        gpiod_line_release(POWER_AMP.line);
    }
    if(POWER_AMP.chip) {
        gpiod_chip_close(POWER_AMP.chip);
    }
    return status;
}


const char* pa_error_to_str(pa_error_t err) {
    switch (err)
    {
    case PA_ERROR:
        return "PA error generic. Possibly the PA was already enabled";

    case PA_OPEN_CHIP_FAIL:
        return "Failed to open /dev/gpiochip associated with the power amp";
    
    case PA_OPEN_LINE_FAIL:
        return "Failed to open PA_ENABLE line";

    case PA_LINE_REQUEST_FAIL:
        return "Failed to reserve PA_ENABLE line for output";

    case PA_ENABLE_FAIL:
        return "Failed to assert PA_ENABLE line high";

    case PA_DISABLE_FAIL:
        return "Failed to deassert PA_ENABLE";

    default:
        return "PA Unknown error";
    }
}

