#ifndef S7_SERVER_HPP
#define S7_SERVER_HPP

#include "core/PlcMemory.hpp"
#include <snap7.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>

class S7Server {
public:
    S7Server(PlcMemory& memory, const std::string& ip_address = "0.0.0.0", uint16_t port = 1020);
    ~S7Server();

    // Start/Stop S7 Server
    bool start();
    void stop();

    // Synchronize data BEFORE PLC logic execution (Reads S7 inputs/DB1 writes and copies to memory_)
    void syncBeforeCycle();

    // Synchronize data AFTER PLC logic execution (Writes memory_ outputs/DB1 changes to S7 buffers)
    void syncAfterCycle();

    bool isRunning() const { return is_running_; }
    uint16_t getPort() const { return port_; }
    int getClientsCount() const;

private:
    void syncLoop();

    PlcMemory& memory_;
    std::string ip_address_;
    uint16_t port_;

    S7Object s7_server_;
    std::atomic<bool> is_running_;
    std::thread sync_thread_;

    // S7 Data Area Buffers
    std::vector<uint8_t> pe_buffer_;  // %I - Inputs (256 bytes = 2048 bits)
    std::vector<uint8_t> pa_buffer_;  // %Q - Outputs (256 bytes = 2048 bits)
    std::vector<uint8_t> db1_buffer_; // DB1 - Data Block (2048 bytes = 1024 registers)
    std::vector<uint8_t> db2_buffer_; // DB2 - Data Block (2048 bytes = 1024 registers)

    // Shadow buffers for detecting S7 Client write actions
    std::vector<uint8_t> db1_shadow_buffer_;
    std::vector<uint8_t> db2_shadow_buffer_;

    // Helper functions for big-endian conversion
    static uint16_t swapBytes(uint16_t val) {
        return (val >> 8) | (val << 8);
    }
};

#endif // S7_SERVER_HPP
