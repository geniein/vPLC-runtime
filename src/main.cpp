#include "core/PlcMemory.hpp"
#include "core/PlcLoader.hpp"
#include "core/PlcScheduler.hpp"
#include "modbus/ModbusServer.hpp"
#include "s7/S7Server.hpp"
#include "mc/McServer.hpp"
#include "xgt/XgtServer.hpp"
#include "tui/PlcTui.hpp"
#include <iostream>
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
    
    // Determine which library to load (default to mock library)
    std::string lib_path = "./libmock_logic.dylib";
    if (argc > 1) {
        lib_path = argv[1];
    }

    if (!plc_loader.loadLibrary(lib_path)) {
        std::cerr << "[vPlc] Failed to load PLC logic library from: " << lib_path << std::endl;
        std::cerr << "[vPlc] Please make sure the shared library is compiled." << std::endl;
        return 1;
    }

    // 3. Initialize Cyclic Scheduler (20ms target cycle time)
    double target_cycle_ms = 20.0;
    PlcScheduler plc_scheduler(plc_loader, target_cycle_ms);

    // 4. Initialize Modbus TCP Server (Port 5020)
    uint16_t port = 5020;
    ModbusServer modbus_server(plc_memory, "0.0.0.0", port);

    // 4b. Initialize Siemens S7 Server (Port 1020)
    uint16_t s7_port = 1020;
    S7Server s7_server(plc_memory, "0.0.0.0", s7_port);

    // 4c. Initialize Mitsubishi MC Server (Port 5011)
    uint16_t mc_port = 5011;
    McServer mc_server(plc_memory, "0.0.0.0", mc_port);

    // 4d. Initialize LS Electric XGT Server (Port 2004)
    uint16_t xgt_port = 2004;
    XgtServer xgt_server(plc_memory, "0.0.0.0", xgt_port);

    // Set initial test values to verify memory loading
    plc_memory.writeDiscreteInput(0, false);
    plc_memory.writeInputRegister(0, 150); // Will double to MW1 (300)

    // 5. Start Servers
    if (!modbus_server.start()) {
        std::cerr << "[vPlc] Failed to start Modbus TCP Server." << std::endl;
        return 1;
    }

    if (!s7_server.start()) {
        std::cerr << "[vPlc] Failed to start Siemens S7 Server." << std::endl;
        modbus_server.stop();
        return 1;
    }

    if (!mc_server.start()) {
        std::cerr << "[vPlc] Failed to start Mitsubishi MC Server." << std::endl;
        s7_server.stop();
        modbus_server.stop();
        return 1;
    }

    if (!xgt_server.start()) {
        std::cerr << "[vPlc] Failed to start LS Electric XGT Server." << std::endl;
        mc_server.stop();
        s7_server.stop();
        modbus_server.stop();
        return 1;
    }

    if (!plc_scheduler.start()) {
        std::cerr << "[vPlc] Failed to start PLC Cyclic Scheduler." << std::endl;
        xgt_server.stop();
        mc_server.stop();
        s7_server.stop();
        modbus_server.stop();
        return 1;
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

    // 9. Stop scheduler and servers
    std::cout << "[vPlc] Shutting down cyclic scheduler..." << std::endl;
    plc_scheduler.stop();

    std::cout << "[vPlc] Shutting down Siemens S7 server..." << std::endl;
    s7_server.stop();

    std::cout << "[vPlc] Shutting down Modbus TCP server..." << std::endl;
    modbus_server.stop();

    std::cout << "[vPlc] Shutting down Mitsubishi MC server..." << std::endl;
    mc_server.stop();

    std::cout << "[vPlc] Shutting down LS Electric XGT server..." << std::endl;
    xgt_server.stop();

    // 10. Unload dynamic library
    std::cout << "[vPlc] Unloading dynamic library..." << std::endl;
    plc_loader.unloadLibrary();

    std::cout << "[vPlc] Shutdown complete." << std::endl;
    return 0;
}
