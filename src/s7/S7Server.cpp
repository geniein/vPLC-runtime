#include "S7Server.hpp"
#include <iostream>
#include <cstring>

S7Server::S7Server(PlcMemory& memory, const std::string& ip_address, uint16_t port)
    : memory_(memory),
      ip_address_(ip_address),
      port_(port),
      s7_server_(0),
      is_running_(false),
      pe_buffer_(256, 0),
      pa_buffer_(256, 0),
      db1_buffer_(2048, 0),
      db1_shadow_buffer_(2048, 0) {
    s7_server_ = Srv_Create();
}

S7Server::~S7Server() {
    stop();
    if (s7_server_ != 0) {
        Srv_Destroy(&s7_server_);
    }
}

bool S7Server::start() {
    if (is_running_) return true;

    // Set port parameter
    // p_u16_LocalPort is parameter number 1 in Snap7 Server
    uint16_t port_param = port_;
    int res = Srv_SetParam(s7_server_, p_u16_LocalPort, &port_param);
    if (res != 0) {
        std::cerr << "[S7Server] Error setting server port: " << res << std::endl;
        return false;
    }

    // Register PLC Memory Areas in Snap7
    // srvAreaPE (0) = Process Inputs
    res = Srv_RegisterArea(s7_server_, srvAreaPE, 0, pe_buffer_.data(), pe_buffer_.size());
    if (res != 0) {
        std::cerr << "[S7Server] Error registering PE (Inputs) area: " << res << std::endl;
        return false;
    }

    // srvAreaPA (1) = Process Outputs
    res = Srv_RegisterArea(s7_server_, srvAreaPA, 0, pa_buffer_.data(), pa_buffer_.size());
    if (res != 0) {
        std::cerr << "[S7Server] Error registering PA (Outputs) area: " << res << std::endl;
        return false;
    }

    // srvAreaDB (5) = Data Blocks. Register Data Block 1 (DB1)
    res = Srv_RegisterArea(s7_server_, srvAreaDB, 1, db1_buffer_.data(), db1_buffer_.size());
    if (res != 0) {
        std::cerr << "[S7Server] Error registering DB1 area: " << res << std::endl;
        return false;
    }

    // Start server
    res = Srv_StartTo(s7_server_, ip_address_.c_str());
    if (res != 0) {
        std::cerr << "[S7Server] Error starting S7 server: " << res << std::endl;
        std::cerr << "[S7Server] Tip: Port 102 requires root/sudo. Make sure no other server (e.g. Siemens PLC Sim) is running on port 102." << std::endl;
        return false;
    }

    is_running_ = true;
    sync_thread_ = std::thread(&S7Server::syncLoop, this);
    std::cout << "[S7Server] Siemens S7 Server started on " << ip_address_ << ":" << port_ << std::endl;
    return true;
}

void S7Server::stop() {
    if (!is_running_) return;

    is_running_ = false;
    Srv_Stop(s7_server_);

    if (sync_thread_.joinable()) {
        sync_thread_.join();
    }
    std::cout << "[S7Server] Siemens S7 Server stopped" << std::endl;
}

void S7Server::syncLoop() {
    while (is_running_) {
        syncBeforeCycle();
        syncAfterCycle();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

int S7Server::getClientsCount() const {
    if (s7_server_ == 0) return 0;
    
    int srv_status = 0;
    int cpu_status = 0;
    int clients_count = 0;
    
    int res = Srv_GetStatus(s7_server_, &srv_status, &cpu_status, &clients_count);
    if (res != 0) return 0;
    
    return clients_count;
}

void S7Server::syncBeforeCycle() {
    if (!is_running_) return;

    // Detect S7 Client Write Actions on DB1
    // We compare db1_buffer_ with db1_shadow_buffer_ to see if S7 client modified DB1
    for (size_t i = 0; i < 1024; ++i) {
        size_t byte_idx = i * 2;
        
        // S7 client writes in big-endian
        uint8_t curr_h = db1_buffer_[byte_idx];
        uint8_t curr_l = db1_buffer_[byte_idx + 1];
        
        uint8_t shad_h = db1_shadow_buffer_[byte_idx];
        uint8_t shad_l = db1_shadow_buffer_[byte_idx + 1];

        if (curr_h != shad_h || curr_l != shad_l) {
            // S7 client has modified this register!
            uint16_t host_val = (static_cast<uint16_t>(curr_h) << 8) | curr_l;
            memory_.writeHoldingRegister(i, host_val);
            
            // Update shadow buffer
            db1_shadow_buffer_[byte_idx] = curr_h;
            db1_shadow_buffer_[byte_idx + 1] = curr_l;
        }
    }
}

void S7Server::syncAfterCycle() {
    if (!is_running_) return;

    // 1. Sync PlcMemory Discrete Inputs to S7 PE buffer
    for (size_t i = 0; i < 2048; ++i) {
        bool val = memory_.readDiscreteInput(i);
        size_t byte_idx = i / 8;
        size_t bit_idx = i % 8;
        if (val) {
            pe_buffer_[byte_idx] |= (1 << bit_idx);
        } else {
            pe_buffer_[byte_idx] &= ~(1 << bit_idx);
        }
    }

    // 2. Sync PlcMemory Coils (Outputs) to S7 PA buffer
    for (size_t i = 0; i < 2048; ++i) {
        bool val = memory_.readCoil(i);
        size_t byte_idx = i / 8;
        size_t bit_idx = i % 8;
        if (val) {
            pa_buffer_[byte_idx] |= (1 << bit_idx);
        } else {
            pa_buffer_[byte_idx] &= ~(1 << bit_idx);
        }
    }

    // 3. Sync PlcMemory Holding Registers to S7 DB1 buffer (and shadow)
    for (size_t i = 0; i < 1024; ++i) {
        uint16_t val = memory_.readHoldingRegister(i);
        size_t byte_idx = i * 2;
        
        // Convert host-endian to big-endian bytes
        uint8_t h = static_cast<uint8_t>((val >> 8) & 0xFF);
        uint8_t l = static_cast<uint8_t>(val & 0xFF);

        db1_buffer_[byte_idx] = h;
        db1_buffer_[byte_idx + 1] = l;

        db1_shadow_buffer_[byte_idx] = h;
        db1_shadow_buffer_[byte_idx + 1] = l;
    }

    // 4. Mirror Input Register 0 (%IW0, Conveyor position) to DB1 buffer (Offset 128)
    uint16_t pos_val = memory_.readInputRegister(0);
    db1_buffer_[128] = static_cast<uint8_t>((pos_val >> 8) & 0xFF);
    db1_buffer_[129] = static_cast<uint8_t>(pos_val & 0xFF);
    db1_shadow_buffer_[128] = db1_buffer_[128];
    db1_shadow_buffer_[129] = db1_buffer_[129];

    // 5. Mirror Coils 0..7 (%QX0.0..%QX0.7, Conveyor run and Robot states) to DB1 buffer (Offset 272)
    uint8_t coils_byte = 0;
    for (size_t bit = 0; bit < 8; ++bit) {
        if (memory_.readCoil(bit)) {
            coils_byte |= (1 << bit);
        }
    }
    db1_buffer_[272] = coils_byte;
    db1_shadow_buffer_[272] = coils_byte;
}
