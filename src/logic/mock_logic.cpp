#include <cstdint>

// Export MatIEC symbols with extern "C" to disable C++ name mangling
extern "C" {
    // Discrete Inputs (%IX0.x)
    uint8_t __IX0_0 = 0; // Auto Mode Switch (1 = Auto, 0 = Manual)
    uint8_t __IX0_1 = 0; // Low Limit Sensor (<= 150mm)
    uint8_t __IX0_2 = 0; // High Limit Sensor (>= 850mm)
    
    // Coils / Outputs (%QX0.x)
    uint8_t __QX0_0 = 0; // Inlet Valve Actuator (Pump)
    uint8_t __QX0_1 = 0; // Outlet Valve Actuator
    uint8_t __QX0_2 = 0; // System Run Lamp
    
    // Input Registers (%IWx)
    uint16_t __IW0 = 0;  // Current Water Level (mm, 0 to 1000)
    
    // Holding Registers (%MWx)
    uint16_t __MW0 = 0;  // Cumulative pump activation counter
    uint16_t __MW1 = 0;  // Target Set Point (mm)
    
    // Internal simulator states (persist across dlopen/dlsym cycles as global symbols)
    double level_sim = 500.0;
    uint8_t prev_inlet_valve = 0;
    uint8_t auto_state = 0; // 0 = Standby/Draining, 1 = Filling

    // 1. PLC Initialization Hook
    void config_init__(void) {
        __IX0_0 = 0;
        __IX0_1 = 0;
        __IX0_2 = 0;
        __QX0_0 = 0;
        __QX0_1 = 0;
        __QX0_2 = 0;
        __IW0 = 500;
        __MW0 = 0;
        __MW1 = 500; // Default target set point

        level_sim = 500.0;
        prev_inlet_valve = 0;
        auto_state = 0;
    }
    
    // 2. PLC Scan Cycle Logic Hook (called every 20ms scan cycle)
    void config_run__(unsigned long tick) {
        // --- 1. PHYSICAL SIMULATION (Water Tank Level model) ---
        // Inlet Valve (+20 mm/s) -> 20ms cycle: +0.4 mm
        // Outlet Valve (-15 mm/s) -> 20ms cycle: -0.3 mm
        // Natural drain (-1 mm/s) -> 20ms cycle: -0.02 mm
        double change = -0.02; // Always some natural minor leakage/drain
        if (__QX0_0) {
            change += 0.4;
        }
        if (__QX0_1) {
            change -= 0.3;
        }
        
        level_sim += change;
        
        // Clamp level within [0.0, 1000.0] mm
        if (level_sim < 0.0) level_sim = 0.0;
        if (level_sim > 1000.0) level_sim = 1000.0;
        
        // Sync calculated level back to Input Register %IW0
        __IW0 = static_cast<uint16_t>(level_sim + 0.5); // Round to nearest int
        
        // Update Low and High Limit sensors based on water level
        __IX0_1 = (level_sim <= 150.0) ? 1 : 0;
        __IX0_2 = (level_sim >= 850.0) ? 1 : 0;
        
        // --- 2. PLC CONTROL LOGIC ---
        if (__IX0_0) {
            // --- AUTO MODE ---
            // If water level drops to low limit, turn on Inlet Valve and close Outlet Valve
            if (__IX0_1) {
                auto_state = 1; // Change state to Filling
            }
            // If water level reaches high limit, close Inlet Valve and open Outlet Valve
            else if (__IX0_2) {
                auto_state = 0; // Change state to Standby/Draining
            }
            
            // Apply states based on auto hysteresis state
            if (auto_state == 1) {
                __QX0_0 = 1; // Fill
                __QX0_1 = 0;
            } else {
                __QX0_0 = 0; // Drain
                __QX0_1 = 1;
            }
        } else {
            // --- MANUAL MODE ---
            // In manual mode, we do not override QX0_0/QX0_1. 
            // Valve actuators remain under direct external or manual control (Modbus, S7, or TUI force).
            auto_state = (__QX0_0) ? 1 : 0; // Sync hysteresis state
        }
        
        // Edge detection on Inlet Valve to increment cumulative activation counter
        if (__QX0_0 && !prev_inlet_valve) {
            __MW0++;
        }
        prev_inlet_valve = __QX0_0;
        
        // Keep Run Lamp ON to indicate PLC is scanning
        __QX0_2 = 1;
    }
}

