#include "PlcTui.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <unistd.h>
#include <sys/select.h>
#include <iomanip>

PlcTui::PlcTui(PlcMemory& memory, PlcScheduler& scheduler, ModbusServer& server, S7Server& s7_server, McServer& mc_server)
    : memory_(memory),
      scheduler_(scheduler),
      server_(server),
      s7_server_(s7_server),
      mc_server_(mc_server),
      is_running_(false),
      raw_mode_enabled_(false) {}

PlcTui::~PlcTui() {
    stop();
}

bool PlcTui::start() {
    if (is_running_) return true;

    is_running_ = true;

    // Clear terminal screen and hide cursor
    std::cout << "\033[2J\033[?25l" << std::flush;

    enableRawMode();

    // Start threads
    render_thread_ = std::thread(&PlcTui::renderLoop, this);
    input_thread_ = std::thread(&PlcTui::inputLoop, this);

    return true;
}

void PlcTui::stop() {
    if (!is_running_) return;

    is_running_ = false;

    // Join threads
    if (render_thread_.joinable()) render_thread_.join();
    if (input_thread_.joinable()) input_thread_.join();

    disableRawMode();

    // Restore cursor and clear screen
    std::cout << "\033[?25h\033[2J\033[H" << std::flush;
}

void PlcTui::renderLoop() {
    while (is_running_) {
        drawScreen();
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 10 FPS
    }
}

