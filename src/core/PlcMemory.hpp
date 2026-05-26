#ifndef PLC_MEMORY_HPP
#define PLC_MEMORY_HPP

#include <vector>
#include <cstdint>
#include <shared_mutex>
#include <mutex>
#include <optional>
#include <string>

// Structure representing an address reference
struct MemoryRef {
    std::string protocol; // "MODBUS", "S7", "MC", "LS"
    std::string area;     // "COIL", "DI", "IR", "HR", "PE", "PA", "DB1", "DB2", "X", "Y", "M", "L", "B", "F", "SM", "D", "W", "R", "ZR", "SD", "I", "Q"
    uint16_t index;       // 0-based offset
    bool is_bit;          // true if bit, false if 16-bit word
};

// Structure representing a single mapping rule
struct MappingRule {
    std::string src_raw;
    std::string dst_raw;
    MemoryRef src;
    MemoryRef dst;

    // Track previous values for bidirectional stateful change detection
    uint16_t last_src_val = 0;
    uint16_t last_dst_val = 0;
    bool initialized = false;
};

class PlcMemory {
public:
    // Memory dimensions
    static constexpr size_t DEFAULT_COILS_SIZE = 2048;
    static constexpr size_t DEFAULT_DISCRETE_INPUTS_SIZE = 2048;
    static constexpr size_t DEFAULT_INPUT_REGISTERS_SIZE = 1024;
    static constexpr size_t DEFAULT_HOLDING_REGISTERS_SIZE = 1024;

    PlcMemory(size_t coils_size = DEFAULT_COILS_SIZE,
              size_t discrete_inputs_size = DEFAULT_DISCRETE_INPUTS_SIZE,
              size_t input_registers_size = DEFAULT_INPUT_REGISTERS_SIZE,
              size_t holding_registers_size = DEFAULT_HOLDING_REGISTERS_SIZE);

    ~PlcMemory() = default;

    // --- Coils (Read/Write, 0-based index) ---
    bool readCoil(uint16_t address) const;
    std::vector<bool> readCoils(uint16_t start_address, uint16_t count) const;
    void writeCoil(uint16_t address, bool value);
    void writeCoils(uint16_t start_address, const std::vector<bool>& values);

    // --- Discrete Inputs (Read-Only, 0-based index) ---
    bool readDiscreteInput(uint16_t address) const;
    std::vector<bool> readDiscreteInputs(uint16_t start_address, uint16_t count) const;
    void writeDiscreteInput(uint16_t address, bool value); // Internal simulation can write to it
    void writeDiscreteInputs(uint16_t start_address, const std::vector<bool>& values);

    // --- Input Registers (Read-Only 16-bit, 0-based index) ---
    uint16_t readInputRegister(uint16_t address) const;
    std::vector<uint16_t> readInputRegisters(uint16_t start_address, uint16_t count) const;
    void writeInputRegister(uint16_t address, uint16_t value); // Internal simulation can write to it
    void writeInputRegisters(uint16_t start_address, const std::vector<uint16_t>& values);

    // --- Holding Registers (Read/Write 16-bit, 0-based index) ---
    uint16_t readHoldingRegister(uint16_t address) const;
    std::vector<uint16_t> readHoldingRegisters(uint16_t start_address, uint16_t count) const;
    void writeHoldingRegister(uint16_t address, uint16_t value);
    void writeHoldingRegisters(uint16_t start_address, const std::vector<uint16_t>& values);

    // --- Siemens S7 Native Buffers Getters/Setters ---
    bool readS7PE(uint16_t address) const;
    void writeS7PE(uint16_t address, bool value);
    bool readS7PA(uint16_t address) const;
    void writeS7PA(uint16_t address, bool value);
    uint16_t readS7DB1(uint16_t address) const;
    void writeS7DB1(uint16_t address, uint16_t value);
    uint16_t readS7DB2(uint16_t address) const;
    void writeS7DB2(uint16_t address, uint16_t value);

    // --- Mitsubishi MC Native Buffers Getters/Setters ---
    bool readMCBit(const std::string& area, uint16_t address) const;
    void writeMCBit(const std::string& area, uint16_t address, bool value);
    uint16_t readMCWord(const std::string& area, uint16_t address) const;
    void writeMCWord(const std::string& area, uint16_t address, uint16_t value);

    // --- LS Electric (XGT) Native Buffers Getters/Setters ---
    bool readLSBit(const std::string& area, uint16_t address) const;
    void writeLSBit(const std::string& area, uint16_t address, bool value);
    uint16_t readLSWord(uint16_t address) const;
    void writeLSWord(uint16_t address, uint16_t value);

    // --- Dynamic Mapping Engine ---
    bool loadMappings(const std::string& path = "mappings.json");
    void syncMappings();
    std::vector<MappingRule> getMappings() const;

    // Helper functions for reading and writing memory dynamically via MemoryRef
    uint16_t readValue(const MemoryRef& ref) const;
    void writeValue(const MemoryRef& ref, uint16_t value);
    MemoryRef parseAddress(const std::string& addr_str) const;

    // Debug & UI info
    size_t getCoilsSize() const { return coils_.size(); }
    size_t getDiscreteInputsSize() const { return discrete_inputs_.size(); }
    size_t getInputRegistersSize() const { return input_registers_.size(); }
    size_t getHoldingRegistersSize() const { return holding_registers_.size(); }

private:
    // Core memory arrays (Modbus standard mapping)
    std::vector<bool> coils_;
    std::vector<bool> discrete_inputs_;
    std::vector<uint16_t> input_registers_;
    std::vector<uint16_t> holding_registers_;

    // Siemens S7 native buffers
    std::vector<bool> s7_pe_;  // Process Inputs
    std::vector<bool> s7_pa_;  // Process Outputs
    std::vector<uint16_t> s7_db1_; // DB1 Data Block
    std::vector<uint16_t> s7_db2_; // DB2 Data Block

    // Mitsubishi MC native buffers
    std::vector<bool> mc_x_;
    std::vector<bool> mc_y_;
    std::vector<bool> mc_m_;
    std::vector<bool> mc_l_;
    std::vector<bool> mc_b_;
    std::vector<bool> mc_f_;
    std::vector<bool> mc_sm_;
    std::vector<uint16_t> mc_d_;
    std::vector<uint16_t> mc_w_;
    std::vector<uint16_t> mc_r_;
    std::vector<uint16_t> mc_zr_;
    std::vector<uint16_t> mc_sd_;

    // LS Electric native buffers
    std::vector<bool> ls_i_;
    std::vector<bool> ls_q_;
    std::vector<bool> ls_m_;
    std::vector<uint16_t> ls_w_;

    // Dynamic mapping configurations
    std::vector<MappingRule> mappings_;
    mutable std::shared_mutex mappings_mutex_;

    // Mutex for thread-safety (multiple reads, single write)
    mutable std::shared_mutex mutex_;
};

#endif // PLC_MEMORY_HPP

