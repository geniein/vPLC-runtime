#ifndef PLC_TUI_HPP
#define PLC_TUI_HPP

#include "core/PlcMemory.hpp"
#include "core/PlcScheduler.hpp"
#include "modbus/ModbusServer.hpp"
#include "s7/S7Server.hpp"
#include "mc/McServer.hpp"
#include "xgt/XgtServer.hpp"
#include <thread>
#include <atomic>
#include <termios.h>

class PlcTui {
public:
    PlcTui(PlcMemory& memory, PlcScheduler& scheduler, ModbusServer& server, S7Server& s7_server, McServer& mc_server, XgtServer& xgt_server);
    ~PlcTui();

    // Start/Stop the TUI rendering loop
    bool start();
    void stop();

    bool isRunning() const { return is_running_; }

private:
    // Core loops
    void renderLoop();
    void inputLoop();

    // Render screen buffers
    void drawScreen();
    void drawWaterTankScreen(const PlcScheduler::Stats& stats);
    void drawAssemblyScreen(const PlcScheduler::Stats& stats);

    // POSIX raw terminal configurations
    void enableRawMode();
    void disableRawMode();
    bool kbhit(); // Check if key was pressed

    PlcMemory& memory_;
    PlcScheduler& scheduler_;
    ModbusServer& server_;
    S7Server& s7_server_;
    McServer& mc_server_;
    XgtServer& xgt_server_;

    std::atomic<bool> is_running_;
    std::thread render_thread_;
    std::thread input_thread_;

    struct termios orig_termios_;
    bool raw_mode_enabled_;
};

#endif // PLC_TUI_HPP
