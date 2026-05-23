#include "core/PlcMemory.hpp"
#include "core/PlcLoader.hpp"
#include "core/PlcScheduler.hpp"
#include "modbus/ModbusServer.hpp"
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

    // 4. Initialize Modbus TCP Server (Port 5020 to avoid root privilege issues)
    uint16_t port = 5020;
    ModbusServer modbus_server(plc_memory, "0.0.0.0", port);

    // Set initial test values to verify memory loading
    plc_memory.writeDiscreteInput(0, false);
    plc_memory.writeInputRegister(0, 150); // Will double to MW1 (300)

    // 5. Start Servers
    if (!modbus_server.start()) {
        std::cerr << "[vPlc] Failed to start Modbus TCP Server." << std::endl;
        return 1;
    }

    if (!plc_scheduler.start()) {
        std::cerr << "[vPlc] Failed to start PLC Cyclic Scheduler." << std::endl;
        modbus_server.stop();
        return 1;
    }

    // 6. Start Terminal User Interface (TUI) Dashboard
    PlcTui plc_tui(plc_memory, plc_scheduler, modbus_server);
    if (!plc_tui.start()) {
        std::cerr << "[vPlc] Failed to start TUI Dashboard." << std::endl;
        plc_scheduler.stop();
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

    // 9. Stop scheduler and server
    std::cout << "[vPlc] Shutting down cyclic scheduler..." << std::endl;
    plc_scheduler.stop();

    std::cout << "[vPlc] Shutting down Modbus TCP server..." << std::endl;
    modbus_server.stop();

    // 10. Unload dynamic library
    std::cout << "[vPlc] Unloading dynamic library..." << std::endl;
    plc_loader.unloadLibrary();

    std::cout << "[vPlc] Shutdown complete." << std::endl;
    return 0;
}
