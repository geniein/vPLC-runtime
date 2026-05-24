#include "core/PlcMemory.hpp"
#include "core/PlcLoader.hpp"
#include "core/PlcScheduler.hpp"
#include "modbus/ModbusServer.hpp"
#include "s7/S7Server.hpp"
#include "mc/McServer.hpp"
#include "xgt/XgtServer.hpp"
#include "tui/PlcTui.hpp"
#include "mqtt/MqttPublisher.hpp"
#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>

std::atomic<bool> should_quit(false);

void signalHandler(int signum) {
    std::cout << "\n[vPlc] Signal (" << signum << ") received. Shutting down gracefully..." << std::endl;
    should_quit = true;
}

int main(int argc, char* argv[]) {
    // Register signal handler for clean shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // 1. Initialize PLC Memory
    PlcMemory plc_memory;

    // 2. Initialize Dynamic Logic Loader
    PlcLoader plc_loader(plc_memory);
    
    // Determine which library to load, MQTT parameters, protocols filtering & port offset
    std::string lib_path = "./libmock_logic.dylib";
    std::string mqtt_broker = "";
    std::string protocols_str = "modbus,s7,mc,xgt";
    uint16_t port_offset = 0;
    bool manual_mode = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--mqtt" || arg == "-m") {
            if (i + 1 < argc) {
                mqtt_broker = argv[++i];
            } else {
                std::cerr << "[vPlc Error] --mqtt or -m option requires broker IP address." << std::endl;
                std::cerr << "Usage: ./vPlc --mqtt [IP]" << std::endl;
                return 1;
            }
        } else if (arg == "--protocols" || arg == "-p") {
            if (i + 1 < argc) {
                protocols_str = argv[++i];
            } else {
                std::cerr << "[vPlc Error] --protocols or -p option requires protocol names." << std::endl;
                std::cerr << "Usage: ./vPlc --protocols modbus,s7,mc,xgt" << std::endl;
                return 1;
            }
        } else if (arg == "--port-offset" || arg == "-o") {
            if (i + 1 < argc) {
                port_offset = static_cast<uint16_t>(std::stoi(argv[++i]));
            } else {
                std::cerr << "[vPlc Error] --port-offset or -o option requires an offset value." << std::endl;
                std::cerr << "Usage: ./vPlc --port-offset 10" << std::endl;
                return 1;
            }
        } else if (arg == "--manual" || arg == "-manual") {
            manual_mode = true;
        } else if (arg == "--help" || arg == "-h" || arg == "help") {
            std::cout << "\033[1;36m================================================================================\033[0m\n";
            std::cout << "\033[1;37m                 VIRTUAL PLC (vPLC) - COMMAND LINE INTERFACE                    \033[0m\n";
            std::cout << "\033[1;36m================================================================================\033[0m\n";
            std::cout << "Usage:\n";
            std::cout << "  ./vPlc [options/path]\n\n";
            std::cout << "Options:\n";
            std::cout << "  tank, mock, --tank, --mock          Run the Water Tank Level Control Simulator (Default)\n";
            std::cout << "  assembly, car, --assembly, --car    Run the Automotive Assembly Line Simulator\n";
            std::cout << "  -m, --mqtt [broker_ip]              Enable MQTT client and publish telemetry to specified broker\n";
            std::cout << "  -p, --protocols [modbus,s7,mc,xgt]  Select which servers to start (Default: all)\n";
            std::cout << "  -o, --port-offset [offset]          Add an offset to all default ports to avoid conflicts\n";
            std::cout << "  --manual, -manual                   Start the PLC in MANUAL mode (Default: AUTO)\n";
            std::cout << "  help, -h, --help                    Show this help message\n\n";
            std::cout << "Custom Logic Path:\n";
            std::cout << "  [path_to_dylib]                     Load any external dynamic link library (e.g. ./libmy_logic.dylib)\n";
            std::cout << "\033[1;36m================================================================================\033[0m\n";
            return 0;
        } else if (arg == "tank" || arg == "mock" || arg == "--tank" || arg == "--mock") {
            lib_path = "./libmock_logic.dylib";
        } else if (arg == "assembly" || arg == "car" || arg == "--assembly" || arg == "--car") {
            lib_path = "./libassembly_logic.dylib";
        } else {
            // Treat as direct file path
            lib_path = arg;
        }
    }

    // Parse protocols filtering
    bool enable_modbus = false;
    bool enable_s7 = false;
    bool enable_mc = false;
    bool enable_xgt = false;

    if (protocols_str != "none") {
        std::stringstream ss(protocols_str);
        std::string token;
        while (std::getline(ss, token, ',')) {
            if (token == "modbus") enable_modbus = true;
            else if (token == "s7") enable_s7 = true;
            else if (token == "mc") enable_mc = true;
            else if (token == "xgt") enable_xgt = true;
        }
    }

    if (!plc_loader.loadLibrary(lib_path)) {
        std::cerr << "[vPlc] Failed to load PLC logic library from: " << lib_path << std::endl;
        std::cerr << "[vPlc] Please make sure the shared library is compiled." << std::endl;
        return 1;
    }

    // 3. Initialize Cyclic Scheduler (20ms target cycle time)
    double target_cycle_ms = 20.0;
    PlcScheduler plc_scheduler(plc_loader, target_cycle_ms);

    // 4. Initialize Modbus TCP Server (Port 5020 + Offset)
    uint16_t port = 5020 + port_offset;
    ModbusServer modbus_server(plc_memory, "0.0.0.0", port);

    // 4b. Initialize Siemens S7 Server (Port 1020 + Offset)
    uint16_t s7_port = 1020 + port_offset;
    S7Server s7_server(plc_memory, "0.0.0.0", s7_port);

    // 4c. Initialize Mitsubishi MC Server (Port 5011 + Offset)
    uint16_t mc_port = 5011 + port_offset;
    McServer mc_server(plc_memory, "0.0.0.0", mc_port);

    // 4d. Initialize LS Electric XGT Server (Port 2004 + Offset)
    uint16_t xgt_port = 2004 + port_offset;
    XgtServer xgt_server(plc_memory, "0.0.0.0", xgt_port);

    // Set initial test values to verify memory loading
    plc_memory.writeDiscreteInput(0, !manual_mode);
    plc_memory.writeInputRegister(0, 0); // Start position at 0 mm instead of 150 mm

    // 5. Start Servers (Conditional based on parsed protocol switches)
    if (enable_modbus) {
        if (!modbus_server.start()) {
            std::cerr << "[vPlc] Failed to start Modbus TCP Server." << std::endl;
            return 1;
        }
    }

    if (enable_s7) {
        if (!s7_server.start()) {
            std::cerr << "[vPlc] Failed to start Siemens S7 Server." << std::endl;
            if (enable_modbus) modbus_server.stop();
            return 1;
        }
    }

    if (enable_mc) {
        if (!mc_server.start()) {
            std::cerr << "[vPlc] Failed to start Mitsubishi MC Server." << std::endl;
            if (enable_s7) s7_server.stop();
            if (enable_modbus) modbus_server.stop();
            return 1;
        }
    }

    if (enable_xgt) {
        if (!xgt_server.start()) {
            std::cerr << "[vPlc] Failed to start LS Electric XGT Server." << std::endl;
            if (enable_mc) mc_server.stop();
            if (enable_s7) s7_server.stop();
            if (enable_modbus) modbus_server.stop();
            return 1;
        }
    }

    if (!plc_scheduler.start()) {
        std::cerr << "[vPlc] Failed to start PLC Cyclic Scheduler." << std::endl;
        if (enable_xgt) xgt_server.stop();
        if (enable_mc) mc_server.stop();
        if (enable_s7) s7_server.stop();
        if (enable_modbus) modbus_server.stop();
        return 1;
    }

    // 4e. Start MQTT Telemetry Publisher (Conditional on parameter presence)
    std::unique_ptr<MqttPublisher> mqtt_publisher;
    if (!mqtt_broker.empty()) {
        mqtt_publisher = std::make_unique<MqttPublisher>(plc_memory, plc_loader, mqtt_broker);
        if (!mqtt_publisher->start()) {
            std::cerr << "[vPlc Warning] MQTT Publisher failed to start. Continuing without MQTT..." << std::endl;
        }
    }

    // 6. Start Terminal User Interface (TUI) Dashboard
    PlcTui plc_tui(plc_memory, plc_scheduler, modbus_server, s7_server, mc_server, xgt_server);
    if (!plc_tui.start()) {
        std::cerr << "[vPlc] Failed to start TUI Dashboard." << std::endl;
        plc_scheduler.stop();
        xgt_server.stop();
        mc_server.stop();
        s7_server.stop();
        modbus_server.stop();
        return 1;
    }

    // 7. Main thread idle wait loop
    while (!should_quit) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 8. Stop TUI first to restore terminal state
    std::cout << "[vPlc] Shutting down TUI Dashboard..." << std::endl;
    plc_tui.stop();

    // 9. Stop MQTT publisher
    if (mqtt_publisher) {
        std::cout << "[vPlc] Shutting down MQTT publisher..." << std::endl;
        mqtt_publisher->stop();
    }

    // 9b. Stop scheduler and servers
    std::cout << "[vPlc] Shutting down cyclic scheduler..." << std::endl;
    plc_scheduler.stop();

    if (enable_s7) {
        std::cout << "[vPlc] Shutting down Siemens S7 server..." << std::endl;
        s7_server.stop();
    }

    if (enable_modbus) {
        std::cout << "[vPlc] Shutting down Modbus TCP server..." << std::endl;
        modbus_server.stop();
    }

    if (enable_mc) {
        std::cout << "[vPlc] Shutting down Mitsubishi MC server..." << std::endl;
        mc_server.stop();
    }

    if (enable_xgt) {
        std::cout << "[vPlc] Shutting down LS Electric XGT server..." << std::endl;
        xgt_server.stop();
    }

    // 10. Unload dynamic library
    std::cout << "[vPlc] Unloading dynamic library..." << std::endl;
    plc_loader.unloadLibrary();

    std::cout << "[vPlc] Shutdown complete." << std::endl;
    return 0;
}
