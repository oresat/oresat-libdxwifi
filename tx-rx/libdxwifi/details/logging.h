/**
 *  logging.h
 * 
 *  DESCRIPTION: DxWiFi Logging API Facade
 * 
 *  This logging facade supports formatted strings as well as user, module, and 
 *  compiler log levels. Logging under the compiler log level gets culled out 
 *  and won't be present in release builds. By default logging is disabled as it 
 *  has no concrete implementation. To enable logging you will need to provide a 
 *  logger function that fullfills the dxwifi_logger interface and bridge it to
 *  your own logging library.
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */
