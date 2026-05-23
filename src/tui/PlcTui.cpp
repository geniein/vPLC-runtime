#include "PlcTui.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <unistd.h>
#include <sys/select.h>
#include <iomanip>

PlcTui::PlcTui(PlcMemory& memory, PlcScheduler& scheduler, ModbusServer& server)
    : memory_(memory),
      scheduler_(scheduler),
      server_(server),
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
                    // Trigger global shutdown by sending a signal or raising should_quit
                    // Since should_quit is in main.cpp, we can just call exit(0) or stop the scheduler
                    // We can stop TUI and exit. For simplicity, we can call stop() from within or trigger exit
                    is_running_ = false;
                    extern std::atomic<bool> should_quit;
                    should_quit = true;
                }
                else if (c == 'i' || c == 'I') {
                    bool val = memory_.readDiscreteInput(0);
                    memory_.writeDiscreteInput(0, !val);
                }
                else if (c == 'o' || c == 'O') {
                    bool val = memory_.readDiscreteInput(1);
                    memory_.writeDiscreteInput(1, !val);
                }
                else if (c == 'a' || c == 'A') {
                    uint16_t val = memory_.readInputRegister(0);
                    memory_.writeInputRegister(0, val + 10);
                }
                else if (c == 'z' || c == 'Z') {
                    uint16_t val = memory_.readInputRegister(0);
                    if (val >= 10) {
                        memory_.writeInputRegister(0, val - 10);
                    } else {
                        memory_.writeInputRegister(0, 0);
                    }
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Poll input
    }
}

void PlcTui::drawScreen() {
    auto stats = scheduler_.getStats();
    
    std::stringstream ss;
    
    // Move cursor to home (top-left) without flickering
    ss << "\033[H";

    // Header Frame
    ss << "\033[1;36m========================================================================\033[0m\n";
    ss << "\033[1;37m                       VIRTUAL PLC (vPLC) DASHBOARD                     \033[0m\n";
    ss << "\033[1;36m========================================================================\033[0m\n";

    // System Status
    ss << " \033[1;33m[SYSTEM STATUS]\033[0m\n";
    ss << "  - Runtime Status:    \033[1;32mACTIVE\033[0m\n";
    ss << "  - Loaded Library:    \033[32m./libmock_logic.dylib\033[0m\n";
    ss << std::fixed << std::setprecision(4);
    ss << "  - Target Scan Rate:  \033[1;37m" << stats.target_cycle_ms << " ms\033[0m\n";
    ss << "  - Execution Scan:    \033[1;37m" << stats.avg_scan_time_ms << " ms\033[0m (Max: " << stats.max_scan_time_ms << " ms)\n";
    ss << "  - Timing Jitter:     \033[1;37m" << stats.jitter_ms << " ms\033[0m\n";
    ss << "  - Total Scan Ticks:  \033[1;37m" << stats.total_ticks << "\033[0m\n";
    ss << "  - Modbus TCP Server: \033[1;37mPort " << server_.getPort() << "\033[0m | Clients Connected: \033[1;32m" << server_.getConnectedClientsCount() << "\033[0m\n";
    ss << "\033[90m------------------------------------------------------------------------\033[0m\n";

    // Register maps
    ss << " \033[1;33m[PLC REGISTER MAP]\033[0m\n";
    ss << "\033[1;37m  DISCRETE INPUTS (%IX)               |  COILS / OUTPUTS (%QX)\033[0m\n";
    ss << "\033[90m  ------------------------------------+---------------------------------\033[0m\n";

    for (int i = 0; i < 8; ++i) {
        bool in_val = memory_.readDiscreteInput(i);
        bool out_val = memory_.readCoil(i);

        // Discrete Inputs column
        ss << "  [" << i << "] %IX0." << i << " : ";
        if (in_val) {
            ss << "\033[1;32m[ ON  ]\033[0m";
        } else {
            ss << "\033[90m[ OFF ]\033[0m";
        }
        
        // Spacer / Column divider
        ss << "                   |  ";

        // Coils / Outputs column
        ss << "[" << i << "] %QX0." << i << " : ";
        if (out_val) {
            ss << "\033[1;32m[ ON  ]\033[0m";
        } else {
            ss << "\033[90m[ OFF ]\033[0m";
        }
        ss << "\n";
    }

    ss << "\033[90m  ------------------------------------+---------------------------------\033[0m\n";
    ss << "\033[1;37m  INPUT REGISTERS (%IW)               |  HOLDING REGISTERS (%MW)\033[0m\n";
    ss << "\033[90m  ------------------------------------+---------------------------------\033[0m\n";

    for (int i = 0; i < 4; ++i) {
        uint16_t iw_val = memory_.readInputRegister(i);
        uint16_t mw_val = memory_.readHoldingRegister(i);

        ss << "  [" << i << "] %IW" << i << " : \033[1;35m" << std::setw(6) << iw_val << "\033[0m";
        ss << "                     |  ";
        ss << "[" << i << "] %MW" << i << " : \033[1;35m" << std::setw(6) << mw_val << "\033[0m\n";
    }

    ss << "\033[1;36m========================================================================\033[0m\n";
    ss << " \033[1;33m[INTERACTIVE CONTROLS]\033[0m\n";
    ss << "  - \033[1;37m[I]\033[0m Toggle %IX0.0 (Input 0)    - \033[1;37m[A]\033[0m Increment %IW0 (Analog Input)\n";
    ss << "  - \033[1;37m[O]\033[0m Toggle %IX0.1 (Input 1)    - \033[1;37m[Z]\033[0m Decrement %IW0 (Analog Input)\n";
    ss << "  - \033[1;31m[Q]\033[0m Graceful Shutdown and Exit\n";
    ss << "\033[1;36m========================================================================\033[0m\n";

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
