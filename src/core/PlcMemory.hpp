#ifndef PLC_MEMORY_HPP
#define PLC_MEMORY_HPP

#include <vector>
#include <cstdint>
#include <shared_mutex>
#include <mutex>
#include <optional>

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

    // Debug & UI info
    size_t getCoilsSize() const { return coils_.size(); }
    size_t getDiscreteInputsSize() const { return discrete_inputs_.size(); }
    size_t getInputRegistersSize() const { return input_registers_.size(); }
    size_t getHoldingRegistersSize() const { return holding_registers_.size(); }

private:
    // Core memory arrays
    std::vector<bool> coils_;
    std::vector<bool> discrete_inputs_;
    std::vector<uint16_t> input_registers_;
    std::vector<uint16_t> holding_registers_;

    // Mutex for thread-safety (multiple reads, single write)
    mutable std::shared_mutex mutex_;
};

#endif // PLC_MEMORY_HPP
