#include <cassert>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <iostream>

extern "C" {
    #include "tx.h"
}

void dxwifi_transmitter_init_default(dxwifi_transmitter& tx) {
    tx.transmit_timeout       = -1;
    tx.redundant_ctrl_frames  = 0;
    tx.enable_pa              = false;
    tx.rtap_flags             = 0x00;
    tx.rtap_rate_mbps         = 1;
    tx.rtap_tx_flags          = IEEE80211_RADIOTAP_F_TX_NOACK;

    uint8_t default_address[] = DXWIFI_DFLT_SENDER_ADDR;
    memcpy(tx.address, default_address, sizeof(default_address));

    tx.fctl.protocol_version  = IEEE80211_PROTOCOL_VERSION;
    tx.fctl.type              = IEEE80211_FTYPE_DATA;
    tx.fctl.stype.data        = IEEE80211_STYPE_DATA;
    tx.fctl.to_ds             = false;
    tx.fctl.from_ds           = true;
    tx.fctl.more_frag         = false;
    tx.fctl.retry             = false;
    tx.fctl.power_mgmt        = false;
    tx.fctl.more_data         = true;
    tx.fctl.wep               = false;
    tx.fctl.order             = false;

    tx.__activated = false;
    tx.__handle    = NULL;

#if defined(DXWIFI_TESTS)
    tx.savefile = NULL;
    tx.dumper   = NULL;
#endif
}

void cli_init_default(cli_args& args) {
    args.tx_mode = TX_STREAM_MODE;
    args.daemon = DAEMON_UNKNOWN_CMD;
    args.pid_file = TX_DEFAULT_PID_FILE;
    args.verbosity = DXWIFI_LOG_INFO;
    args.quiet = false;
    args.use_syslog = false;
    args.file_count = 0,
    args.file_filter = "*",
    args.retransmit_count = 0;
    args.transmit_current_files = false;
    args.listen_for_new_files = true;
    args.dirwatch_timeout = -1;
    args.tx_delay = 0;
    args.error_rate = 0;
    args.packet_loss = 0;
    dxwifi_transmitter_init_default(args.tx);
    args.coderate = 0.667;
}

void init_transmitter_wrapper(dxwifi_transmitter* tx, const std::string& device_name) {
    init_transmitter(tx, device_name.c_str());
}

int main_wrapper(pybind11::list args) {
    int argc = pybind11::len(args);
    char** argv = new char*[argc];
    for (int i = 0; i < argc; ++i) {
        std::string arg = pybind11::cast<std::string>(args[i]);
        argv[i] = strdup(arg.c_str());
    }

    int result = main_worker(argc, argv);

    for (int i = 0; i < argc; ++i) {
        free(argv[i]);
    }
    delete[] argv;
    return result;
}

std::vector<unsigned char> get_address(const dxwifi_transmitter& tx) {
    return std::vector<unsigned char>(tx.address, tx.address + sizeof(tx.address));
}

void set_address(dxwifi_transmitter& tx, const std::vector<unsigned char>& addr) {
    std::copy(addr.begin(), addr.end(), tx.address);
}

const char* get_device(const cli_args& args) {
    return args.device;
}

void set_device(cli_args& args, const std::string& device) {
    args.device = strdup(device.c_str());
}

const char* get_file_filter(const cli_args& args) {
    return args.file_filter;
}

void set_file_filter(cli_args& args, const std::string& file_filter) {
    free(const_cast<char*>(args.file_filter));
    args.file_filter = strdup(file_filter.c_str());
}

std::vector<std::string> get_files(const cli_args& args) {
    std::vector<std::string> files;
    for (int i = 0; i < args.file_count; ++i) {
        files.push_back(std::string(args.files[i]));
    }
    return files;
}

void set_files(cli_args& args, const std::vector<std::string>& files) {
    for (size_t i = 0; i < files.size() && i < TX_CLI_FILE_MAX; ++i) {
        free(args.files[i]);
        args.files[i] = strdup(files[i].c_str());
    }
    args.file_count = files.size();
}

