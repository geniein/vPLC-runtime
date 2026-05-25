#include "ModbusServer.hpp"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>
#include <mutex>

ModbusServer::ModbusServer(PlcMemory& memory, const std::string& ip_address, uint16_t port)
    : memory_(memory),
      ip_address_(ip_address),
      port_(port),
      server_fd_(-1),
      is_running_(false) {}

ModbusServer::~ModbusServer() {
    stop();
}

bool ModbusServer::start() {
    if (is_running_) return true;

    // Create server socket
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "[ModbusServer] Error creating socket: " << strerror(errno) << std::endl;
        return false;
    }

    // Set socket options (Allow port reuse)
    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "[ModbusServer] Error setting socket options: " << strerror(errno) << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    // Bind socket
    struct sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port_);
    if (ip_address_ == "0.0.0.0") {
        address.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, ip_address_.c_str(), &address.sin_addr);
    }

    if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "[ModbusServer] Error binding to " << ip_address_ << ":" << port_
                  << " - " << strerror(errno) << std::endl;
        std::cerr << "[ModbusServer] Tip: Ports < 1024 require root/sudo privileges. Try port >= 1024 (e.g. 5020)" << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    // Start listening
    if (listen(server_fd_, 10) < 0) {
        std::cerr << "[ModbusServer] Error listening: " << strerror(errno) << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    is_running_ = true;
    server_thread_ = std::thread(&ModbusServer::serverLoop, this);
    std::cout << "[ModbusServer] Server started on " << ip_address_ << ":" << port_ << std::endl;

    return true;
}

void ModbusServer::stop() {
    if (!is_running_) return;

    is_running_ = false;

    // Shutdown server socket to wake up accept()
    if (server_fd_ >= 0) {
        shutdown(server_fd_, SHUT_RDWR);
        close(server_fd_);
    }

    // Close all connected client sockets
    {
        std::unique_lock<std::shared_mutex> lock(clients_mutex_);
        for (int fd : client_fds_) {
            if (fd >= 0) {
                shutdown(fd, SHUT_RDWR);
                close(fd);
            }
        }
        client_fds_.clear();
    }

    // Join main server thread
    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    server_fd_ = -1;
    std::cout << "[ModbusServer] Server stopped" << std::endl;
}

size_t ModbusServer::getConnectedClientsCount() const {
    std::shared_lock<std::shared_mutex> lock(clients_mutex_);
    return client_fds_.size();
}

void ModbusServer::serverLoop() {
    while (is_running_) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) {
            if (is_running_) {
                std::cerr << "[ModbusServer] accept() failed: " << strerror(errno) << std::endl;
            }
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        
        // Track the connection
        {
            std::unique_lock<std::shared_mutex> lock(clients_mutex_);
            client_fds_.push_back(client_fd);
        }

        // Spawn a thread for client handler
        std::thread client_thread(&ModbusServer::handleClient, this, client_fd, std::string(client_ip));
        client_thread.detach(); // Let it run independently
    }
}

void ModbusServer::handleClient(int client_fd, std::string client_ip) {
    std::cout << "[ModbusServer] Client connected: " << client_ip << " (socket " << client_fd << ")" << std::endl;

    std::vector<uint8_t> buffer(2048);

    while (is_running_) {
        // 1. Read MBAP Header (7 bytes)
        size_t bytes_to_read = 7;
        size_t bytes_read = 0;
        
        while (bytes_read < bytes_to_read) {
            ssize_t n = recv(client_fd, buffer.data() + bytes_read, bytes_to_read - bytes_read, 0);
            if (n <= 0) {
                // Connection closed or error
                goto client_disconnect;
            }
            bytes_read += n;
        }

        // Decode MBAP header
        uint16_t transaction_id = readUint16(&buffer[0]);
        uint16_t protocol_id = readUint16(&buffer[2]);
        uint16_t length = readUint16(&buffer[4]);
        uint8_t unit_id = buffer[6];

        if (protocol_id != 0) {
            std::cerr << "[ModbusServer] Invalid Modbus protocol ID: " << protocol_id << std::endl;
            goto client_disconnect;
        }

        if (length < 2 || length > 250) {
            std::cerr << "[ModbusServer] Invalid PDU length in MBAP: " << length << std::endl;
            goto client_disconnect;
        }

        // 2. Read the remaining PDU data (length - 1 bytes, since length includes unit_id)
        size_t pdu_len = length - 1;
        bytes_to_read = 7 + pdu_len;
        
        while (bytes_read < bytes_to_read) {
            ssize_t n = recv(client_fd, buffer.data() + bytes_read, bytes_to_read - bytes_read, 0);
            if (n <= 0) {
                goto client_disconnect;
            }
            bytes_read += n;
        }

        // Extract request PDU
        std::vector<uint8_t> request_pdu(buffer.begin() + 7, buffer.begin() + 7 + pdu_len);

        // Process request PDU
        std::vector<uint8_t> response_pdu = processRequest(request_pdu);

        // Construct response MBAP + PDU
        std::vector<uint8_t> response(7 + response_pdu.size());
        writeUint16(&response[0], transaction_id);
        writeUint16(&response[2], protocol_id);
        writeUint16(&response[4], static_cast<uint16_t>(response_pdu.size() + 1)); // PDU + unit ID
        response[6] = unit_id;
        std::memcpy(response.data() + 7, response_pdu.data(), response_pdu.size());

        // Send response
        ssize_t sent = send(client_fd, response.data(), response.size(), 0);
        if (sent <= 0) {
            std::cerr << "[ModbusServer] Send failed: " << strerror(errno) << std::endl;
            goto client_disconnect;
        }
    }

client_disconnect:
    std::cout << "[ModbusServer] Client disconnected: " << client_ip << " (socket " << client_fd << ")" << std::endl;
    close(client_fd);

    // Remove from client tracking list
    {
        std::unique_lock<std::shared_mutex> lock(clients_mutex_);
        client_fds_.erase(std::remove(client_fds_.begin(), client_fds_.end(), client_fd), client_fds_.end());
    }
}

