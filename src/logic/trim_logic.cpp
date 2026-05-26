#include <cstdint>
#include <string>

// Export MatIEC symbols with extern "C" to disable C++ name mangling
extern "C" {
    // -------------------------------------------------------------------------
    // 1. PLC DISCRETE INPUT REGISTERS (%IX0.x)
    // -------------------------------------------------------------------------
    uint8_t __IX0_0 = 0; // Auto Mode Switch (1 = Auto, 0 = Manual)
    uint8_t __IX0_1 = 0; // Station 1: Body (Chassis) Arrived at Cockpit Docking Station
    uint8_t __IX0_2 = 0; // Station 2: Body Arrived at Glass Glue Dispensing Station
    uint8_t __IX0_3 = 0; // Station 3: Body Arrived at Seat Insertion Station
    uint8_t __IX0_4 = 0; // Station 4: Body Arrived at Final Inspection Station
    uint8_t __IX0_5 = 0; // Emergency Stop / Safety Curtain Intrusion Sensor (1 = E-Stop Active)
    uint8_t __IX0_6 = 0; // Vision Sensor: Front Glass Alignment Complete & OK (1 = Success)
    uint8_t __IX0_7 = 0; // Dispenser Alarm: Urethane Glue Level Low Alert (1 = Refill Required)
    
    // -------------------------------------------------------------------------
    // 2. PLC COIL OUTPUT REGISTERS (%QX0.x)
    // -------------------------------------------------------------------------
    uint8_t __QX0_0 = 0; // Main Overhead Hanger Conveyor Run (1 = Run, 0 = Stop)
    uint8_t __QX0_1 = 0; // Cockpit Module Gantry Docking Cylinder Actuator (1 = Dock, 0 = Home)
    uint8_t __QX0_2 = 0; // Cockpit Bolt Tightening Tool (Nutrunner) Operation (1 = Bolting)
    uint8_t __QX0_3 = 0; // Glass Glue Dispenser & Robotic Urethane Jet Active (1 = Jetting)
    uint8_t __QX0_4 = 0; // Seat Insertion Lifter Cylinder (1 = Descend Seat, 0 = Retract Lifter)
    uint8_t __QX0_5 = 0; // Quality Status Lamp: Pass Indicator (Green Lamp)
    uint8_t __QX0_6 = 0; // Quality Status Lamp: Fail Indicator (Red Siren Alarm)
    uint8_t __QX0_7 = 0; // System Status Lamp: Running Indicator (Yellow Flash Lamp)
    
    // -------------------------------------------------------------------------
    // 3. PLC ANALOG INPUT REGISTERS (%IWx)
    // -------------------------------------------------------------------------
    uint16_t __IW0 = 0;  // Conveyor Current Travel Position (0 to 1000 mm)
    uint16_t __IW1 = 0;  // Cockpit Tightening Torque Feedback (0.1 Nm resolution, Target: 450 -> 45.0 Nm)
    uint16_t __IW2 = 0;  // Glass Pressing Load-Cell Force Feedback (Newton, Target: 150 N)
    uint16_t __IW3 = 0;  // Glue Heater Temperature (°C, Target: 40 °C)
    
    // -------------------------------------------------------------------------
    // 4. PLC SYSTEM HOLDING REGISTERS (%MWx)
    // -------------------------------------------------------------------------
    uint16_t __MW0 = 0;  // Cumulative Total Produced Vehicles Count
    uint16_t __MW1 = 0;  // Cumulative OK Quality Vehicles Count
    uint16_t __MW2 = 0;  // Cumulative NG Quality Vehicles Count (Failures)
    uint16_t __MW3 = 450; // Cockpit Tightening Target Setpoint (0.1 Nm, Default: 450 -> 45.0 Nm)
    uint16_t __MW4 = 0;   // Bypass Station 2 (Glue Station) Switch for Fast Testing (1 = Skip, 0 = Normal)
    
    // Path identifier so that TUI/Web UI knows which simulator is active
    const char* PLC_SIMULATOR_TYPE = "AUTOMOTIVE_TRIM_LINE";

    // -------------------------------------------------------------------------
    // 5. INTERNAL PHYSICAL SIMULATOR STATES
    // -------------------------------------------------------------------------
    double conveyor_pos = 0.0;
    double cockpit_dock_pos = 0.0; // 0.0 = Retracted/Home, 1.0 = Fully Docked
    double bolt_torque = 0.0;      // Current real-time simulated torque
    double glass_glue_disp = 0.0;  // Dispensed volume simulated
    double glass_press_load = 0.0; // Current real-time simulated pressing load
    double seat_lifter_pos = 0.0;  // 0.0 = Up/Home, 1.0 = Fully Inserted Down
    double glue_temp = 20.0;       // Temperature starts at ambient 20.0°C

    int trim_sfc_state = 0;        // Sequential Function Chart (SFC) State Machine
    uint8_t yellow_flash = 0;      // System status yellow lamp status
    uint8_t prev_conveyor_run = 0;

    // -------------------------------------------------------------------------
    // 6. INITIALIZATION HOOK
    // -------------------------------------------------------------------------
    void config_init__(void) {
        __IX0_0 = 1; // Default to Auto Mode enabled
        __IX0_1 = 0;
        __IX0_2 = 0;
        __IX0_3 = 0;
        __IX0_4 = 0;
        __IX0_5 = 0; // E-Stop safe initially
        __IX0_6 = 0; // Vision alignment initial state
        __IX0_7 = 0; // Glue tank level OK
        
        __QX0_0 = 0;
        __QX0_1 = 0;
        __QX0_2 = 0;
        __QX0_3 = 0;
        __QX0_4 = 0;
        __QX0_5 = 0;
        __QX0_6 = 0;
        __QX0_7 = 0;
        
        __IW0 = 0;
        __IW1 = 0;
        __IW2 = 0;
        __IW3 = 20;  // Ambient 20°C
        
        __MW0 = 0;
        __MW1 = 0;
        __MW2 = 0;
        __MW3 = 450; // Default target 45.0 Nm
        __MW4 = 0;

        conveyor_pos = 0.0;
        cockpit_dock_pos = 0.0;
        bolt_torque = 0.0;
        glass_glue_disp = 0.0;
        glass_press_load = 0.0;
        seat_lifter_pos = 0.0;
        glue_temp = 20.0;
        trim_sfc_state = 0;
        yellow_flash = 0;
        prev_conveyor_run = 0;
    }
    
    // -------------------------------------------------------------------------
    // 7. CYCLIC RUN HOOK (Called every 20ms)
    // -------------------------------------------------------------------------
    void config_run__(unsigned long tick) {
        // --- A. EMERGENCY STOP / INTERLOCK LAYERS ---
        bool e_stop = (__IX0_5 != 0);
        
        if (e_stop) {
            // Drop outputs safely during emergency stop, keeping siren active
            __QX0_0 = 0; // Conveyor Stop
            __QX0_1 = 0; // Docking Cylinder Retracted
            __QX0_2 = 0; // Torque Nutrunner Stop
            __QX0_3 = 0; // Glue jet stop
            __QX0_4 = 0; // Seat lifter UP
            __QX0_5 = 0; // Green lamp OFF
            
            // Alarm Siren ON (Red light flash)
            __QX0_6 = (tick % 10 < 5) ? 1 : 0;
            __QX0_7 = 0; // Yellow lamp OFF
            return;
        } else {
            // Siren off unless quality check fails
            if (trim_sfc_state != 7) {
                __QX0_6 = 0; 
            }
            
            // Pulse System Running Yellow Lamp every 500ms (25 ticks)
            if (tick % 25 == 0) {
                yellow_flash = !yellow_flash;
            }
            __QX0_7 = yellow_flash ? 1 : 0;
        }

        // --- B. PHYSICAL SIMULATION LAYER ---

        // 1) Hanger Conveyor Movement (Constant speed 150mm/s -> 3mm per tick)
        if (__QX0_0) {
            conveyor_pos += 3.0;
            
            // Boundary clamping & station triggers
            if (conveyor_pos >= 1000.0) {
                conveyor_pos = 0.0; // Wrap around for next car
            }
            
            // Station 1: Cockpit Area (Travel distance: 200 mm)
            if (conveyor_pos >= 200.0 && conveyor_pos < 210.0) {
                conveyor_pos = 200.0; // Clamp exact spot
                __IX0_1 = 1;          // Vehicle Present at Station 1
            } else {
                __IX0_1 = 0;
            }
            
            // Station 2: Glass Glue Area (Travel distance: 450 mm)
            if (__MW4 != 1) { // Normal mode (Not bypassed)
                if (conveyor_pos >= 450.0 && conveyor_pos < 460.0) {
                    conveyor_pos = 450.0; 
                    __IX0_2 = 1;          // Vehicle Present at Station 2
                } else {
                    __IX0_2 = 0;
                }
            } else {
                __IX0_2 = 0; // Bypass Station 2
            }
            
            // Station 3: Seat Insertion Area (Travel distance: 700 mm)
            if (conveyor_pos >= 700.0 && conveyor_pos < 710.0) {
                conveyor_pos = 700.0;
                __IX0_3 = 1;          // Vehicle Present at Station 3
            } else {
                __IX0_3 = 0;
            }

            // Station 4: Quality Inspection Area (Travel distance: 900 mm)
            if (conveyor_pos >= 900.0 && conveyor_pos < 910.0) {
                conveyor_pos = 900.0;
                __IX0_4 = 1;          // Vehicle Present at Station 4
            } else {
                __IX0_4 = 0;
            }
        }
        __IW0 = static_cast<uint16_t>(conveyor_pos + 0.5);

        // 2) Cockpit Docking Cylinder Travel (Takes ~400ms -> 20 ticks -> 0.05 step)
        if (__QX0_1) {
            cockpit_dock_pos += 0.05;
        } else {
            cockpit_dock_pos -= 0.05;
        }
        if (cockpit_dock_pos < 0.0) cockpit_dock_pos = 0.0;
        if (cockpit_dock_pos > 1.0) cockpit_dock_pos = 1.0;

        // 3) Cockpit Nutrunner Torque Simulation (Target: 45.0 Nm -> 450 feedback)
        if (__QX0_2 && cockpit_dock_pos >= 0.95) {
            // Tightening torque profile (linear rise + noise)
            bolt_torque += 22.5; // reaches ~45.0 Nm in ~400ms
            if (bolt_torque > 452.0) {
                bolt_torque = 450.0 + (tick % 5 - 2); // stable around target with minor jitter
            }
        } else {
            bolt_torque = 0.0;
        }
        __IW1 = static_cast<uint16_t>(bolt_torque + 0.5);

        // 4) Glue Heater PID Simulation (Target: 40 °C)
        // If heater is on, rise temp. If off, cool down to ambient (20°C)
        if (glue_temp < 40.0) {
            glue_temp += 0.15; // slow heater warmup
        } else {
            glue_temp -= 0.05; // ambient cooling loss
        }
        __IW3 = static_cast<uint16_t>(glue_temp + 0.5);

        // 5) Robotic Glass Press Load Cell Simulation (Target: 150 N)
        if (__QX0_3) {
            // Press load ramps up quickly when jet/robot active
            glass_press_load += 10.0;
            if (glass_press_load > 152.0) {
                glass_press_load = 150.0 + (tick % 3 - 1);
            }
            
            // Vision sensor auto alignment completion simulation after pressure stabilizes
            if (glass_press_load >= 145.0) {
                __IX0_6 = 1; // Vision System Alignment Complete
            }
            
            // Simulate slow urethane glue tank volume reduction
            if (tick % 100 == 0) {
                // Occasionally alert low level for test realism
                __IX0_7 = (tick % 1000 >= 800) ? 1 : 0; 
            }
        } else {
            glass_press_load = 0.0;
            __IX0_6 = 0;
        }
        __IW2 = static_cast<uint16_t>(glass_press_load + 0.5);

        // 6) Seat Insertion Lifter Cylinder (Takes ~600ms -> 30 ticks -> 0.033 step)
        if (__QX0_4) {
            seat_lifter_pos += 0.033;
        } else {
            seat_lifter_pos -= 0.033;
        }
        if (seat_lifter_pos < 0.0) seat_lifter_pos = 0.0;
        if (seat_lifter_pos > 1.0) seat_lifter_pos = 1.0;


        // --- C. PLC SEQUENCE CONTROL LOGIC (SFC) ---
        if (__IX0_0) { // Auto Mode Enabled
            switch (trim_sfc_state) {
                case 0: // State 0: Run conveyor to transport vehicle to Cockpit area
                    __QX0_0 = 1; // Conveyor Run
                    __QX0_1 = 0; // Dock OFF
                    __QX0_2 = 0; // Bolting OFF
                    __QX0_3 = 0; // Glue OFF
                    __QX0_4 = 0; // Lifter OFF
                    __QX0_5 = 0; // Pass LED OFF
                    
                    if (__IX0_1) {
                        __QX0_0 = 0; // Stop Conveyor
                        trim_sfc_state = 1; // Transition: Start Docking Cockpit Module
                    }
                    break;
                    
                case 1: // State 1: Dock Cockpit Module Gantry
                    __QX0_1 = 1; // Activate Docking Solenoid
                    if (cockpit_dock_pos >= 0.95) {
                        trim_sfc_state = 2; // Transition: Fully docked, start tightening
                    }
                    break;
                    
                case 2: // State 2: Cockpit Tightening & Torque Nutrunner cycle
                    __QX0_2 = 1; // Nutrunner ON
                    
                    // Wait for tightening torque to reach target setpoint (e.g. 45.0 Nm)
                    if (__IW1 >= __MW3) {
                        __QX0_2 = 0; // Nutrunner OFF
                        __QX0_1 = 0; // Retract Gantry
                        trim_sfc_state = 3; // Transition: Gantry retracted, transport to Glass station
                    }
                    break;
                    
                case 3: // State 3: Transport vehicle to Glass Glue Station
                    __QX0_0 = 1; // Conveyor Run
                    if (__IX0_2) {
                        __QX0_0 = 0; // Stop Conveyor
                        trim_sfc_state = 4; // Transition: Arrived, start Glass fitting
                    } else if (__MW4 == 1) {
                        // Skip Station 2 logic if bypass enabled
                        trim_sfc_state = 5;
                    }
                    break;
                    
                case 4: // State 4: Glass Glue Jetting and robotic press fitting
                    __QX0_3 = 1; // Dispenser/Robot ON
                    
                    // Wait for vision system to confirm glass alignment & load force to stabilize
                    if (__IX0_6 && __IW2 >= 148) {
                        __QX0_3 = 0; // Dispenser/Robot OFF
                        trim_sfc_state = 5; // Transition: Glass fit completed, transport to Seat Station
                    }
                    break;
                    
                case 5: // State 5: Transport vehicle to Seat Insertion Station
                    __QX0_0 = 1; // Conveyor Run
                    if (__IX0_3) {
                        __QX0_0 = 0; // Stop Conveyor
                        trim_sfc_state = 6; // Transition: Arrived, descend Seat Lifter
                    }
                    break;
                    
                case 6: // State 6: Descend Electric Seat Lifter
                    __QX0_4 = 1; // Descend seat
                    if (seat_lifter_pos >= 0.95) {
                        // Simulate seat locked, retract lifter
                        __QX0_4 = 0; // Retract lifter
                        trim_sfc_state = 7; // Transition: Seat lifter retracted, transport to final Inspection
                    }
                    break;
                    
                case 7: // State 7: Transport to Inspection & Quality Decision Cycle
                    __QX0_0 = 1; // Conveyor Run
                    if (__IX0_4) {
                        __QX0_0 = 0; // Stop conveyor at Inspection station
                        
                        // Quality Gate Evaluation:
                        // Cockpit Torque must be close to target 45.0 Nm, and if station 2 wasn't bypassed, glass force was normal
                        bool torque_pass = (__IW1 >= 430 && __IW1 <= 470);
                        bool glass_pass = (__MW4 == 1) ? true : (__IW2 >= 145 && __IW2 <= 155 && __IX0_7 == 0);
                        
                        __MW0++; // Increment total vehicle produced counter
                        
                        if (torque_pass && glass_pass) {
                            __QX0_5 = 1; // Turn on Green PASS Lamp
                            __QX0_6 = 0; // Siren alarm OFF
                            __MW1++;     // Increment OK counter
                        } else {
                            __QX0_5 = 0; // PASS Lamp OFF
                            __QX0_6 = 1; // Siren alarm ON (Quality Defect Siren!)
                            __MW2++;     // Increment NG (defect) counter
                        }
                        
                        trim_sfc_state = 8; // Transition: Decision made, hold to display results
                    }
                    break;
                    
                case 8: // State 8: Quality result hold display (~1 second -> 50 ticks)
                    if (tick % 50 == 0) {
                        __QX0_5 = 0; // Clear lamps
                        __QX0_6 = 0;
                        
                        // Push conveyor slightly past the station sensor to release Chassis Present
                        conveyor_pos = 925.0; 
                        __IX0_4 = 0; 
                        
                        trim_sfc_state = 0; // Cycle restart: Return to transport next chassis
                    }
                    break;
            }
        } else {
            // Manual Mode: Reset state machine, actuators are directly manipulated
            trim_sfc_state = 0;
        }
    }
}
