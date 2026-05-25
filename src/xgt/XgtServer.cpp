#include "XgtServer.hpp"
#include <iostream>
#include <cstring>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>
#include <mutex>

XgtServer::XgtServer(PlcMemory& memory, const std::string& ip_address, uint16_t port)
    : memory_(memory),
      ip_address_(ip_address),
      port_(port),
      server_fd_(-1),
      is_running_(false) {}

XgtServer::~XgtServer() {
    stop();
}

bool XgtServer::start() {
    if (is_running_) return true;

    // Create server socket
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "[XgtServer] Error creating socket: " << strerror(errno) << std::endl;
        return false;
    }

    // Allow port reuse
    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "[XgtServer] Error setting socket options: " << strerror(errno) << std::endl;
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
        std::cerr << "[XgtServer] Error binding to " << ip_address_ << ":" << port_
                  << " - " << strerror(errno) << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    // Start listening
    if (listen(server_fd_, 10) < 0) {
        std::cerr << "[XgtServer] Error listening: " << strerror(errno) << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    is_running_ = true;
    server_thread_ = std::thread(&XgtServer::serverLoop, this);
    std::cout << "[XgtServer] LS Electric XGT Server started on " << ip_address_ << ":" << port_ << std::endl;

    return true;
}

void XgtServer::stop() {
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
    std::cout << "[XgtServer] LS Electric XGT Server stopped" << std::endl;
}

size_t XgtServer::getClientsCount() const {
    std::shared_lock<std::shared_mutex> lock(clients_mutex_);
    return client_fds_.size();
}

void XgtServer::serverLoop() {
    while (is_running_) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) {
            if (is_running_) {
                std::cerr << "[XgtServer] accept() failed: " << strerror(errno) << std::endl;
            }
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        
        // Track connection
        {
            std::unique_lock<std::shared_mutex> lock(clients_mutex_);
            client_fds_.push_back(client_fd);
        }

        // Spawn client thread
        std::thread client_thread(&XgtServer::handleClient, this, client_fd, std::string(client_ip));
        client_thread.detach();
    }
}

void XgtServer::handleClient(int client_fd, std::string client_ip) {
    std::cout << "[XgtServer] Client connected: " << client_ip << " (socket " << client_fd << ")" << std::endl;

    // Standard LS Dedicated header has 18 bytes up to the Length field
    std::vector<uint8_t> header(18);

    while (is_running_) {
        size_t bytes_to_read = 18;
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

        // Verify Company ID prefix (first 8 bytes must be "LSIS-XGT" or "LGIS-GLO")
        if (std::memcmp(header.data(), "LSIS-XGT", 8) != 0 && std::memcmp(header.data(), "LGIS-GLO", 8) != 0) {
            std::cerr << "[XgtServer] Invalid Company ID received: " 
                      << std::string(reinterpret_cast<const char*>(header.data()), 8) << std::endl;
            break;
        }

        // Extract remaining packet length
        uint16_t packet_len = readUint16LE(&header[16]);
        if (packet_len == 0 || packet_len > 2048) {
            std::cerr << "[XgtServer] Invalid packet length: " << packet_len << std::endl;
            break;
        }

        // Read remaining request body
        std::vector<uint8_t> body(packet_len);
        size_t body_read = 0;
        while (body_read < packet_len) {
            ssize_t ret = recv(client_fd, body.data() + body_read, packet_len - body_read, 0);
            if (ret <= 0) {
                disconnected = true;
                break;
            }
            body_read += ret;
        }

        if (disconnected) {
            break;
        }

        // Construct complete request packet
        std::vector<uint8_t> request;
        request.reserve(18 + packet_len);
        request.insert(request.end(), header.begin(), header.end());
        request.insert(request.end(), body.begin(), body.end());

        // Process request
        std::vector<uint8_t> response = processRequest(request);

        // Send response back
        ssize_t bytes_sent = send(client_fd, response.data(), response.size(), 0);
        if (bytes_sent < 0) {
            std::cerr << "[XgtServer] Send failed" << std::endl;
            break;
        }
    }

    // Clean up
    std::cout << "[XgtServer] Client disconnected: " << client_ip << " (socket " << client_fd << ")" << std::endl;
    close(client_fd);

    std::unique_lock<std::shared_mutex> lock(clients_mutex_);
    auto it = std::find(client_fds_.begin(), client_fds_.end(), client_fd);
    if (it != client_fds_.end()) {
        client_fds_.erase(it);
    }
}

