#ifndef MC_SERVER_HPP
#define MC_SERVER_HPP

#include "core/PlcMemory.hpp"
#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <shared_mutex>

class McServer {
public:
    McServer(PlcMemory& memory, const std::string& ip_address = "0.0.0.0", uint16_t port = 5011);
    ~McServer();

    // Start/Stop server thread
    bool start();
    void stop();

    bool isRunning() const { return is_running_; }
    uint16_t getPort() const { return port_; }
    size_t getClientsCount() const;

private:
    // Core Server Loop
    void serverLoop();

    // Handling single client connection
    void handleClient(int client_fd, std::string client_ip);

    // Processing MELSEC MC 3E Frame packet
    std::vector<uint8_t> processRequest(const std::vector<uint8_t>& request);

    // Error response helper
    std::vector<uint8_t> makeErrorResponse(uint16_t end_code);

    // Helper functions for little-endian values (MC Protocol standard)
    static uint16_t readUint16LE(const uint8_t* buffer) {
        return static_cast<uint16_t>(buffer[0]) | (static_cast<uint16_t>(buffer[1]) << 8);
    }

    static void writeUint16LE(uint8_t* buffer, uint16_t value) {
        buffer[0] = static_cast<uint8_t>(value & 0xFF);
        buffer[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    }

    static uint32_t readUint32LE(const uint8_t* buffer) {
        return static_cast<uint32_t>(buffer[0]) |
               (static_cast<uint32_t>(buffer[1]) << 8) |
               (static_cast<uint32_t>(buffer[2]) << 16) |
               (static_cast<uint32_t>(buffer[3]) << 24);
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

#endif // MC_SERVER_HPP
