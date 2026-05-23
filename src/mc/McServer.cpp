#include "McServer.hpp"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>

McServer::McServer(PlcMemory& memory, const std::string& ip_address, uint16_t port)
    : memory_(memory),
      ip_address_(ip_address),
      port_(port),
      server_fd_(-1),
      is_running_(false) {}

McServer::~McServer() {
    stop();
}

bool McServer::start() {
    if (is_running_) return true;

    // Create server socket
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "[McServer] Error creating socket: " << strerror(errno) << std::endl;
        return false;
    }

    // Allow port reuse
    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "[McServer] Error setting socket options: " << strerror(errno) << std::endl;
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
        std::cerr << "[McServer] Error binding to " << ip_address_ << ":" << port_
                  << " - " << strerror(errno) << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    // Start listening
    if (listen(server_fd_, 10) < 0) {
        std::cerr << "[McServer] Error listening: " << strerror(errno) << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    is_running_ = true;
    server_thread_ = std::thread(&McServer::serverLoop, this);
    std::cout << "[McServer] Mitsubishi MC Server started on " << ip_address_ << ":" << port_ << std::endl;

    return true;
}

void McServer::stop() {
    if (!is_running_) return;

    is_running_ = false;

    // Close server socket to wake up accept()
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
    std::cout << "[McServer] Mitsubishi MC Server stopped" << std::endl;
}

size_t McServer::getClientsCount() const {
    std::shared_lock<std::shared_mutex> lock(clients_mutex_);
    return client_fds_.size();
}

void McServer::serverLoop() {
    while (is_running_) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) {
            if (is_running_) {
                std::cerr << "[McServer] accept() failed: " << strerror(errno) << std::endl;
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

        // Spawn thread for client
        std::thread client_thread(&McServer::handleClient, this, client_fd, std::string(client_ip));
        client_thread.detach();
    }
}

void McServer::handleClient(int client_fd, std::string client_ip) {
    std::cout << "[McServer] Client connected: " << client_ip << " (socket " << client_fd << ")" << std::endl;

    std::vector<uint8_t> header(9);

    while (is_running_) {
        // 1. Read standard 3E Frame header (9 bytes)
        size_t bytes_to_read = 9;
        size_t bytes_read = 0;
        bool disconnected = false;

        while (bytes_read < bytes_to_read) {
            ssize_t ret = recv(client_fd, header.data() + bytes_read, bytes_to_read - bytes_read, 0);
            if (ret <= 0) {
                disconnected = true;
                break;
            }
            bytes_read += ret;
        }

        if (disconnected) {
            break;
        }

        // Verify subheader is 0x50 0x00 (Request)
        if (header[0] != 0x50 || header[1] != 0x00) {
            std::cerr << "[McServer] Invalid Subheader received: " 
                      << std::hex << (int)header[0] << " " << (int)header[1] << std::dec << std::endl;
            break;
        }

        // 2. Extract remaining request data length
        uint16_t req_data_len = readUint16LE(&header[7]);
        if (req_data_len == 0 || req_data_len > 2048) {
            std::cerr << "[McServer] Invalid request data length: " << req_data_len << std::endl;
            break;
        }

        // 3. Read remaining request body
        std::vector<uint8_t> body(req_data_len);
        size_t body_read = 0;
        while (body_read < req_data_len) {
            ssize_t ret = recv(client_fd, body.data() + body_read, req_data_len - body_read, 0);
            if (ret <= 0) {
                disconnected = true;
                break;
            }
            body_read += ret;
        }

        if (disconnected) {
            break;
        }

        // 4. Construct complete request packet
        std::vector<uint8_t> request;
        request.reserve(9 + req_data_len);
        request.insert(request.end(), header.begin(), header.end());
        request.insert(request.end(), body.begin(), body.end());

        // 5. Process request
        std::vector<uint8_t> response = processRequest(request);

        // 6. Send response back
        ssize_t bytes_sent = send(client_fd, response.data(), response.size(), 0);
        if (bytes_sent < 0) {
            std::cerr << "[McServer] Send failed" << std::endl;
            break;
        }
    }

    // Clean up connection
    std::cout << "[McServer] Client disconnected: " << client_ip << " (socket " << client_fd << ")" << std::endl;
    close(client_fd);

    std::unique_lock<std::shared_mutex> lock(clients_mutex_);
    auto it = std::find(client_fds_.begin(), client_fds_.end(), client_fd);
    if (it != client_fds_.end()) {
        client_fds_.erase(it);
    }
}

