#ifndef MODBUS_SERVER_HPP
#define MODBUS_SERVER_HPP

#include "core/PlcMemory.hpp"
#include <thread>
#include <atomic>
#include <vector>
#include <string>

class ModbusServer {
public:
    ModbusServer(PlcMemory& memory, const std::string& ip_address = "0.0.0.0", uint16_t port = 5020);
    ~ModbusServer();

    // Start/Stop server thread
    bool start();
    void stop();

    bool isRunning() const { return is_running_; }
    uint16_t getPort() const { return port_; }
    size_t getConnectedClientsCount() const;

private:
    // Core Server Loop
    void serverLoop();

    // Handling single client connection
    void handleClient(int client_fd, std::string client_ip);

    // Processing Modbus packet
    std::vector<uint8_t> processRequest(const std::vector<uint8_t>& request);

    // Exception response helper
    std::vector<uint8_t> makeExceptionResponse(uint8_t function_code, uint8_t exception_code);

    // Helper functions for reading/writing big-endian values
    static uint16_t readUint16(const uint8_t* buffer) {
        return (static_cast<uint16_t>(buffer[0]) << 8) | buffer[1];
    }

    static void writeUint16(uint8_t* buffer, uint16_t value) {
        buffer[0] = static_cast<uint8_t>((value >> 8) & 0xFF);
        buffer[1] = static_cast<uint8_t>(value & 0xFF);
    }

    PlcMemory& memory_;
    std::string ip_address_;
    uint16_t port_;

    int server_fd_;
    std::atomic<bool> is_running_;
    std::thread server_thread_;

    // Connection tracking
    mutable std::shared_mutex clients_mutex_;
    std::vector<int> client_fds_;
};

#endif // MODBUS_SERVER_HPP
