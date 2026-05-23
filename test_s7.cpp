#include <iostream>
#include <vector>
#include <cstdint>
#include <cassert>
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
    std::cout << "=== Starting Active vPLC Siemens S7 Protocol Test ===" << std::endl;

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
        std::cerr << "Make sure vPLC is running in another terminal!" << std::endl;
        Cli_Destroy(&client);
        return 1;
    }
    std::cout << "[Client] Connected to S7 Server at 127.0.0.1:1020" << std::endl;

    // --- TEST 1: Read DB1 (Data Block 1) registers 0 to 2 (first 6 bytes) ---
    // In our vPLC:
    // - DB1 maps to Holding Registers %MW0 to %MW1023
    // - DB1.DBB0/DB1.DBB1 maps to %MW0 (Counter, starts at 100)
    // - DB1.DBB2/DB1.DBB3 maps to %MW1 (Math Out = %IW0 * 2, starts at 300)
    // - DB1.DBB4/DB1.DBB5 maps to %MW2 (Unbound, starts at 0)
    uint8_t buffer[1024];
    res = Cli_DBRead(client, 1, 0, 6, buffer);
    if (res != 0) {
        char err_text[1024];
        Cli_ErrorText(res, err_text, sizeof(err_text));
        std::cerr << "[Test 1 Error] DBRead failed: " << err_text << std::endl;
        Cli_Disconnect(client);
        Cli_Destroy(&client);
        return 1;
    }
    printHex("\n[Test 1] DB1 Bytes Received (first 6 bytes)", buffer, 6);

    uint16_t val0 = (static_cast<uint16_t>(buffer[0]) << 8) | buffer[1];
    uint16_t val1 = (static_cast<uint16_t>(buffer[2]) << 8) | buffer[3];
    uint16_t val2 = (static_cast<uint16_t>(buffer[4]) << 8) | buffer[5];

    std::cout << "  -> DB1.DBW0 (%MW0 - Pump Starts) : " << val0 << " (Expected: >= 0)" << std::endl;
    std::cout << "  -> DB1.DBW2 (%MW1 - Set Point)   : " << val1 << " (Expected: 500)" << std::endl;
    std::cout << "  -> DB1.DBW4 (%MW2 - Unbound)     : " << val2 << " (Expected: 0)" << std::endl;

    assert(val0 >= 0);
    assert(val1 == 500);
    std::cout << "  [SUCCESS] Test 1 Passed." << std::endl;

    // --- TEST 2: Write DB1.DBW0 (%MW0) to 750 ---
    // 750 in big-endian is 0x02EE -> bytes [0x02, 0xEE]
    buffer[0] = 0x02;
    buffer[1] = 0xEE;
    printHex("\n[Test 2] Write DB1.DBW0 Request bytes", buffer, 2);
    res = Cli_DBWrite(client, 1, 0, 2, buffer);
    if (res != 0) {
        char err_text[1024];
        Cli_ErrorText(res, err_text, sizeof(err_text));
        std::cerr << "[Test 2 Error] DBWrite failed: " << err_text << std::endl;
        Cli_Disconnect(client);
        Cli_Destroy(&client);
        return 1;
    }
    std::cout << "  -> Written 750 to DB1.DBW0." << std::endl;
    std::cout << "  [SUCCESS] Test 2 Passed." << std::endl;

    // Wait briefly for synchronization
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // --- TEST 3: Read DB1.DBW0 (%MW0) again to verify ---
    // Expected: >= 750 (PLC scheduler should sync this value to memory, and continue counting from 750 if inputs are true)
    res = Cli_DBRead(client, 1, 0, 2, buffer);
    if (res != 0) {
        char err_text[1024];
        Cli_ErrorText(res, err_text, sizeof(err_text));
        std::cerr << "[Test 3 Error] DBRead failed: " << err_text << std::endl;
        Cli_Disconnect(client);
        Cli_Destroy(&client);
        return 1;
    }
    printHex("\n[Test 3] Read DB1.DBW0 Response bytes", buffer, 2);

    val0 = (static_cast<uint16_t>(buffer[0]) << 8) | buffer[1];
    std::cout << "  -> DB1.DBW0 (%MW0) read back: " << val0 << " (Expected: >= 750)" << std::endl;
    assert(val0 >= 750);
    std::cout << "  [SUCCESS] Test 3 Passed." << std::endl;

    // 4. Disconnect and Destroy S7 Client
    Cli_Disconnect(client);
    Cli_Destroy(&client);
    std::cout << "\n================ All Siemens S7 Tests Passed! ================" << std::endl;

    return 0;
}
