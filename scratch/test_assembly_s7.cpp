#include <iostream>
#include <vector>
#include <cstdint>
#include <thread>
#include <chrono>
#include <snap7.h>

void printHex(const std::string& label, const uint8_t* data, int len) {
    std::cout << label << ": ";
    for (int i = 0; i < len; ++i) {
        printf("%02X ", data[i]);
    }
    std::cout << std::endl;
}

int main() {
    std::cout << "=== Starting Automotive Assembly Line Siemens S7 Test ===" << std::endl;

    // 1. Create S7 Client
    S7Object client = Cli_Create();
    if (client == 0) {
        std::cerr << "[Error] Failed to create S7 client object." << std::endl;
        return 1;
    }

    // 2. Set Remote Port to 1020 (bypass port)
    uint16_t remote_port = 1020;
    int res = Cli_SetParam(client, p_u16_RemotePort, &remote_port);
    if (res != 0) {
        std::cerr << "[Error] Failed to set client remote port param: " << res << std::endl;
        Cli_Destroy(&client);
        return 1;
    }

    // 3. Connect to vPLC S7 Server (IP: 127.0.0.1, Rack: 0, Slot: 1)
    res = Cli_ConnectTo(client, "127.0.0.1", 0, 1);
    if (res != 0) {
        char err_text[1024];
        Cli_ErrorText(res, err_text, sizeof(err_text));
        std::cerr << "[Error] Failed to connect: " << err_text << " (Code: " << res << ")" << std::endl;
        std::cerr << "Make sure vPLC is running with --assembly first!" << std::endl;
        Cli_Destroy(&client);
        return 1;
    }
    std::cout << "[Client] Connected to S7 Server at 127.0.0.1:1020" << std::endl;

    // 4. Read DB1.DBW2 (%MW1 - Conveyor Speed)
    // - DB1.DBB2/DB1.DBB3 maps to %MW1 (Conveyor Speed, default 200)
    uint8_t buffer[1024];
    res = Cli_DBRead(client, 1, 0, 6, buffer);
    if (res != 0) {
        char err_text[1024];
        Cli_ErrorText(res, err_text, sizeof(err_text));
        std::cerr << "[Error] DBRead failed: " << err_text << std::endl;
        Cli_Disconnect(client);
        Cli_Destroy(&client);
        return 1;
    }

    uint16_t completed_cars = (static_cast<uint16_t>(buffer[0]) << 8) | buffer[1];
    uint16_t conveyor_speed = (static_cast<uint16_t>(buffer[2]) << 8) | buffer[3];
    std::cout << "[Read] Completed Cars (DB1.DBW0): " << completed_cars << std::endl;
    std::cout << "[Read] Conveyor Speed (DB1.DBW2): " << conveyor_speed << " mm/s (Default should be 200)" << std::endl;

    // 5. WRITE DB1.DBW2 (%MW1) to 550 mm/s (Speed up!)
    // 550 in big-endian is 0x0226 -> bytes [0x02, 0x26]
    buffer[0] = 0x02;
    buffer[1] = 0x26;
    std::cout << "\n[Action] Writing Conveyor Speed (DB1.DBW2) = 550 mm/s through S7comm..." << std::endl;
    res = Cli_DBWrite(client, 1, 2, 2, buffer);
    if (res != 0) {
        char err_text[1024];
        Cli_ErrorText(res, err_text, sizeof(err_text));
        std::cerr << "[Error] DBWrite failed: " << err_text << std::endl;
    } else {
        std::cout << "[Success] Written Speed = 550 mm/s. LOOK AT THE TUI DASHBOARD! The conveyor roller speed will accelerate!" << std::endl;
    }

    // 6. Monitor real-time completed cars
    std::cout << "\n[Action] Monitoring real-time completed cars on S7comm for 5 seconds..." << std::endl;
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        res = Cli_DBRead(client, 1, 0, 2, buffer);
        if (res == 0) {
            uint16_t comp = (static_cast<uint16_t>(buffer[0]) << 8) | buffer[1];
            std::cout << "  -> [" << (i * 0.5) << "s] Completed Cars: " << comp << std::endl;
        }
    }

    // 7. Disconnect and Destroy Client
    Cli_Disconnect(client);
    Cli_Destroy(&client);
    std::cout << "\n=== S7 Client Automotive Simulation Test Completed Gracefully! ===" << std::endl;

    return 0;
}