void transmit_wrapper(cli_args& args, dxwifi_transmitter& tx) {
    transmit(&args, &tx);
}

PYBIND11_MODULE(tx_module, m) {
    m.doc() = "controls transmission";

    m.def("main_wrapper", &main_wrapper, "main main");

    m.def("transmit", &transmit_wrapper, "Determine the transmission mode and transmit files");

    m.def("terminate", &terminate, "SIGTERM handler for daemonized process. Ensures transmitter is closed.");

    m.def("init_transmitter", &init_transmitter_wrapper, "See transmitter.h for description of non-static functions");

    m.def("default", &cli_init_default, "inits cli_args with default values");

    m.def("close_transmitter", &close_transmitter, "closes transmitter");

    pybind11::class_<dxwifi_transmitter>(m, "DxWifiTransmitter")
        .def(pybind11::init<>())
        .def_readwrite("transmit_timeout", &dxwifi_transmitter::transmit_timeout)
        .def_readwrite("redundant_ctrl_frames", &dxwifi_transmitter::redundant_ctrl_frames)
        .def_readwrite("enable_pa", &dxwifi_transmitter::enable_pa)
        .def("get_address", &get_address)
        .def("set_address", &set_address)
        .def_readwrite("rtap_flags", &dxwifi_transmitter::rtap_flags)
        .def_readwrite("rtap_rate_mbps", &dxwifi_transmitter::rtap_rate_mbps)
        .def_readwrite("rtap_tx_flags", &dxwifi_transmitter::rtap_tx_flags);

    pybind11::enum_<tx_mode_t>(m, "TxMode")
        .value("TX_TEST_MODE", TX_TEST_MODE)
        .value("TX_FILE_MODE", TX_FILE_MODE)
        .value("TX_STREAM_MODE", TX_STREAM_MODE)
        .value("TX_DIRECTORY_MODE", TX_DIRECTORY_MODE)
        .export_values();

    pybind11::enum_<dxwifi_daemon_cmd_t>(m, "DaemonCommand")
        .value("UNKNOWN_CMD", DAEMON_UNKNOWN_CMD)
        .value("START", DAEMON_START)
        .value("STOP", DAEMON_STOP)
        .export_values();

    pybind11::class_<cli_args>(m, "CliArgs")
        .def(pybind11::init<>())
        .def_readwrite("tx_mode", &cli_args::tx_mode)
        .def_readwrite("daemon", &cli_args::daemon)
        .def_readwrite("pid_file", &cli_args::pid_file)
        .def("get_files", &get_files)
        .def("set_files", &set_files)
        .def("get_file_filter", &get_file_filter)
        .def("set_file_filter", &set_file_filter)
        .def_readwrite("file_filter", &cli_args::file_filter)
        .def_readwrite("retransmit_count", &cli_args::retransmit_count)
        .def_readwrite("transmit_current_files", &cli_args::transmit_current_files)
        .def_readwrite("listen_for_new_files", &cli_args::listen_for_new_files)
        .def_readwrite("dirwatch_timeout", &cli_args::dirwatch_timeout)
        .def_readwrite("verbosity", &cli_args::verbosity)
        .def_readwrite("quiet", &cli_args::quiet)
        .def_readwrite("use_syslog", &cli_args::use_syslog)
        .def_readwrite("tx_delay", &cli_args::tx_delay)
        .def_readwrite("file_delay", &cli_args::file_delay)
        .def_readwrite("device", &cli_args::device)
        .def("get_device", &get_device)
        .def("set_device", &set_device)
        .def_readwrite("packet_loss", &cli_args::packet_loss)
        .def_readwrite("error_rate", &cli_args::error_rate)
        .def_readwrite("tx", &cli_args::tx)
        .def_readwrite("coderate", &cli_args::coderate);
}
