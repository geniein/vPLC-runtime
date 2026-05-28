#include <cstdint>

// Export MatIEC symbols with extern "C" to disable C++ name mangling
extern "C" {
    // Discrete Inputs (%IX0.x)
    uint8_t __IX0_0 = 0; // Auto Mode Switch (1 = Auto, 0 = Manual)
    uint8_t __IX0_1 = 0; // Chassis Present Sensor (At Assembly Station: 500mm)
    uint8_t __IX0_2 = 0; // Part Present Sensor (1 = Part ready at pick station)
    uint8_t __IX0_3 = 0; // Robot Arm Home Position Sensor (Lift Up + Rotate Left)
    uint8_t __IX0_4 = 0; // Part Clamped Sensor
    uint8_t __IX0_5 = 0; // Robot Arm Rotated Right (Over assembly station) Sensor
    uint8_t __IX0_6 = 0; // Robot Arm Lift Down Sensor
    uint8_t __IX0_7 = 0; // Safety Light Curtain Intrusion Sensor (1 = Emergency Stop, 0 = Safe)
    
    // Coils / Outputs (%QX0.x)
    uint8_t __QX0_0 = 0; // Conveyor Run (1 = Run, 0 = Stop)
    uint8_t __QX0_1 = 0; // Robot Lift Down Solenoid
    uint8_t __QX0_2 = 0; // Robot Clamp Solenoid
    uint8_t __QX0_3 = 0; // Robot Rotate Right Solenoid
    uint8_t __QX0_4 = 0; // Assembly Done Indicator
    uint8_t __QX0_5 = 0; // System Run Lamp (Green)
    uint8_t __QX0_6 = 0; // System Warning Lamp (Red Alarm)
    
    // Input Registers (%IWx)
    uint16_t __IW0 = 0;  // Conveyor Position (0 to 1000 mm)
    
    // Holding Registers (%MWx)
    uint16_t __MW0 = 0;  // Completed Car Count
    uint16_t __MW1 = 200; // Conveyor Speed Set Point (mm/s, default 200)
    uint16_t __MW2 = 0;   // Chassis Present Force Switch (1 = force present)
    uint16_t __MW3 = 0;   // Part Present Force Switch (1 = force part present)
    
    // Path identifier so that TUI knows which simulator is active
    const char* PLC_SIMULATOR_TYPE = "AUTOMOTIVE_LINE";

    // Internal simulation states
    double conveyor_pos_sim = 0.0;
    double lift_pos_sim = 0.0;   // 0.0 = Up (Home), 1.0 = Down
    double rotate_pos_sim = 0.0; // 0.0 = Left (Home), 1.0 = Right
    double clamp_pos_sim = 0.0;  // 0.0 = Release (Home), 1.0 = Clamped
    
    int sfc_state = 0;           // Sequence state (0 to 7)
    uint8_t alarm_flash = 0;

    void config_init__(void) {
        __IX0_0 = 1; // Default to Auto Mode enabled for silent background running
        __IX0_1 = 0;
        __IX0_2 = 1; // Part initially ready at pick station
        __IX0_3 = 1; // Starts at home
        __IX0_4 = 0;
        __IX0_5 = 0;
        __IX0_6 = 0;
        __IX0_7 = 0; // Safe initially
        
        __QX0_0 = 0;
        __QX0_1 = 0;
        __QX0_2 = 0;
        __QX0_3 = 0;
        __QX0_4 = 0;
        __QX0_5 = 0;
        __QX0_6 = 0;
        
        __IW0 = 0;
        __MW0 = 0;
        __MW1 = 200; // Default speed 200mm/s
        __MW2 = 0;
        __MW3 = 0;

        conveyor_pos_sim = 0.0;
        lift_pos_sim = 0.0;
        rotate_pos_sim = 0.0;
        clamp_pos_sim = 0.0;
        sfc_state = 0;
        alarm_flash = 0;
    }
    
    void config_run__(unsigned long tick) {
        // --- 1. PHYSICAL SIMULATION ---
        
        // Handle Safety Intrusion (Light Curtain Blocked)
        bool e_stop = (__IX0_7 != 0);
        
        if (e_stop) {
            // Force-cut all actuator outputs immediately for safety!
            __QX0_0 = 0; // Conveyor Stop
            __QX0_1 = 0; // Lift Solenoid OFF (retracts up)
            // Clamp remains activated to prevent dropping the part!
            __QX0_3 = 0; // Rotate Solenoid OFF (returns left)
            __QX0_4 = 0; // Assembly Done indicator OFF
            __QX0_5 = 0; // Run Lamp OFF
            
            // Flash Alarm Lamp every 10 ticks (200ms)
            if (tick % 10 == 0) {
                alarm_flash = !alarm_flash;
            }
            __QX0_6 = alarm_flash ? 1 : 0;
        } else {
            __QX0_5 = 1; // System Run green lamp ON
            __QX0_6 = 0; // Warning lamp OFF
        }

        // Conveyor movement physics (VFD speed in MW1)
        if (__QX0_0 && !e_stop) {
            double speed_per_tick = (__MW1 * 0.02); // 20ms tick
            conveyor_pos_sim += speed_per_tick;
            
            // Check if chassis has reached assembly station (500 mm)
            if (conveyor_pos_sim >= 500.0 && conveyor_pos_sim < 520.0) {
                conveyor_pos_sim = 500.0; // Stop exactly at station
                __IX0_1 = 1;              // Chassis Present
            } else {
                __IX0_1 = 0;
            }
            
            // Check if chassis completed transit (1000 mm)
            if (conveyor_pos_sim >= 1000.0) {
                conveyor_pos_sim = 0.0; // Reset for next chassis
                __MW0++;                // Increment Completed Car Count!
            }
        }
        __IW0 = static_cast<uint16_t>(conveyor_pos_sim + 0.5);

        // Cylinder Lift Up/Down physics (Takes ~500ms to fully travel -> 25 ticks -> 0.04 change)
        if (__QX0_1 && !e_stop) {
            lift_pos_sim += 0.04;
        } else {
            lift_pos_sim -= 0.04;
        }
        if (lift_pos_sim < 0.0) lift_pos_sim = 0.0;
        if (lift_pos_sim > 1.0) lift_pos_sim = 1.0;
        __IX0_6 = (lift_pos_sim >= 0.95) ? 1 : 0; // Down limit sensor

        // Cylinder Rotate Left/Right physics (Takes ~600ms to travel -> 30 ticks -> 0.033 change)
        if (__QX0_3 && !e_stop) {
            rotate_pos_sim += 0.033;
        } else {
            rotate_pos_sim -= 0.033;
        }
        if (rotate_pos_sim < 0.0) rotate_pos_sim = 0.0;
        if (rotate_pos_sim > 1.0) rotate_pos_sim = 1.0;
        __IX0_5 = (rotate_pos_sim >= 0.95) ? 1 : 0; // Right limit sensor

        // Cylinder Clamp Open/Close physics (Takes ~200ms -> 10 ticks -> 0.1 change)
        if (__QX0_2) { // Clamp keeps holding even during e-stop
            clamp_pos_sim += 0.1;
        } else {
            clamp_pos_sim -= 0.1;
        }
        if (clamp_pos_sim < 0.0) clamp_pos_sim = 0.0;
        if (clamp_pos_sim > 1.0) clamp_pos_sim = 1.0;
        __IX0_4 = (clamp_pos_sim >= 0.95) ? 1 : 0; // Clamped sensor

        // Home Position Sensor (Lift is Up AND Rotate is Left)
        __IX0_3 = (lift_pos_sim <= 0.05 && rotate_pos_sim <= 0.05) ? 1 : 0;

        // Auto-generation of material parts at pick station
        if ((__IX0_3 && !__QX0_2) || __MW3 == 1) {
            __IX0_2 = 1; // Part ready at pick station
        }

        // --- 2. PLC SEQUENCE CONTROL LOGIC (SFC) ---
        if (__IX0_0 && !e_stop) {
            switch (sfc_state) {
                case 0: // State 0: Wait for chassis entry trigger
                    __QX0_0 = 0; // Stop Conveyor initially (Wait for trigger)
                    __QX0_1 = 0; // Lift UP
                    __QX0_2 = 0; // Clamp OFF
                    __QX0_3 = 0; // Rotate LEFT
                    __QX0_4 = 0; // Done Lamp OFF
                    __IX0_1 = 0; // Reset Chassis Present
                    
                    if (__MW2 == 1) { // External Chassis provision trigger received
                        __MW2 = 0; // Clear the trigger immediately
                        conveyor_pos_sim = 0.0; // Reset position to start
                        __QX0_0 = 1; // Start Conveyor
                        sfc_state = 8; // Move to dynamic transit conveying state
                    }
                    break;

                case 8: // State 8: Conveying Chassis to Assembly Station
                    __QX0_0 = 1; // Keep running conveyor
                    if (conveyor_pos_sim >= 500.0) {
                        conveyor_pos_sim = 500.0;
                        __IX0_1 = 1; // Chassis Present Sensor Activated
                        __QX0_0 = 0; // Stop Conveyor exactly at station
                        if (__IX0_2) { // Part ready at pick station
                            sfc_state = 1; // Proceed to assembly logic
                        }
                    }
                    break;
                    
                case 1: // State 1: Move lift down to pick part
                    __QX0_1 = 1; // Lift DOWN
                    if (__IX0_6) { // Arm reached down limit
                        sfc_state = 2; // Move to Clamp
                    }
                    break;
                    
                case 2: // State 2: Clamp part
                    __QX0_2 = 1; // Clamp ON
                    if (__IX0_4) { // Part clamped successfully
                        __IX0_2 = 0;   // Part taken from pick station
                        sfc_state = 3; // Move to Lift Up
                    }
                    break;
                    
                case 3: // State 3: Lift up with part
                    __QX0_1 = 0; // Lift UP
                    if (lift_pos_sim <= 0.05) { // Reached top
                        sfc_state = 4; // Move to Rotate
                    }
                    break;
                    
                case 4: // State 4: Rotate to Assembly station
                    __QX0_3 = 1; // Rotate RIGHT
                    if (__IX0_5) { // Reached assembly station
                        sfc_state = 5; // Move to Place Down
                    }
                    break;
                    
                case 5: // State 5: Lift down to place part on chassis
                    __QX0_1 = 1; // Lift DOWN
                    if (__IX0_6) { // Reached down
                        sfc_state = 6; // Move to Release
                    }
                    break;
                    
                case 6: // State 6: Release part
                    __QX0_2 = 0; // Clamp OFF
                    if (clamp_pos_sim <= 0.05) { // Gripper fully open
                        __QX0_4 = 1;   // Assembly Done indicator ON
                        sfc_state = 7; // Move to Return Home
                    }
                    break;
                    
                case 7: // State 7: Return robotic arm to home position
                    __QX0_1 = 0; // Lift UP
                    __QX0_3 = 0; // Rotate LEFT
                    
                    if (__IX0_3) { // Robot back home (Up + Left)
                        __QX0_4 = 0; // Done Lamp OFF
                        
                        // Push conveyor slightly past the station sensor to release Chassis Present
                        conveyor_pos_sim = 525.0; 
                        __IX0_1 = 0; 
                        
                        sfc_state = 0; // Return to 이송 대기
                    }
                    break;
            }
        } else if (!__IX0_0) {
            // Manual Mode: Reset state machine, variables are manipulated externally
            sfc_state = 0;
        }
    }
}