std::vector<uint8_t> XgtServer::processRequest(const std::vector<uint8_t>& request) {
    // Basic length verification: header (18) + Slot/Station/Checksum/Reserved (4) + Cmd (2) + DataType (2) + Reserved (2) + Blocks (2) + VarLen (2) = 34 bytes
    if (request.size() < 34) {
        std::vector<uint8_t> err_resp(20, 0);
        std::memcpy(err_resp.data(), "LSIS-XGT\x00\x00", 10);
        err_resp[13] = 0x11; // Response direction
        writeUint16LE(&err_resp[16], 2); // Length
        writeUint16LE(&err_resp[18], 0x00FF); // Error code: general failure
        return err_resp;
    }

    uint16_t command = readUint16LE(&request[22]);
    uint16_t data_type = readUint16LE(&request[24]);
    uint16_t var_len = readUint16LE(&request[30]);

    if (request.size() < 32 + var_len) {
        std::vector<uint8_t> err_resp(20, 0);
        std::memcpy(err_resp.data(), "LSIS-XGT\x00\x00", 10);
        err_resp[13] = 0x11;
        return err_resp;
    }

    // Extract ASCII variable name
    std::string var_name(reinterpret_cast<const char*>(&request[32]), var_len);
    
    // Resolve address to flat PlcMemory indexes
    bool is_bit = false;
    bool is_input = false;
    uint16_t index = 0;
    uint16_t sub_index = 0;
    bool resolve_success = resolveAddress(var_name, is_bit, is_input, index, sub_index);

    std::vector<uint8_t> resp_data;

    if (command == 0x0054) {
        // --- READ COMMAND ---
        if (!resolve_success) {
            resp_data.resize(4);
            writeUint16LE(&resp_data[0], 0x1102); // Error status: Address parsing error
            writeUint16LE(&resp_data[2], 0);      // Data Length
        } else {
            resp_data.resize(6);
            writeUint16LE(&resp_data[0], 0x0000); // Error status: Success
            writeUint16LE(&resp_data[2], var_len); // Echo var name length
            
            // Insert variable name into response
            resp_data.insert(resp_data.end(), var_name.begin(), var_name.end());
            
            size_t data_offset = resp_data.size();
            if (is_bit) {
                resp_data.resize(data_offset + 3);
                writeUint16LE(&resp_data[data_offset], 1); // 1 byte data length
                
                // Read bit value
                bool bit_val = false;
                // If it is multi-dot like %IX0.0.1, we map flat index Base*8 + Channel
                uint16_t flat_idx = index * 8 + sub_index;
                if (is_input) {
                    bit_val = memory_.readDiscreteInput(flat_idx);
                } else {
                    bit_val = memory_.readCoil(flat_idx);
                }
                resp_data[data_offset + 2] = bit_val ? 0x01 : 0x00;
            } else {
                resp_data.resize(data_offset + 4);
                writeUint16LE(&resp_data[data_offset], 2); // 2 bytes data length (Word)
                
                uint16_t val = 0;
                if (is_input) {
                    val = memory_.readInputRegister(index);
                } else {
                    val = memory_.readHoldingRegister(index);
                }
                writeUint16LE(&resp_data[data_offset + 2], val);
            }
        }
    } 
    else if (command == 0x0058) {
        // --- WRITE COMMAND ---
        size_t write_len_offset = 32 + var_len;
        if (request.size() < write_len_offset + 2) {
            resp_data.resize(4);
            writeUint16LE(&resp_data[0], 0x1102); // Length error
        } else if (!resolve_success) {
            resp_data.resize(4);
            writeUint16LE(&resp_data[0], 0x1102); // Address parsing error
        } else {
            uint16_t write_data_len = readUint16LE(&request[write_len_offset]);
            if (request.size() < write_len_offset + 2 + write_data_len) {
                resp_data.resize(4);
                writeUint16LE(&resp_data[0], 0x1102);
            } else {
                // Perform write to memory
                if (is_bit) {
                    uint16_t flat_idx = index * 8 + sub_index;
                    bool val = request[write_len_offset + 2] != 0;
                    if (is_input) {
                        memory_.writeDiscreteInput(flat_idx, val);
                    } else {
                        memory_.writeCoil(flat_idx, val);
                    }
                } else {
                    uint16_t val = readUint16LE(&request[write_len_offset + 2]);
                    if (is_input) {
                        memory_.writeInputRegister(index, val);
                    } else {
                        memory_.writeHoldingRegister(index, val);
                    }
                }

                resp_data.resize(4 + var_len);
                writeUint16LE(&resp_data[0], 0x0000); // Error status: Success
                writeUint16LE(&resp_data[2], var_len); // Echo var name length
                std::memcpy(resp_data.data() + 4, var_name.data(), var_len);
            }
        }
    } else {
        resp_data.resize(4);
        writeUint16LE(&resp_data[0], 0x00FF); // Unknown command
    }

    // Construct final response packet
    std::vector<uint8_t> response(30 + resp_data.size()); // header(18) + routing(4) + response body(8 + resp_data)
    
    // 1. Copy Company ID & Base fields (0-13)
    std::memcpy(response.data(), request.data(), 12);
    response[12] = request[12]; // CPU Info
    response[13] = 0x11;        // Response direction (0x11)
    
    // 2. Invoke ID
    response[14] = request[14];
    response[15] = request[15];
    
    // 3. Length of response remaining payload starting from index 18 (Slot No)
    writeUint16LE(&response[16], static_cast<uint16_t>(4 + resp_data.size()));
    
    // 4. Routing block (Slot, Station, Checksum, Reserved) at index 18-21
    response[18] = request[18]; // Slot
    response[19] = request[19]; // Station
    response[20] = 0x00;        // Checksum
    response[21] = 0x00;        // Reserved
    
    // 5. Command Response code
    uint16_t resp_cmd = (command == 0x0054) ? 0x0055 : 0x0059;
    writeUint16LE(&response[22], resp_cmd);
    
    // 6. Data Type
    writeUint16LE(&response[24], data_type);
    
    // 7. Reserved & Block Count
    response[26] = 0x00;
    response[27] = 0x00;
    writeUint16LE(&response[28], 1); // 1 block
    
    // 8. Copy response data
    std::memcpy(response.data() + 30, resp_data.data(), resp_data.size());

    return response;
}

