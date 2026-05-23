#include <cstdint>

// Export MatIEC symbols with extern "C" to disable C++ name mangling
extern "C" {
    // Discrete Inputs (%IX0.x)
    uint8_t __IX0_0 = 0;
    
    // Coils / Outputs (%QX0.x)
    uint8_t __QX0_0 = 0;
    uint8_t __QX0_1 = 0;
    
    // Input Registers (%IWx)
    uint16_t __IW0 = 0;
    
    // Holding Registers (%MWx)
    uint16_t __MW0 = 0;
    uint16_t __MW1 = 0;
    
    // 1. PLC Initialization Hook
    void config_init__(void) {
        __IX0_0 = 0;
        __QX0_0 = 0;
        __QX0_1 = 0;
        __IW0 = 0;
        __MW0 = 100; // Start counter at 100
        __MW1 = 0;
    }
    
    // 2. PLC Scan Cycle Logic Hook (called every scan cycle)
    void config_run__(unsigned long tick) {
        // --- LOGIC 1: Conditional Switch ---
        // If Input 0 is ON (%IX0.0), turn ON Output 0 (%QX0.0) and increment Holding Reg 0 (%MW0)
        if (__IX0_0) {
            __QX0_0 = 1;
            __MW0++;
        } else {
            __QX0_0 = 0;
        }
        
        // --- LOGIC 2: Standard Blinker (Oscillator) ---
        // Toggle Output 1 (%QX0.1) every 25 ticks (simulates a 500ms blinker at 20ms scan time)
        if (tick % 25 == 0) {
            __QX0_1 = !__QX0_1;
        }
        
        // --- LOGIC 3: Analog Math ---
        // Read Input Register 0 (%IW0), double it, and write to Holding Register 1 (%MW1)
        __MW1 = __IW0 * 2;
    }
}
