/**
 *  daemon.c
 * 
 *  DESCRIPTION: See daemon.h for description
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <unistd.h>
#include <signal.h>

#include <libdxwifi/details/daemon.h>
#include <libdxwifi/details/assert.h>
#include <libdxwifi/details/logging.h>
#include <libdxwifi/details/syslogger.h>

/**
 *  DESCRIPTION:    Attempts to read pid from pid file
 * 
 *  ARGUMENTS: 
 *      
 *      pid_file:   Path to PID File to read from
 *  
 *  RETURNS:
 *  
 *      int:        The pid in the file, or 0 on failure
 *
 */
static int read_pid_file(const char* pid_file) {

    int pid = 0;
    FILE* fp = fopen(pid_file, "r");

    if(fp) {
        ssize_t nbytes = 0;
        char* line = NULL;
        size_t len = 0;

        nbytes = getline(&line, &len, fp);

        if(nbytes > 0) {
            pid = strtol(line, NULL, 0);
        }
        if(line) {
            free(line);
        }
        fclose(fp);
    }     
    return pid;
}


/**
 *  DESCRIPTION:    Attempts to write pid to pid file
 * 
 *  ARGUMENTS: 
 *      
 *      pid_file:   Path to PID File to write to
 *  
 *      pid:        Process Identifier
 *
 *  RETURNS:
 *  
 *      int:        0 on sucess, else non-zero
 *  
 */
static int write_pid_file(const char* pid_file, int pid) {
    FILE* fp = fopen(pid_file, "w");

    if(fp) {
        fprintf(fp, "%d\n", pid);
        fflush(fp);
        fclose(fp);
        return 0;
    }
    else {
        log_error("Failed to open %s for writing: %s", pid_file, strerror(errno));
        return 1;
    }
}

// See daemon.h for non-static function descriptions

dxwifi_daemon_cmd_t str_to_daemon_cmd(const char* cmd) {
    if(strncasecmp(cmd, "start", strlen("start")) == 0) {
        return DAEMON_START;
    }
    if(strncasecmp(cmd, "stop", strlen("start")) == 0) {
        return DAEMON_STOP;
    }
    return DAEMON_UNKNOWN_CMD;
}


int daemon_run(const char* pid_file, dxwifi_daemon_cmd_t cmd) {
    switch (cmd)
    {
    case DAEMON_START:
        return start_daemon(pid_file);

    case DAEMON_STOP:
        return stop_daemon(pid_file);
    default:
        return -1;
    }
}


int start_daemon(const char* pid_file) {
    debug_assert(pid_file);

    int pid = read_pid_file(pid_file);

    if(pid > 0) {
        log_fatal("PID File %s already exists with PID %d. Daemon already running?", pid_file, pid);
        exit(EXIT_FAILURE);
    }

    log_info("Daemonizing process %d...", getpid());

    if(daemon(0, 0) < 0) { // Parent process exits on success
        log_fatal("Failed to daemonize process: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Child process continues on
    set_logger(DXWIFI_LOG_ALL_MODULES, syslogger);

    umask(0);

    pid = getpid();

    // Log daemons PID to the PID File
    if(write_pid_file(pid_file, pid)) { 
        log_fatal("Failed to write to PID File %s", pid_file);
        exit(EXIT_FAILURE);
    }

    log_info("Daemon successfully started! PID: %d PID File: %s", pid, pid_file);

    return 1;
}

int stop_daemon(const char* pid_file) {

    int pid = read_pid_file(pid_file);

    if(!pid) {
        log_fatal("PID File %s does not exist. Daemon not running?", pid_file);
        exit(EXIT_FAILURE);
    }

    if(pid != getpid()) {
        kill(pid, SIGTERM);

        msleep(1000, true); // Give daemon time to terminate

        if(is_alive(pid)) { // Verify daemon exited
            log_warning("Failed to terminate %d. Sending kill signal", pid);
            kill(pid, SIGKILL);
        }
    }

    if(remove(pid_file) < 0) {
        log_error("Failed to remove PID file %s: %s", pid_file, strerror(errno));
        exit(EXIT_FAILURE);
    }

    log_info("Stopped process %d and removed PID file: %s", pid, pid_file);

    exit(EXIT_SUCCESS);
}