bool XgtServer::resolveAddress(const std::string& addr_str, bool& is_bit, bool& is_input, uint16_t& index, uint16_t& sub_index) {
    if (addr_str.size() < 4) return false;
    
    size_t start = 0;
    if (addr_str[0] == '%') {
        start = 1;
    }
    
    if (addr_str.size() - start < 3) return false;
    
    char area = addr_str[start];
    char type = addr_str[start + 1];
    
    if (area == 'I') {
        is_input = true;
    } else if (area == 'Q' || area == 'M') {
        is_input = false;
    } else {
        return false;
    }
    
    if (type == 'X') {
        is_bit = true;
    } else if (type == 'W') {
        is_bit = false;
    } else {
        return false;
    }
    
    std::string number_part = addr_str.substr(start + 2);
    
    if (is_bit) {
        std::vector<int> nums;
        std::string token;
        std::stringstream ss(number_part);
        while (std::getline(ss, token, '.')) {
            try {
                nums.push_back(std::stoi(token));
            } catch (...) {
                return false;
            }
        }
        
        if (nums.empty()) return false;
        if (nums.size() == 1) {
            index = nums[0];
            sub_index = 0;
        } else if (nums.size() == 2) {
            index = nums[0];
            sub_index = nums[1];
        } else {
            // e.g. %IX0.0.1 -> index=nums[last-1], sub_index=nums[last]
            index = nums[nums.size() - 2];
            sub_index = nums[nums.size() - 1];
        }
    } else {
        try {
            index = std::stoi(number_part);
            sub_index = 0;
        } catch (...) {
            return false;
        }
    }
    
    return true;
}