std::vector<uint8_t> ModbusServer::processRequest(const std::vector<uint8_t>& request) {
    if (request.empty()) {
        return makeExceptionResponse(0, 0x04); // Server Failure
    }

    uint8_t function_code = request[0];
    
    // Check minimal sizes per function code
    if (request.size() < 5) {
        return makeExceptionResponse(function_code, 0x03); // Illegal Data Value
    }

    uint16_t start_addr = readUint16(&request[1]);
    uint16_t quantity_val = readUint16(&request[3]);

    std::vector<uint8_t> response;

    switch (function_code) {
        case 0x01: { // Read Coils (%QX)
            if (quantity_val < 1 || quantity_val > 2000) {
                return makeExceptionResponse(function_code, 0x03);
            }
            if (start_addr + quantity_val > memory_.getCoilsSize()) {
                return makeExceptionResponse(function_code, 0x02); // Illegal Data Address
            }

            std::vector<bool> coils = memory_.readCoils(start_addr, quantity_val);
            uint8_t byte_count = static_cast<uint8_t>((quantity_val + 7) / 8);
            
            response.push_back(function_code);
            response.push_back(byte_count);
            
            for (uint8_t b = 0; b < byte_count; ++b) {
                uint8_t value = 0;
                for (int bit = 0; bit < 8; ++bit) {
                    size_t idx = b * 8 + bit;
                    if (idx < coils.size() && coils[idx]) {
                        value |= (1 << bit);
                    }
                }
                response.push_back(value);
            }
            break;
        }

        case 0x02: { // Read Discrete Inputs (%IX)
            if (quantity_val < 1 || quantity_val > 2000) {
                return makeExceptionResponse(function_code, 0x03);
            }
            if (start_addr + quantity_val > memory_.getDiscreteInputsSize()) {
                return makeExceptionResponse(function_code, 0x02);
            }

            std::vector<bool> inputs = memory_.readDiscreteInputs(start_addr, quantity_val);
            uint8_t byte_count = static_cast<uint8_t>((quantity_val + 7) / 8);
            
            response.push_back(function_code);
            response.push_back(byte_count);
            
            for (uint8_t b = 0; b < byte_count; ++b) {
                uint8_t value = 0;
                for (int bit = 0; bit < 8; ++bit) {
                    size_t idx = b * 8 + bit;
                    if (idx < inputs.size() && inputs[idx]) {
                        value |= (1 << bit);
                    }
                }
                response.push_back(value);
            }
            break;
        }

        case 0x03: { // Read Holding Registers (%MW)
            if (quantity_val < 1 || quantity_val > 125) {
                return makeExceptionResponse(function_code, 0x03);
            }
            if (start_addr + quantity_val > memory_.getHoldingRegistersSize()) {
                return makeExceptionResponse(function_code, 0x02);
            }

            std::vector<uint16_t> registers = memory_.readHoldingRegisters(start_addr, quantity_val);
            uint8_t byte_count = static_cast<uint8_t>(2 * quantity_val);

            response.push_back(function_code);
            response.push_back(byte_count);

            for (uint16_t val : registers) {
                response.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
                response.push_back(static_cast<uint8_t>(val & 0xFF));
            }
            break;
        }

        case 0x04: { // Read Input Registers (%IW)
            if (quantity_val < 1 || quantity_val > 125) {
                return makeExceptionResponse(function_code, 0x03);
            }
            if (start_addr + quantity_val > memory_.getInputRegistersSize()) {
                return makeExceptionResponse(function_code, 0x02);
            }

            std::vector<uint16_t> registers = memory_.readInputRegisters(start_addr, quantity_val);
            uint8_t byte_count = static_cast<uint8_t>(2 * quantity_val);

            response.push_back(function_code);
            response.push_back(byte_count);

            for (uint16_t val : registers) {
                response.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
                response.push_back(static_cast<uint8_t>(val & 0xFF));
            }
            break;
        }

        case 0x05: { // Write Single Coil
            if (start_addr >= memory_.getCoilsSize()) {
                return makeExceptionResponse(function_code, 0x02);
            }
            // Value is passed in quantity_val variable (bytes 3 and 4)
            // 0xFF00 turns ON, 0x0000 turns OFF
            if (quantity_val != 0xFF00 && quantity_val != 0x0000) {
                return makeExceptionResponse(function_code, 0x03);
            }

            memory_.writeCoil(start_addr, quantity_val == 0xFF00);
            
            // Echo request
            response.push_back(function_code);
            response.push_back(request[1]);
            response.push_back(request[2]);
            response.push_back(request[3]);
            response.push_back(request[4]);
            break;
        }

        case 0x06: { // Write Single Register
            if (start_addr >= memory_.getHoldingRegistersSize()) {
                return makeExceptionResponse(function_code, 0x02);
            }
            // Value is passed in quantity_val
            memory_.writeHoldingRegister(start_addr, quantity_val);

            // Echo request
            response.push_back(function_code);
            response.push_back(request[1]);
            response.push_back(request[2]);
            response.push_back(request[3]);
            response.push_back(request[4]);
            break;
        }

        case 0x0F: { // Write Multiple Coils
            if (request.size() < 6) {
                return makeExceptionResponse(function_code, 0x03);
            }
            uint8_t byte_count = request[5];
            if (request.size() < static_cast<size_t>(6 + byte_count)) {
                return makeExceptionResponse(function_code, 0x03);
            }
            if (quantity_val < 1 || quantity_val > 2000 || byte_count != (quantity_val + 7) / 8) {
                return makeExceptionResponse(function_code, 0x03);
            }
            if (start_addr + quantity_val > memory_.getCoilsSize()) {
                return makeExceptionResponse(function_code, 0x02);
            }

            std::vector<bool> values;
            values.reserve(quantity_val);
            for (uint16_t i = 0; i < quantity_val; ++i) {
                uint8_t byte_idx = 6 + (i / 8);
                uint8_t bit_idx = i % 8;
                bool bit_val = (request[byte_idx] & (1 << bit_idx)) != 0;
                values.push_back(bit_val);
            }

            memory_.writeCoils(start_addr, values);

            // Response: FC, Starting Address, Quantity
            response.push_back(function_code);
            response.push_back(request[1]);
            response.push_back(request[2]);
            response.push_back(request[3]);
            response.push_back(request[4]);
            break;
        }

        case 0x10: { // Write Multiple Registers
            if (request.size() < 6) {
                return makeExceptionResponse(function_code, 0x03);
            }
            uint8_t byte_count = request[5];
            if (request.size() < static_cast<size_t>(6 + byte_count)) {
                return makeExceptionResponse(function_code, 0x03);
            }
            if (quantity_val < 1 || quantity_val > 123 || byte_count != 2 * quantity_val) {
                return makeExceptionResponse(function_code, 0x03);
            }
            if (start_addr + quantity_val > memory_.getHoldingRegistersSize()) {
                return makeExceptionResponse(function_code, 0x02);
            }

            std::vector<uint16_t> values;
            values.reserve(quantity_val);
            for (uint16_t i = 0; i < quantity_val; ++i) {
                values.push_back(readUint16(&request[6 + i * 2]));
            }

            memory_.writeHoldingRegisters(start_addr, values);

            // Response: FC, Starting Address, Quantity
            response.push_back(function_code);
            response.push_back(request[1]);
            response.push_back(request[2]);
            response.push_back(request[3]);
            response.push_back(request[4]);
            break;
        }

        default: {
            // Function code not supported
            return makeExceptionResponse(function_code, 0x01); // Illegal Function Code
        }
    }

    return response;
}

std::vector<uint8_t> ModbusServer::makeExceptionResponse(uint8_t function_code, uint8_t exception_code) {
    std::vector<uint8_t> response;
    response.push_back(function_code | 0x80);
    response.push_back(exception_code);
    return response;
}
