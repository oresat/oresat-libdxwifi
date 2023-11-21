#ifndef TX_H
#define TX_H

#include <stdbool.h>


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <poll.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <fnmatch.h>

#include <arpa/inet.h>
#include <linux/limits.h>

#include <dxwifi/tx/cli.h>

#include <libdxwifi/dxwifi.h>
#include <libdxwifi/transmitter.h>
#include <libdxwifi/details/utils.h>
#include <libdxwifi/details/daemon.h>
#include <libdxwifi/details/logging.h>
#include <libdxwifi/details/dirwatch.h>
#include <libdxwifi/details/syslogger.h>

//Syscalls for Memory Mapping
#include <sys/mman.h>

// Function declarations
void terminate(int signum);
void transmit(cli_args* args, dxwifi_transmitter* tx);
void tx_sigint_handler(int signum);
void watchdir_sigint_handler(int signum);
void log_tx_stats(dxwifi_tx_stats stats);
bool log_frame_stats(dxwifi_tx_frame* frame, dxwifi_tx_stats stats, void* user);
bool delay_transmission(dxwifi_tx_frame* frame, dxwifi_tx_stats stats, void* user);
bool packet_loss_sim(dxwifi_tx_frame* frame, dxwifi_tx_stats stats, void* user);
bool bit_error_rate_sim(dxwifi_tx_frame* frame, dxwifi_tx_stats stats, void* user);
bool attach_frame_number(dxwifi_tx_frame* frame, dxwifi_tx_stats stats, void* user);
dxwifi_tx_state_t setup_handlers_and_transmit(dxwifi_transmitter* tx, int fd);
dxwifi_tx_state_t transmit_files(dxwifi_transmitter* tx, char** files, size_t num_files, unsigned delay, int retransmit_count, float coderate);
void transmit_directory_contents(dxwifi_transmitter* tx, const char* filter, const char* dirname, unsigned delay, int retransmit_count, float coderate);
static void transmit_new_file(const dirwatch_event* event, void* user);
void transmit_directory(cli_args* args, dxwifi_transmitter* tx);
void transmit_test_sequence(dxwifi_transmitter* tx, int retransmit);
int main_worker(int argc, char** argv);

#endif // TX_H
