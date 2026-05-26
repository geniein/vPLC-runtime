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

        // --- PROTOCOL DRAIN & RECOVERY LAYER FOR PyXGT BUG ---
        // PyXGT length field (offset 16-17) is written as len(app_instruction) (14) instead of 2 + len(app_instruction) (16).
        // This causes the server to stop reading before the last 2 bytes are retrieved.
        // We inspect the body and read the exact remaining bytes blockingly to avoid any timing or scheduling issues.
        if (body.size() >= 12) {
            uint16_t command = body[2] | (body[3] << 8);
            uint16_t var_len = body[10] | (body[11] << 8);
            
            size_t expected_body_len = 12 + var_len;
            if (command == 0x0058) { // Write command
                // For Write, we need 12 + var_len + 2 bytes to read the write_data_len field
                expected_body_len = 12 + var_len + 2;
            }
            
            // If we have less than expected, read the difference blockingly
            if (body.size() < expected_body_len) {
                size_t diff = expected_body_len - body.size();
                std::cerr << "[XgtServer Debug] PyXGT bug detected! expected_body_len=" << expected_body_len 
                          << ", body.size()=" << body.size() << ", diff=" << diff << " blockingly..." << std::endl;
                std::vector<uint8_t> extra(diff);
                size_t extra_read = 0;
                while (extra_read < diff) {
                    ssize_t ret = recv(client_fd, extra.data() + extra_read, diff - extra_read, 0);
                    if (ret <= 0) {
                        disconnected = true;
                        break;
                    }
                    extra_read += ret;
                }
                if (disconnected) break;
                body.insert(body.end(), extra.begin(), extra.end());
                std::cerr << "[XgtServer Debug] Successfully read extra " << extra_read 
                          << " bytes. New body size=" << body.size() << std::endl;
            }
            
            // Now, if it is a Write command, we can check the write_data_len and read the remaining data
            if (command == 0x0058 && body.size() >= 12 + var_len + 2) {
                uint16_t write_data_len = body[12 + var_len] | (body[12 + var_len + 1] << 8);
                
                // If write_data_len is PyXGT's buggy "20 00" (0x0020), correct it to actual 2 bytes data
                if (write_data_len == 0x0020) {
                    std::cerr << "[XgtServer Debug] Corrected PyXGT write_data_len from 0x0020 to 2 bytes" << std::endl;
                    write_data_len = 2;
                }
                
                size_t full_expected_len = 12 + var_len + 2 + write_data_len;
                if (body.size() < full_expected_len) {
                    size_t diff = full_expected_len - body.size();
                    std::cerr << "[XgtServer Debug] PyXGT Write diff detected! expected=" << full_expected_len 
                              << ", body=" << body.size() << ", diff=" << diff << " blockingly..." << std::endl;
                    std::vector<uint8_t> extra(diff);
                    size_t extra_read = 0;
                    while (extra_read < diff) {
                        ssize_t ret = recv(client_fd, extra.data() + extra_read, diff - extra_read, 0);
                        if (ret <= 0) {
                            disconnected = true;
                            break;
                        }
                        extra_read += ret;
                    }
                    if (disconnected) break;
                    body.insert(body.end(), extra.begin(), extra.end());
                    std::cerr << "[XgtServer Debug] Successfully read extra write " << extra_read 
                              << " bytes. New body size=" << body.size() << std::endl;
                }
            }
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
    if (request.size() < 30) {
        std::vector<uint8_t> err_resp(30, 0);
        std::memcpy(err_resp.data(), "LSIS-XGT\x00\x00", 10);
        err_resp[13] = 0x11; // Response direction
        writeUint16LE(&err_resp[16], 12); // Length of payload starting from offset 18
        writeUint16LE(&err_resp[20], 0x00FF); // Error code/cmd: general failure
        writeUint16LE(&err_resp[26], 0x00FF); // Error status
        return err_resp;
    }

    uint16_t command = readUint16LE(&request[20]);
    uint16_t data_type = readUint16LE(&request[22]);
    uint16_t var_len = readUint16LE(&request[28]);

    if (request.size() < 30u + var_len) {
        std::vector<uint8_t> err_resp(30, 0);
        std::memcpy(err_resp.data(), "LSIS-XGT\x00\x00", 10);
        err_resp[13] = 0x11;
        writeUint16LE(&err_resp[16], 12);
        writeUint16LE(&err_resp[20], (command == 0x0054) ? 0x0055 : 0x0059);
        writeUint16LE(&err_resp[26], 0x1102); // Length error
        return err_resp;
    }

    // Extract ASCII variable name
    std::string var_name(reinterpret_cast<const char*>(&request[30]), var_len);
    
    std::cerr << "[XgtServer Debug] Parsed var_name: \"" << var_name << "\" (len=" << var_len << ")" << std::endl;
    std::cerr << "  Bytes: ";
    for (size_t i = 0; i < var_len; ++i) {
        std::cerr << std::hex << (int)(uint8_t)var_name[i] << " ";
    }
    std::cerr << std::dec << std::endl;
    
    // Resolve address to LS native areas
    char area_char = 0;
    bool is_bit = false;
    uint16_t index = 0;
    uint16_t sub_index = 0;
    bool resolve_success = resolveAddress(var_name, area_char, is_bit, index, sub_index);

    std::vector<uint8_t> resp_data;
    uint16_t error_status = 0x0000;

    if (command == 0x0054) {
        // --- READ COMMAND ---
        if (!resolve_success) {
            error_status = 0x1102; // Error status: Address parsing error
        } else {
            resp_data.resize(2); // For data size (2 bytes)
            if (is_bit) {
                writeUint16LE(&resp_data[0], 1); // 1 byte size
                
                // Read bit value
                bool bit_val = false;
                uint16_t flat_idx = index * 8 + sub_index;
                std::string area_str(1, area_char);
                bit_val = memory_.readLSBit(area_str, flat_idx);
                resp_data.push_back(bit_val ? 0x01 : 0x00);
            } else {
                writeUint16LE(&resp_data[0], 2); // 2 bytes size (Word)
                
                uint16_t val = memory_.readLSWord(index);
                resp_data.resize(4);
                writeUint16LE(&resp_data[2], val);
            }
        }
    } 
    else if (command == 0x0058) {
        // --- WRITE COMMAND ---
        size_t write_len_offset = 30 + var_len;
        if (request.size() < write_len_offset + 2) {
            error_status = 0x1102; // Length error
        } else if (!resolve_success) {
            error_status = 0x1102; // Address parsing error
        } else {
            uint16_t write_data_len = readUint16LE(&request[write_len_offset]);
            
            // PyXGT write_data_len bug workaround
            if (write_data_len == 0x0020) {
                write_data_len = 2;
            }
            
            if (request.size() < write_len_offset + 2 + write_data_len) {
                error_status = 0x1102;
            } else {
                // Perform write to memory
                if (is_bit) {
                    uint16_t flat_idx = index * 8 + sub_index;
                    bool val = request[write_len_offset + 2] != 0;
                    std::string area_str(1, area_char);
                    memory_.writeLSBit(area_str, flat_idx, val);
                } else {
                    uint16_t val = readUint16LE(&request[write_len_offset + 2]);
                    memory_.writeLSWord(index, val);
                }
            }
        }
    } else {
        error_status = 0x00FF; // Unknown command
    }

    // Construct final response packet
    std::vector<uint8_t> response(30 + resp_data.size()); // header(20) + response body(10 + resp_data)
    
    // 1. Copy Company ID & Base fields (0-13)
    std::memcpy(response.data(), request.data(), 12);
    response[12] = request[12]; // CPU Info
    response[13] = 0x11;        // Response direction (0x11)
    
    // 2. Invoke ID
    response[14] = request[14];
    response[15] = request[15];
    
    // 3. Length of response remaining payload starting from index 18 (FEnet Position)
    writeUint16LE(&response[16], static_cast<uint16_t>(12 + resp_data.size()));
    
    // 4. Routing block (FEnet Position, BCC) at index 18-19
    response[18] = request[18]; // FEnet Position
    response[19] = request[19]; // BCC (just copy from request)
    
    // 5. Command Response code at index 20-21
    uint16_t resp_cmd = (command == 0x0054) ? 0x0055 : 0x0059;
    writeUint16LE(&response[20], resp_cmd);
    
    // 6. Data Type at index 22-23
    writeUint16LE(&response[22], data_type);
    
    // 7. Reserved at index 24-25
    response[24] = 0x00;
    response[25] = 0x00;
    
    // 8. Error Status at index 26-27
    writeUint16LE(&response[26], error_status);
    
    // 9. Block Count at index 28-29 (1 block)
    writeUint16LE(&response[28], 1);
    
    // 10. Copy response data starting from index 30
    if (!resp_data.empty()) {
        std::memcpy(response.data() + 30, resp_data.data(), resp_data.size());
    }

    return response;
}

bool XgtServer::resolveAddress(const std::string& addr_str, char& area_char, bool& is_bit, uint16_t& index, uint16_t& sub_index) {
    if (addr_str.size() < 4) return false;
    
    size_t start = 0;
    if (addr_str[0] == '%') {
        start = 1;
    }
    
    if (addr_str.size() - start < 3) return false;
    
    char area = addr_str[start];
    char type = addr_str[start + 1];
    
    if (area == 'I' || area == 'Q' || area == 'M') {
        area_char = area;
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
            index = static_cast<uint16_t>(nums[0]);
            sub_index = 0;
        } else if (nums.size() == 2) {
            index = static_cast<uint16_t>(nums[0]);
            sub_index = static_cast<uint16_t>(nums[1]);
        } else {
            index = static_cast<uint16_t>(nums[nums.size() - 2]);
            sub_index = static_cast<uint16_t>(nums[nums.size() - 1]);
        }
    } else {
        try {
            index = static_cast<uint16_t>(std::stoi(number_part));
            sub_index = 0;
        } catch (...) {
            return false;
        }
    }
    
    return true;
}