std::vector<uint8_t> McServer::processRequest(const std::vector<uint8_t>& request) {
    if (request.size() < 21) {
        return makeErrorResponse(0xC051); // Invalid request format length
    }

    // Parse QnA 3E Frame header fields
    uint16_t command = readUint16LE(&request[11]);
    uint16_t subcommand = readUint16LE(&request[13]);
    
    // Address (3 bytes LE)
    uint32_t start_addr = static_cast<uint32_t>(request[15]) |
                          (static_cast<uint32_t>(request[16]) << 8) |
                          (static_cast<uint32_t>(request[17]) << 16);
    
    uint8_t device_code = request[18];
    uint16_t points = readUint16LE(&request[19]);

    std::vector<uint8_t> resp_data;

    if (command == 0x0401) {
        // --- BATCH READ ---
        if (subcommand == 0x0000) {
            // 1. Word unit read (D registers %MW)
            if (device_code == 0xA8) { // Device D
                resp_data.resize(2 + points * 2);
                writeUint16LE(&resp_data[0], 0x0000); // End Code: Success

                for (uint16_t i = 0; i < points; ++i) {
                    uint16_t val = memory_.readHoldingRegister(start_addr + i);
                    writeUint16LE(&resp_data[2 + i * 2], val);
                }
            } else {
                return makeErrorResponse(0xC05C); // Unsupported device code
            }
        } 
        else if (subcommand == 0x0001) {
            // 2. Bit unit read (X registers %IX, Y registers %QX)
            if (device_code == 0x9C || device_code == 0x9D) { // X or Y
                size_t bit_bytes = (points + 1) / 2;
                resp_data.resize(2 + bit_bytes, 0);
                writeUint16LE(&resp_data[0], 0x0000); // End Code: Success

                for (uint16_t i = 0; i < points; ++i) {
                    bool bit_val = false;
                    if (device_code == 0x9C) {
                        bit_val = memory_.readDiscreteInput(start_addr + i);
                    } else {
                        bit_val = memory_.readCoil(start_addr + i);
                    }

                    // Pack: High 4 bits for first point, Low 4 bits for second point
                    size_t byte_idx = i / 2;
                    if (i % 2 == 0) {
                        resp_data[2 + byte_idx] |= (bit_val ? 0x10 : 0x00);
                    } else {
                        resp_data[2 + byte_idx] |= (bit_val ? 0x01 : 0x00);
                    }
                }
            } else {
                return makeErrorResponse(0xC05C); // Unsupported device code
            }
        } else {
            return makeErrorResponse(0xC051); // Invalid subcommand
        }
    } 
    else if (command == 0x1401) {
        // --- BATCH WRITE ---
        if (subcommand == 0x0000) {
            // 1. Word unit write (D registers %MW)
            if (device_code == 0xA8) {
                if (request.size() < 21 + points * 2) {
                    return makeErrorResponse(0xC051);
                }

                for (uint16_t i = 0; i < points; ++i) {
                    uint16_t val = readUint16LE(&request[21 + i * 2]);
                    memory_.writeHoldingRegister(start_addr + i, val);
                }

                resp_data.resize(2);
                writeUint16LE(&resp_data[0], 0x0000); // End Code: Success
            } else {
                return makeErrorResponse(0xC05C);
            }
        } 
        else if (subcommand == 0x0001) {
            // 2. Bit unit write (X registers %IX, Y registers %QX)
            if (device_code == 0x9C || device_code == 0x9D) {
                size_t bit_bytes = (points + 1) / 2;
                if (request.size() < 21 + bit_bytes) {
                    return makeErrorResponse(0xC051);
                }

                for (uint16_t i = 0; i < points; ++i) {
                    size_t byte_idx = i / 2;
                    uint8_t byte_val = request[21 + byte_idx];
                    bool bit_val = false;

                    if (i % 2 == 0) {
                        bit_val = ((byte_val >> 4) & 0x0F) != 0;
                    } else {
                        bit_val = (byte_val & 0x0F) != 0;
                    }

                    if (device_code == 0x9C) {
                        memory_.writeDiscreteInput(start_addr + i, bit_val);
                    } else {
                        memory_.writeCoil(start_addr + i, bit_val);
                    }
                }

                resp_data.resize(2);
                writeUint16LE(&resp_data[0], 0x0000); // End Code: Success
            } else {
                return makeErrorResponse(0xC05C);
            }
        } else {
            return makeErrorResponse(0xC051);
        }
    } else {
        return makeErrorResponse(0xC051); // Unknown command
    }

    // Construct response packet
    std::vector<uint8_t> response(9 + resp_data.size());
    
    // Subheader: 0xD0 0x00 (Response)
    response[0] = 0xD0;
    response[1] = 0x00;
    
    // Copy routing info from request
    response[2] = request[2]; // Network No.
    response[3] = request[3]; // PC No.
    response[4] = request[4]; // Module I/O No LSB
    response[5] = request[5]; // Module I/O No MSB
    response[6] = request[6]; // Station No.
    
    // Response Data Length (End Code + Data size)
    writeUint16LE(&response[7], static_cast<uint16_t>(resp_data.size()));
    
    // Copy end code and read data
    std::memcpy(response.data() + 9, resp_data.data(), resp_data.size());

    return response;
}

std::vector<uint8_t> McServer::makeErrorResponse(uint16_t end_code) {
    std::vector<uint8_t> response(11);
    response[0] = 0xD0;
    response[1] = 0x00;
    response[2] = 0x00; // Network
    response[3] = 0xFF; // PC
    response[4] = 0xFF; // Module LSB
    response[5] = 0x03; // Module MSB
    response[6] = 0x00; // Station
    
    writeUint16LE(&response[7], 2); // Length of End Code
    writeUint16LE(&response[9], end_code); // End Code
    
    return response;
}
