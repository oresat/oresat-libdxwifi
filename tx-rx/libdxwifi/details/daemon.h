/**
 *  daemon.c
 * 
 *  DESCRIPTION: Utilities for daemonizing a calling process
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */


#ifndef LIBDXWIFI_DAEMON_H
#define LIBDXWIFI_DAEMON_H

typedef enum {
    DAEMON_UNKNOWN_CMD = 0,
    DAEMON_START,
    DAEMON_STOP,
} dxwifi_daemon_cmd_t;


/**
 *  DESCRIPTION:    Initializes a forked daemon process.
 * 
 *  ARGUMENTS: 
 *      
 *      pid_file:   Path to PID File to write to
 *
 *  NOTES: The calling process WILL exit after the child has been forked. Only 
 *  the child process will return from the function. If the PID file currently 
 *  exists then the function will error out and exit
 *      
 */
int start_daemon(const char* pid_file);


/**
 *  DESCRIPTION:    Stops the daemon with the PID associated with the PID file
 * 
 *  ARGUMENTS: 
 *      
 *      pid_file:   Path to Daemons PID file
 *
 *  NOTES: The function calls exit. If the pid_file exists and the Daemon was 
 *  stopped then we exit with EXIT_SUCCESS. Otherwise it exits with EXIT_FAILURE
 *  
 */
int stop_daemon(const char* pid_file);


/**
 *  DESCRIPTION:    Utility function to run a daemon command
 * 
 *  ARGUMENTS: 
 *      
 *      pid_file:   Path to Daemons PID file
 *  
 *      cmd:        Command for daemon to execute
 *
 */
int daemon_run(const char* pid_file, dxwifi_daemon_cmd_t cmd);

/**
 *  DESCRIPTION:    Utility function to run a daemon command
 * 
 *  ARGUMENTS: 
 *      
 *      pid_file:   Path to Daemons PID file
 *  
 *      cmd:        Command for daemon to execute
 *
 */
dxwifi_daemon_cmd_t str_to_daemon_cmd(const char* cmd);

#endif // LIBDXWIFI_DAEMON_H