void PlcTui::inputLoop() {
    while (is_running_) {
        if (kbhit()) {
            char c = '\0';
            if (read(STDIN_FILENO, &c, 1) == 1) {
                if (c == 'q' || c == 'Q') {
                    is_running_ = false;
                    extern std::atomic<bool> should_quit;
                    should_quit = true;
                }
                else if (c == 'i' || c == 'I') {
                    // Toggle Auto Mode Switch (%IX0.0)
                    bool val = memory_.readDiscreteInput(0);
                    memory_.writeDiscreteInput(0, !val);
                }
                else if (c == 'a' || c == 'A') {
                    // Increase Target Set Point (%MW1) by 50mm (max 1000)
                    uint16_t val = memory_.readHoldingRegister(1);
                    if (val + 50 <= 1000) {
                        memory_.writeHoldingRegister(1, val + 50);
                    } else {
                        memory_.writeHoldingRegister(1, 1000);
                    }
                }
                else if (c == 'z' || c == 'Z') {
                    // Decrease Target Set Point (%MW1) by 50mm (min 0)
                    uint16_t val = memory_.readHoldingRegister(1);
                    if (val >= 50) {
                        memory_.writeHoldingRegister(1, val - 50);
                    } else {
                        memory_.writeHoldingRegister(1, 0);
                    }
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Poll input
    }
}

void PlcTui::drawScreen() {
    auto stats = scheduler_.getStats();
    
    // Read variables from PLC memory
    bool auto_mode = memory_.readDiscreteInput(0);
    bool low_limit = memory_.readDiscreteInput(1);
    bool high_limit = memory_.readDiscreteInput(2);
    
    bool inlet_valve = memory_.readCoil(0);
    bool outlet_valve = memory_.readCoil(1);
    bool run_lamp = memory_.readCoil(2);
    
    uint16_t water_level = memory_.readInputRegister(0);
    uint16_t pump_starts = memory_.readHoldingRegister(0);
    uint16_t set_point = memory_.readHoldingRegister(1);
    
    std::stringstream ss;
    
    // Move cursor to home (top-left) without flickering
    ss << "\033[H";

    // Header Frame
    ss << "\033[1;36m================================================================================\033[0m\n";
    ss << "\033[1;37m                 VIRTUAL PLC (vPLC) WATER TANK DIGITAL TWIN                     \033[0m\n";
    ss << "\033[1;36m================================================================================\033[0m\n";

    // Build the tank visualizer rows (from 1000 down to 0)
    // We have 10 rows for levels, plus header and footer
    std::string tank_rows[14];
    tank_rows[0]  = "     \033[1;33m[WATER TANK SIM]\033[0m";
    tank_rows[1]  = "      \033[1;37m+------------+\033[0m";
    
    for (int h = 9; h >= 0; --h) {
        int height_threshold_lower = h * 100;
        int height_threshold_upper = (h + 1) * 100;
        
        std::stringstream row_ss;
        row_ss << " \033[37m" << std::setw(4) << height_threshold_upper << "\033[0m \033[1;37m|\033[0m";
        
        if (water_level >= height_threshold_upper) {
            row_ss << "\033[96m████████████\033[0m"; // Water
        } else if (water_level > height_threshold_lower) {
            row_ss << "\033[1;96m~~~~~~~~~~~~\033[0m"; // Wave
        } else {
            row_ss << "            "; // Empty
        }
        
        row_ss << "\033[1;37m|\033[0m";
        
        // Dynamic labels next to rows
        if (h == 8) { // 850mm High Limit area
            row_ss << " \033[90m<-\033[0m ";
            if (high_limit) {
                row_ss << "\033[1;31m[HIGH LIMIT ON]\033[0m";
            } else {
                row_ss << "\033[90mHigh Limit (850)\033[0m";
            }
        } else if (h == 5) { // 500mm Set point area
            row_ss << " \033[90m<-\033[0m ";
            row_ss << "\033[1;32mSet Point (" << set_point << ")\033[0m";
        } else if (h == 1) { // 150mm Low Limit area
            row_ss << " \033[90m<-\033[0m ";
            if (low_limit) {
                row_ss << "\033[1;31m[LOW LIMIT ON]\033[0m";
            } else {
                row_ss << "\033[90mLow Limit (150)\033[0m";
            }
        }
        
        tank_rows[11 - h] = row_ss.str();
    }
    
    tank_rows[12] = "    \033[37m0\033[0m \033[1;37m+------------+\033[0m";
    
    std::stringstream level_ss;
    level_ss << "      \033[1;37mLevel: \033[1;36m" << water_level << " mm\033[0m";
    tank_rows[13] = level_ss.str();

    // Left Column elements
    std::string left_side[22];
    left_side[0]  = " \033[1;33m[SYSTEM STATUS]\033[0m";
    
    std::stringstream ss_run;
    ss_run << "  Runtime Status:     \033[1;32mACTIVE\033[0m";
    left_side[1]  = ss_run.str();
    
    std::stringstream ss_scan;
    ss_scan << "  Target Scan Rate:   \033[1;37m" << stats.target_cycle_ms << " ms\033[0m";
    left_side[2]  = ss_scan.str();
    
    std::stringstream ss_exec;
    ss_exec << "  Avg Execution Scan: \033[1;37m" << std::fixed << std::setprecision(3) << stats.avg_scan_time_ms << " ms\033[0m";
    left_side[3]  = ss_exec.str();
    
    std::stringstream ss_jit;
    ss_jit << "  Scan Timing Jitter: \033[1;37m" << stats.jitter_ms << " ms\033[0m";
    left_side[4]  = ss_jit.str();
    
    std::stringstream ss_tick;
    ss_tick << "  Total Scan Ticks:   \033[1;37m" << stats.total_ticks << "\033[0m";
    left_side[5]  = ss_tick.str();
    
    std::stringstream ss_mod;
    ss_mod << "  Modbus Server:      \033[1;37mPort " << server_.getPort() << "\033[0m (" << server_.getConnectedClientsCount() << " clients)";
    left_side[6] = ss_mod.str();
    
    std::stringstream ss_s7;
    ss_s7 << "  Siemens S7 Server:  \033[1;37mPort " << s7_server_.getPort() << "\033[0m (" << s7_server_.getClientsCount() << " clients)";
    left_side[7] = ss_s7.str();
    
    std::stringstream ss_mc;
    ss_mc << "  Mitsubishi MC:      \033[1;37mPort " << mc_server_.getPort() << "\033[0m (" << mc_server_.getClientsCount() << " clients)";
    left_side[8] = ss_mc.str();
    
    left_side[9]  = " \033[90m-----------------------------------------------\033[0m";
    
    left_side[10]  = " \033[1;33m[PLC REGISTER MAP (DESCRIPTIVE)]\033[0m";
    
    std::stringstream ss_ix00;
    ss_ix00 << "  %IX0.0 Auto Mode Switch  : " << (auto_mode ? "\033[1;32m[ AUTO ]\033[0m" : "\033[1;30m[MANUAL]\033[0m");
    left_side[11] = ss_ix00.str();
    
    std::stringstream ss_ix01;
    ss_ix01 << "  %IX0.1 Low Limit Sensor  : " << (low_limit ? "\033[1;31m[ ON   ]\033[0m" : "\033[1;30m[ OFF  ]\033[0m");
    left_side[12] = ss_ix01.str();
    
    std::stringstream ss_ix02;
    ss_ix02 << "  %IX0.2 High Limit Sensor : " << (high_limit ? "\033[1;31m[ ON   ]\033[0m" : "\033[1;30m[ OFF  ]\033[0m");
    left_side[13] = ss_ix02.str();
    
    std::stringstream ss_qx00;
    ss_qx00 << "  %QX0.0 Inlet Valve Act   : " << (inlet_valve ? "\033[1;32m[ OPEN ]\033[0m" : "\033[1;30m[CLOSED]\033[0m");
    left_side[14] = ss_qx00.str();
    
    std::stringstream ss_qx01;
    ss_qx01 << "  %QX0.1 Outlet Valve Act  : " << (outlet_valve ? "\033[1;32m[ OPEN ]\033[0m" : "\033[1;30m[CLOSED]\033[0m");
    left_side[15] = ss_qx01.str();
    
    std::stringstream ss_qx02;
    ss_qx02 << "  %QX0.2 System Run Lamp   : " << (run_lamp ? "\033[1;32m[ RUN  ]\033[0m" : "\033[1;30m[ STOP ]\033[0m");
    left_side[16] = ss_qx02.str();
    
    std::stringstream ss_iw0;
    ss_iw0 << "  %IW0   Current Level     : \033[1;36m" << std::setw(4) << water_level << " mm\033[0m";
    left_side[17] = ss_iw0.str();
    
    std::stringstream ss_mw0;
    ss_mw0 << "  %MW0   Pump Starts       : \033[1;35m" << std::setw(4) << pump_starts << "\033[0m";
    left_side[18] = ss_mw0.str();
    
    std::stringstream ss_mw1;
    ss_mw1 << "  %MW1   Target Set Point  : \033[1;35m" << std::setw(4) << set_point << " mm\033[0m";
    left_side[19] = ss_mw1.str();
    
    left_side[20] = "";
    left_side[21] = "";

    // Draw left and right columns side-by-side using Horizontal Absolute Cursor code \033[48G
    for (int i = 0; i < 22; ++i) {
        ss << left_side[i];
        if (i < 14) {
            ss << "\033[48G" << tank_rows[i];
        }
        ss << "\n";
    }

    // Footer Frame
    ss << "\033[1;36m================================================================================\033[0m\n";
    ss << " \033[1;33m[INTERACTIVE CONTROLS]\033[0m\n";
    ss << "  - \033[1;37m[I]\033[0m Toggle Auto Mode (%IX0.0)      - \033[1;37m[A]\033[0m Increase Setpoint (%MW1) [+50mm]\n";
    ss << "  - \033[1;31m[Q]\033[0m Graceful Shutdown and Exit       - \033[1;37m[Z]\033[0m Decrease Setpoint (%MW1) [-50mm]\n";
    ss << "\033[1;36m================================================================================\033[0m\n";

    std::cout << ss.str() << std::flush;
}

void PlcTui::enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &orig_termios_) == -1) return;
    struct termios raw = orig_termios_;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_cflag |= (CS8);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) return;
    raw_mode_enabled_ = true;
}

void PlcTui::disableRawMode() {
    if (raw_mode_enabled_) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios_);
        raw_mode_enabled_ = false;
    }
}

bool PlcTui::kbhit() {
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}
