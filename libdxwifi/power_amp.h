/**
 *  power_amp.h
 *  
 *  DESCRIPTION: Hardware abstraction layer for DxWiFi onboard power amplifier
 * 
 *  NOTES:  This API is NOT thread-safe as the function calls will modify a 
 *  static structure. Unless someone wants to add a mutex to the power amp, do
 *  not use this library in a multi-threaded environment. 
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */

#ifndef LIBDXWIFI_POWER_AMP_H
#define LIBDXWIFI_POWER_AMP_H

typedef enum {
    PA_OKAY = 0,
    PA_ERROR,
    PA_OPEN_CHIP_FAIL,
    PA_OPEN_LINE_FAIL,
    PA_LINE_REQUEST_FAIL,
    PA_ENABLE_FAIL,
    PA_DISABLE_FAIL,
} pa_error_t;


/**
 *  DESCRIPTION:    Enables the DxWiFi board power amplifier by asserting the 
 *                  PA_ENABLE pin. 
 *  
 *  RETURNS:
 *      pa_error_t: Status of initialization, if the PA failed to initialize
 *                  this function will handle cleaning up resources. 
 * 
 *  NOTES: WARNING! Do not call this function without an antenna or dummy load 
 *  on the output of the card! Runing the PA without an antenna or dummy load 
 *  could destroy the PA.
 * 
 */
pa_error_t enable_power_amplifier();


/**
 *  DESCRIPTION:    Disables the power amplifier and tearsdown any associated
 *                  resources.
 *  
 *  RETURNS:
 *      pa_error_t: PA_DISABLE_FAIL if the power amp failed to be deasserted. 
 *                  else, PA_OKAY.
 * 
 */
pa_error_t close_power_amplifier();

/**
 *  DESCRIPTION:    Convert pa_error_t into a descriptive string
 *  
 *  ARGUMENTS:
 *      const char*: string describing the error
 * 
 */
const char* pa_error_to_str(pa_error_t err);

#endif // LIBDXWIFI_POWER_AMP_H
